#!/usr/bin/env python3
"""Phase 6C malformed-input regression tests for command-line file boundaries."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def run_expect_failure(argv: list[str], label: str) -> None:
    proc = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if proc.returncode == 0:
        raise RuntimeError(f"{label}: unexpectedly succeeded\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}")
    # POSIX return codes below zero indicate signal termination. The CLI should
    # turn malformed external input into a normal diagnostic exit instead.
    if proc.returncode < 0:
        raise RuntimeError(f"{label}: process terminated by signal ({proc.returncode})")
    combined = (proc.stdout + proc.stderr).lower()
    if not any(word in combined for word in ("error", "invalid", "must", "cannot", "checksum", "compile")):
        raise RuntimeError(f"{label}: failure had no useful diagnostic\n{proc.stdout}\n{proc.stderr}")


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: test_malformed_inputs.py FV1_CLI VALID_PROGRAM WORKDIR", file=sys.stderr)
        return 2

    cli = str(Path(sys.argv[1]).resolve())
    valid_program = str(Path(sys.argv[2]).resolve())
    root = Path(sys.argv[3]).resolve()
    root.mkdir(parents=True, exist_ok=True)

    short_bin = root / "short.bin"
    short_bin.write_bytes(bytes(511))
    run_expect_failure([cli, "inspect", str(short_bin)], "511-byte program")

    long_bin = root / "long.bin"
    long_bin.write_bytes(bytes(513))
    run_expect_failure([cli, "inspect", str(long_bin)], "513-byte program")

    bad_hex = root / "bad.hex"
    bad_hex.write_text(":040000000102030400\n:00000001FF\n", encoding="ascii")
    run_expect_failure([cli, "inspect", str(bad_hex)], "bad Intel HEX checksum")

    bad_spn = root / "bad.spn"
    bad_spn.write_text("THIS_IS_NOT_AN_OPCODE REG0, 1.0\n", encoding="utf-8")
    run_expect_failure([cli, "assemble", str(bad_spn), str(root / "bad.bin")], "invalid SpinASM")

    truncated_wav = root / "truncated.wav"
    truncated_wav.write_bytes(b"RIFF\x10\x00\x00\x00WAVEfmt ")
    run_expect_failure([cli, "render", valid_program, str(truncated_wav), str(root / "out.wav")],
                       "truncated WAV")

    garbage_wav = root / "garbage.wav"
    garbage_wav.write_bytes(b"not-a-wave-file" * 7)
    run_expect_failure([cli, "render", valid_program, str(garbage_wav), str(root / "out2.wav")],
                       "garbage WAV")

    print("Phase 6C malformed-input suite passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
