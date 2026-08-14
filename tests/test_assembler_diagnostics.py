#!/usr/bin/env python3
"""Ensure GUI-facing SpinASM diagnostics stay concise and line-numbered."""
from __future__ import annotations
import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_assembler_diagnostics.py <assembler.py> <workdir>")
    assembler = Path(sys.argv[1])
    work = Path(sys.argv[2])
    work.mkdir(parents=True, exist_ok=True)
    source = work / "invalid.spn"
    output = work / "invalid.bin"
    source.write_text("RDAX ADCL, 1.0\nNOPE REG0, 1.0\n")
    proc = subprocess.run([sys.executable, str(assembler), str(source), str(output)],
                          text=True, capture_output=True)
    if proc.returncode == 0:
        raise RuntimeError("invalid SpinASM unexpectedly compiled")
    if "Line 2" not in proc.stderr or "unsupported mnemonic NOPE" not in proc.stderr:
        raise RuntimeError(f"missing concise line diagnostic: {proc.stderr!r}")
    if "Traceback" in proc.stderr:
        raise RuntimeError("assembler leaked Python traceback into GUI-facing diagnostic")
    print("SpinASM concise-diagnostic regression passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
