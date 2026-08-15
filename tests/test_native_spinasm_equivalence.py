#!/usr/bin/env python3
"""Require the native compiler to reproduce the historical Python oracle."""
import pathlib
import subprocess
import sys

if len(sys.argv) != 6:
    raise SystemExit("usage: test_native_spinasm_equivalence.py <fv1-cli> <python-assembler> <program-dir> <workdir>")

cli = pathlib.Path(sys.argv[1])
assembler = pathlib.Path(sys.argv[2])
program_dir = pathlib.Path(sys.argv[3])
work = pathlib.Path(sys.argv[4])
# Compatibility with an older five-argument CTest invocation is deliberately
# avoided; CMake passes a final marker so accidental argument drift is caught.
marker = sys.argv[5]
if marker != "phase6a":
    raise SystemExit(f"unexpected test marker: {marker}")
work.mkdir(parents=True, exist_ok=True)

programs = sorted(program_dir.glob("*.spn"))
if len(programs) != 8:
    raise RuntimeError(f"expected eight demo programs, found {len(programs)}")

for source in programs:
    native = work / f"{source.stem}.native.bin"
    oracle = work / f"{source.stem}.python.bin"
    subprocess.run([str(cli), "assemble", str(source), str(native)], check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    subprocess.run([sys.executable, str(assembler), str(source), str(oracle)], check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    nb = native.read_bytes()
    ob = oracle.read_bytes()
    if nb != ob:
        for index, (a, b) in enumerate(zip(nb, ob)):
            if a != b:
                raise RuntimeError(f"{source.name}: native/Python mismatch at byte {index}: {a:02x}!={b:02x}")
        raise RuntimeError(f"{source.name}: native/Python image lengths differ")

print("native SpinASM compiler is byte-identical to Python oracle for all eight demo programs")
