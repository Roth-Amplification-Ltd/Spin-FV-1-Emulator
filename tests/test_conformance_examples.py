#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

cli = Path(sys.argv[1])
program_dir = Path(sys.argv[2])
expected = [
    "00_55_gallon_saint.spn",
    "01_last_known_copy.spn",
    "02_ghost_spring.spn",
    "03_gravity_clerk.spn",
    "04_cold_case.spn",
    "05_municipal_lung.spn",
    "06_reverse_witness.spn",
    "07_data_felon.spn",
]

for name in expected:
    program = program_dir / name
    if not program.exists():
        raise SystemExit(f"missing demo program: {program}")
    proc = subprocess.run(
        [str(cli), "conformance", str(program), "--samples", "32", "--seed", "0x4656312026"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if proc.returncode != 0 or "result:                 PASS" not in proc.stdout:
        print(proc.stdout)
        raise SystemExit(f"conformance failed for {name}")
    print(f"{name}: PASS")

print("Steal This DSP Phase 5C conformance bank: PASS")
