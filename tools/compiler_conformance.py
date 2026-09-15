#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""
FV-1 Lab compiler differential-conformance harness.

Acceptance criterion:
  FV-1 Lab native C++ compiler output MUST be byte-for-byte identical to
  output produced by the official Spin Semiconductor SpinAsm compiler.

The historical Python assembler is diagnostic only. It may help explain a
mismatch, but it can never cause this harness to PASS.

The official SpinAsm step is intentionally external: this harness does not
redistribute SpinAsm. `prepare-official` packs a corpus into batches of up to
eight sources, matching SpinAsm's eight EEPROM program slots. After the user
builds each batch in official SpinAsm and drops the resulting .hex files into
the prepared output directory, `run` performs the authoritative comparison.
If official output is missing, the harness FAILS.
"""
from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from typing import Iterable

PROGRAM_BYTES = 512
PROGRAM_WORDS = 128
BANK_SLOTS = 8
BANK_BYTES = PROGRAM_BYTES * BANK_SLOTS


@dataclasses.dataclass(frozen=True)
class Case:
    source: Path
    rel: str
    stem_key: str


@dataclasses.dataclass
class CompareResult:
    lhs_name: str
    rhs_name: str
    equal: bool
    detail: str
    mismatch_count: int = 0


def die(message: str, code: int = 2) -> "NoReturn":
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(code)


def run_checked(argv: list[str], *, cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(
        argv,
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if proc.returncode != 0:
        cmd = " ".join(argv)
        raise RuntimeError(
            f"command failed ({proc.returncode}): {cmd}\n"
            f"--- stdout ---\n{proc.stdout}\n"
            f"--- stderr ---\n{proc.stderr}"
        )
    return proc


def discover_cases(repo: Path, explicit: list[str]) -> list[Case]:
    if explicit:
        sources = []
        for raw in explicit:
            p = Path(raw)
            if not p.is_absolute():
                p = repo / p
            if not p.is_file():
                die(f"source not found: {p}")
            sources.append(p.resolve())
    else:
        examples = repo / "examples"
        if not examples.is_dir():
            die(f"examples directory not found: {examples}")
        sources = sorted(
            p.resolve()
            for p in examples.rglob("*.spn")
            if "compiled" not in p.parts
        )

    cases: list[Case] = []
    for source in sources:
        try:
            rel = source.relative_to(repo).as_posix()
        except ValueError:
            rel = source.name
        stem_key = rel[:-4] if rel.lower().endswith(".spn") else rel
        cases.append(Case(source=source, rel=rel, stem_key=stem_key))
    if not cases:
        die("no .spn compiler-conformance cases found")
    return cases


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validate_program(data: bytes, label: str) -> None:
    if len(data) != PROGRAM_BYTES:
        raise ValueError(
            f"{label}: expected {PROGRAM_BYTES} bytes / {PROGRAM_WORDS} words, "
            f"got {len(data)} bytes"
        )


def compile_native(cli: Path, source: Path, output: Path) -> bytes:
    output.parent.mkdir(parents=True, exist_ok=True)
    run_checked([str(cli), "assemble", str(source), str(output)])
    data = output.read_bytes()
    validate_program(data, f"native output for {source.name}")
    return data


def compile_python(assembler: Path, source: Path, output: Path) -> bytes:
    output.parent.mkdir(parents=True, exist_ok=True)
    run_checked([sys.executable, str(assembler), str(source), str(output)])
    data = output.read_bytes()
    validate_program(data, f"Python output for {source.name}")
    return data


def intel_hex_to_bytes(path: Path) -> tuple[int, bytes]:
    """
    Parse Intel HEX with checksum validation.

    Returns (lowest_data_address, contiguous_data). Supports data records,
    extended-segment and extended-linear address records. Start-address records
    are ignored because they carry execution metadata, not EEPROM payload.
    """
    memory: dict[int, int] = {}
    upper_linear = 0
    upper_segment = 0
    eof_seen = False

    for line_no, raw in enumerate(path.read_text(encoding="ascii").splitlines(), start=1):
        line = raw.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise ValueError(f"{path}:{line_no}: not an Intel HEX record")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as exc:
            raise ValueError(f"{path}:{line_no}: invalid hexadecimal data") from exc
        if len(record) < 5:
            raise ValueError(f"{path}:{line_no}: record too short")

        count = record[0]
        if len(record) != count + 5:
            raise ValueError(
                f"{path}:{line_no}: byte count says {count}, "
                f"record contains {len(record) - 5}"
            )
        if sum(record) & 0xFF:
            raise ValueError(f"{path}:{line_no}: Intel HEX checksum mismatch")

        address = (record[1] << 8) | record[2]
        rectype = record[3]
        payload = record[4 : 4 + count]

        if rectype == 0x00:
            base = upper_linear + upper_segment + address
            for i, byte in enumerate(payload):
                a = base + i
                if a in memory and memory[a] != byte:
                    raise ValueError(f"{path}:{line_no}: overlapping conflicting data at 0x{a:X}")
                memory[a] = byte
        elif rectype == 0x01:
            eof_seen = True
            break
        elif rectype == 0x02:
            if count != 2:
                raise ValueError(f"{path}:{line_no}: malformed extended segment record")
            upper_segment = int.from_bytes(payload, "big") << 4
            upper_linear = 0
        elif rectype == 0x04:
            if count != 2:
                raise ValueError(f"{path}:{line_no}: malformed extended linear record")
            upper_linear = int.from_bytes(payload, "big") << 16
            upper_segment = 0
        elif rectype in (0x03, 0x05):
            # Start segment / start linear address. Irrelevant to EEPROM image.
            continue
        else:
            raise ValueError(f"{path}:{line_no}: unsupported Intel HEX record type 0x{rectype:02X}")

    if not memory:
        raise ValueError(f"{path}: no Intel HEX data records found")
    if not eof_seen:
        raise ValueError(f"{path}: missing Intel HEX EOF record")

    lo = min(memory)
    hi = max(memory)
    missing = [a for a in range(lo, hi + 1) if a not in memory]
    if missing:
        first = missing[0]
        raise ValueError(
            f"{path}: data is not contiguous; first missing address is 0x{first:X}"
        )
    return lo, bytes(memory[a] for a in range(lo, hi + 1))


def normalize_official_bank(path: Path) -> tuple[int, bytes]:
    base, raw = intel_hex_to_bytes(path)

    if len(raw) == BANK_BYTES:
        return base, raw

    # Some tools can wrap a larger EEPROM image around the 4 KiB FV-1 bank.
    # If the file is larger, require an exact 4096-byte leading bank rather
    # than silently guessing an interior offset.
    if len(raw) > BANK_BYTES:
        return base, raw[:BANK_BYTES]

    raise ValueError(
        f"{path}: official HEX contains {len(raw)} contiguous data bytes; "
        f"expected at least {BANK_BYTES} for eight FV-1 slots"
    )


def words(data: bytes) -> list[int]:
    validate_program(data, "program")
    return [int.from_bytes(data[i:i+4], "big") for i in range(0, len(data), 4)]


def diagnose_compare(lhs: bytes, rhs: bytes, lhs_name: str, rhs_name: str) -> CompareResult:
    if lhs == rhs:
        return CompareResult(lhs_name, rhs_name, True, "byte-identical", 0)

    validate_program(lhs, lhs_name)
    validate_program(rhs, rhs_name)

    mismatching_bytes = [i for i, (a, b) in enumerate(zip(lhs, rhs)) if a != b]
    lw = words(lhs)
    rw = words(rhs)
    mismatching_words = [i for i, (a, b) in enumerate(zip(lw, rw)) if a != b]

    reversed_per_word = b"".join(
        rhs[i:i+4][::-1] for i in range(0, PROGRAM_BYTES, 4)
    )
    swapped_halfwords = b"".join(
        rhs[i+2:i+4] + rhs[i:i+2] for i in range(0, PROGRAM_BYTES, 4)
    )

    hints: list[str] = []
    if lhs == reversed_per_word:
        hints.append("outputs match if every 32-bit instruction word is byte-reversed")
    if lhs == swapped_halfwords:
        hints.append("outputs match if 16-bit halfwords are swapped inside each 32-bit word")

    samples = []
    for wi in mismatching_words[:8]:
        samples.append(
            f"word[{wi:03d}] {lhs_name}=0x{lw[wi]:08X} "
            f"{rhs_name}=0x{rw[wi]:08X}"
        )

    detail = (
        f"{len(mismatching_bytes)} byte(s), {len(mismatching_words)} word(s) differ"
    )
    if hints:
        detail += "; " + "; ".join(hints)
    if samples:
        detail += "; first differences: " + " | ".join(samples)

    return CompareResult(
        lhs_name=lhs_name,
        rhs_name=rhs_name,
        equal=False,
        detail=detail,
        mismatch_count=len(mismatching_words),
    )


def batch_cases(cases: list[Case]) -> list[list[Case]]:
    return [cases[i:i+BANK_SLOTS] for i in range(0, len(cases), BANK_SLOTS)]


def prepare_official(repo: Path, work: Path, cases: list[Case]) -> None:
    prep = work / "official-input"
    out = work / "official-output"
    if prep.exists():
        shutil.rmtree(prep)
    prep.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    batches = batch_cases(cases)
    all_manifest = {
        "format": 1,
        "program_bytes": PROGRAM_BYTES,
        "bank_slots": BANK_SLOTS,
        "batches": [],
    }

    for batch_index, batch in enumerate(batches):
        name = f"batch-{batch_index:03d}"
        bd = prep / name
        bd.mkdir(parents=True)
        slots = []

        for slot, case in enumerate(batch):
            safe = Path(case.rel).name
            dst_name = f"slot{slot}_{safe}"
            shutil.copy2(case.source, bd / dst_name)
            slots.append(
                {
                    "slot": slot,
                    "source": case.rel,
                    "prepared_file": dst_name,
                }
            )

        manifest = {
            "batch": name,
            "official_hex": f"{name}.hex",
            "slots": slots,
        }
        (bd / "slots.json").write_text(json.dumps(manifest, indent=2) + "\n")
        all_manifest["batches"].append(manifest)

        instructions = [
            f"FV-1 compiler conformance: {name}",
            "",
            "Official SpinAsm procedure:",
            "1. Open SpinAsm and create/open an 8-program project.",
            "2. Put the following prepared source files into the matching slots:",
        ]
        for item in slots:
            instructions.append(
                f"   slot {item['slot']}: {item['prepared_file']}  "
                f"(repo source: {item['source']})"
            )
        instructions += [
            "3. Leave unused slots alone; the harness ignores them.",
            "4. Enable Intel Hex output.",
            "5. Build the project.",
            f"6. Save/copy the resulting HEX file as: ../../official-output/{name}.hex",
            "",
            "Do not edit source between the FV-1 Lab and SpinAsm runs.",
        ]
        (bd / "README-OFFICIAL-SPINASM.txt").write_text("\n".join(instructions) + "\n")

    (prep / "manifest.json").write_text(json.dumps(all_manifest, indent=2) + "\n")
    print(f"Prepared {len(cases)} source case(s) in {len(batches)} SpinAsm batch(es).")
    print(f"Official input:  {prep}")
    print(f"Official output: {out}")
    print()
    print("Build each batch in official SpinAsm and place its Intel HEX file in")
    print("official-output/ with the batch filename shown in each README.")


def load_official_slots(work: Path, cases: list[Case]) -> tuple[dict[str, bytes], list[str]]:
    out = work / "official-output"
    official: dict[str, bytes] = {}
    notes: list[str] = []

    for batch_index, batch in enumerate(batch_cases(cases)):
        name = f"batch-{batch_index:03d}"
        hex_path = out / f"{name}.hex"
        if not hex_path.is_file():
            notes.append(f"{name}: official HEX missing ({hex_path})")
            continue

        base, bank = normalize_official_bank(hex_path)
        notes.append(
            f"{name}: loaded official HEX, base=0x{base:X}, bytes={len(bank)}"
        )
        for slot, case in enumerate(batch):
            start = slot * PROGRAM_BYTES
            official[case.stem_key] = bank[start:start+PROGRAM_BYTES]
    return official, notes


def render_markdown(
    repo: Path,
    cases: list[Case],
    rows: list[dict],
    notes: list[str],
    official_required: bool,
) -> str:
    total = len(rows)
    native_python_ok = sum(r["native_python"] == "PASS" for r in rows)
    official_compared = sum(r["official_status"] not in ("PENDING", "N/A") for r in rows)
    official_ok = sum(r["official_status"] == "PASS" for r in rows)
    overall_fail = any(r["status"] == "FAIL" for r in rows)
    pending = any(r["official_status"] == "PENDING" for r in rows)

    if overall_fail:
        verdict = "FAIL"
    elif pending or official_compared != total:
        verdict = "FAIL - official SpinAsm output incomplete"
    elif official_ok == total:
        verdict = "PASS - FV-1 Lab native output is byte-identical to official SpinAsm"
    else:
        verdict = "FAIL"

    lines = [
        "# FV-1 Compiler Conformance Report",
        "",
        f"**Verdict:** {verdict}",
        "",
        f"- Corpus: {total} program(s)",
        f"- AUTHORITATIVE: native C++ vs official SpinAsm: {official_ok}/{total} byte-identical",
        f"- Official SpinAsm outputs present: {official_compared}/{total}",
        f"- Diagnostic only: native C++ vs Python oracle: {native_python_ok}/{total} byte-identical",
        "",
        "## Results",
        "",
        "| Source | Native/Python | Official/native | Native SHA-256 |",
        "|---|---:|---:|---|",
    ]
    for r in rows:
        lines.append(
            f"| `{r['source']}` | {r['native_python']} | "
            f"{r['official_status']} | `{r['native_sha256'][:16]}…` |"
        )

    problems = [r for r in rows if r["details"]]
    if problems:
        lines += ["", "## Differences / notes", ""]
        for r in problems:
            lines.append(f"### `{r['source']}`")
            lines.append("")
            for detail in r["details"]:
                lines.append(f"- {detail}")
            lines.append("")

    if notes:
        lines += ["", "## Official SpinAsm ingestion", ""]
        lines.extend(f"- {n}" for n in notes)

    lines += [
        "",
        "## Equality contract",
        "",
        "The ONLY PASS condition is that every FV-1 Lab native 512-byte program",
        "is byte-for-byte identical to the corresponding EEPROM slot emitted by",
        "the official Spin Semiconductor SpinAsm compiler. The Python assembler",
        "is diagnostic only and has no authority over the final verdict.",
        "",
    ]
    return "\n".join(lines)


def command_run(args: argparse.Namespace) -> int:
    repo = args.repo.resolve()
    work = args.work.resolve()
    cases = discover_cases(repo, args.source)

    native_cli = args.native.resolve()
    python_assembler = args.python_assembler.resolve()

    if not native_cli.is_file():
        die(f"native compiler CLI not found: {native_cli}")
    if not os.access(native_cli, os.X_OK):
        die(f"native compiler CLI is not executable: {native_cli}")
    if not python_assembler.is_file():
        die(f"Python assembler not found: {python_assembler}")

    work.mkdir(parents=True, exist_ok=True)
    generated = work / "generated"
    generated.mkdir(parents=True, exist_ok=True)

    official, official_notes = load_official_slots(work, cases)
    rows = []
    any_fail = False
    any_pending = False

    print(f"Corpus: {len(cases)} program(s)")
    print(f"Native: {native_cli}")
    print(f"Python: {python_assembler}")
    print()

    for index, case in enumerate(cases, start=1):
        safe_key = case.stem_key.replace("/", "__")
        nd = generated / "native" / f"{safe_key}.bin"
        pd = generated / "python" / f"{safe_key}.bin"
        details: list[str] = []
        status = "PASS"

        print(f"[{index:02d}/{len(cases):02d}] {case.rel}")
        try:
            native = compile_native(native_cli, case.source, nd)
            pyout = compile_python(python_assembler, case.source, pd)
        except Exception as exc:
            print(f"  COMPILE FAIL: {exc}")
            rows.append(
                {
                    "source": case.rel,
                    "status": "FAIL",
                    "native_python": "FAIL",
                    "official_status": "N/A",
                    "native_sha256": "",
                    "details": [str(exc)],
                }
            )
            any_fail = True
            continue

        np = diagnose_compare(native, pyout, "native", "python")
        if np.equal:
            np_status = "PASS"
            print("  native vs python: PASS")
        else:
            np_status = "FAIL"
            status = "FAIL"
            any_fail = True
            details.append(f"native vs Python: {np.detail}")
            print(f"  native vs python: FAIL - {np.detail}")

        official_bytes = official.get(case.stem_key)
        if official_bytes is None:
            official_status = "PENDING"
            any_pending = True
            print("  official SpinAsm:  PENDING")
        else:
            try:
                validate_program(official_bytes, f"official output for {case.rel}")
                no = diagnose_compare(native, official_bytes, "native", "official")
                po = diagnose_compare(pyout, official_bytes, "python", "official")
                if no.equal and po.equal:
                    official_status = "PASS"
                    print("  official SpinAsm:  PASS")
                else:
                    official_status = "FAIL"
                    status = "FAIL"
                    any_fail = True
                    if not no.equal:
                        details.append(f"native vs official: {no.detail}")
                    if not po.equal:
                        details.append(f"Python vs official: {po.detail}")
                    print(f"  official SpinAsm:  FAIL - {no.detail}")
            except Exception as exc:
                official_status = "FAIL"
                status = "FAIL"
                any_fail = True
                details.append(f"official output error: {exc}")
                print(f"  official SpinAsm:  FAIL - {exc}")

        rows.append(
            {
                "source": case.rel,
                "status": status,
                "native_python": np_status,
                "official_status": official_status,
                "native_sha256": sha256(native),
                "details": details,
            }
        )

    report_md = render_markdown(
        repo, cases, rows, official_notes, args.require_official
    )
    report_path = work / "report.md"
    json_path = work / "report.json"
    report_path.write_text(report_md + "\n")
    json_path.write_text(
        json.dumps(
            {
                "format": 1,
                "repo": str(repo),
                "program_bytes": PROGRAM_BYTES,
                "rows": rows,
                "official_notes": official_notes,
            },
            indent=2,
        )
        + "\n"
    )

    print()
    print(f"Markdown report: {report_path}")
    print(f"JSON report:     {json_path}")

    if any_fail:
        print("Compiler conformance: FAIL")
        return 1
    if any_pending:
        print("Compiler conformance: FAIL - official SpinAsm output is incomplete")
        return 3

    print("Compiler conformance: PASS - FV-1 Lab native output is byte-identical to official SpinAsm")
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Differential FV-1 compiler conformance harness"
    )
    sub = p.add_subparsers(dest="command", required=True)

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument(
        "--repo",
        type=Path,
        default=Path.cwd(),
        help="Spin-FV-1-Emulator repository root (default: cwd)",
    )
    common.add_argument(
        "--work",
        type=Path,
        default=Path("build/compiler-conformance"),
        help="harness workspace (default: build/compiler-conformance)",
    )
    common.add_argument(
        "--source",
        action="append",
        default=[],
        help="explicit .spn source; repeat to override automatic examples discovery",
    )

    prep = sub.add_parser(
        "prepare-official",
        parents=[common],
        help="prepare up-to-8-slot batches for official SpinAsm",
    )

    run = sub.add_parser(
        "run",
        parents=[common],
        help="authoritative native-vs-official SpinAsm comparison",
    )
    run.add_argument(
        "--native",
        type=Path,
        default=Path("build/fv1-cli"),
        help="path to native fv1-cli",
    )
    run.add_argument(
        "--python-assembler",
        type=Path,
        default=Path("tools/fv1_assembler.py"),
        help="path to Python assembler oracle",
    )

    check = sub.add_parser(
        "check",
        parents=[common],
        help="strict three-way check; official SpinAsm output is required",
    )
    check.add_argument(
        "--native",
        type=Path,
        default=Path("build/fv1-cli"),
        help="path to native fv1-cli",
    )
    check.add_argument(
        "--python-assembler",
        type=Path,
        default=Path("tools/fv1_assembler.py"),
        help="path to Python assembler oracle",
    )
    return p


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    # Resolve paths relative to repo instead of the caller's current directory.
    args.repo = args.repo.resolve()
    if not args.work.is_absolute():
        args.work = args.repo / args.work

    if args.command == "prepare-official":
        cases = discover_cases(args.repo, args.source)
        prepare_official(args.repo, args.work, cases)
        return 0

    if not args.native.is_absolute():
        args.native = args.repo / args.native
    if not args.python_assembler.is_absolute():
        args.python_assembler = args.repo / args.python_assembler
    args.require_official = True
    return command_run(args)


if __name__ == "__main__":
    raise SystemExit(main())
