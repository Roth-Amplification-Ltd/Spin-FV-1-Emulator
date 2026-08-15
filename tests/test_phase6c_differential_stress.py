#!/usr/bin/env python3
"""Bounded deterministic production/reference stress sweep for Phase 6C."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

SEEDS = [
    0x0000000000000001,
    0x4656315C2026,
    0x0123456789ABCDEF,
    0xDEADBEEFCAFEBABE,
    0xFFFFFFFFFFFFFFFF,
    0xA5A5A5A55A5A5A5A,
    0x3141592653589793,
    0x2718281828459045,
]

HASH_RE = re.compile(r"(?:architectural|delay-memory) hash:\s+([^\n]+)")


def run_one(cli: str, program: Path, seed: int) -> str:
    proc = subprocess.run(
        [cli, "conformance", str(program), "--samples", "64", "--seed", hex(seed)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if proc.returncode != 0 or "result:                 PASS" not in proc.stdout:
        raise RuntimeError(
            f"conformance failed for {program.name}, seed {seed:#x}\n"
            f"return={proc.returncode}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )
    if "instructions compared:" not in proc.stdout:
        raise RuntimeError(f"missing instruction coverage summary for {program.name}")
    hashes = HASH_RE.findall(proc.stdout)
    if len(hashes) < 2:
        raise RuntimeError(f"missing deterministic state hashes for {program.name}")
    return "\n".join(hashes)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: test_phase6c_differential_stress.py FV1_CLI PROGRAM_DIRECTORY", file=sys.stderr)
        return 2

    cli = str(Path(sys.argv[1]).resolve())
    directory = Path(sys.argv[2]).resolve()
    programs = sorted(directory.glob("*.spn"))
    if len(programs) != 8:
        raise RuntimeError(f"expected exactly 8 Steal This DSP programs, found {len(programs)}")

    comparisons = 0
    for program in programs:
        first_signature = None
        for seed in SEEDS:
            signature = run_one(cli, program, seed)
            comparisons += 1
            if seed == SEEDS[0]:
                first_signature = signature
        # Repeat one complete run to prove the visible end-state hashes are
        # deterministic for identical vectors, not merely cross-model equal.
        repeated = run_one(cli, program, SEEDS[0])
        comparisons += 1
        if repeated != first_signature:
            raise RuntimeError(f"nondeterministic conformance hashes for {program.name}")

    print(f"Phase 6C differential stress passed: {comparisons} deterministic program/seed runs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
