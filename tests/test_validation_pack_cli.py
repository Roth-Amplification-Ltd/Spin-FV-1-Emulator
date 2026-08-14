#!/usr/bin/env python3
"""Phase-5B end-to-end hardware-validation pack CLI regression."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_validation_pack_cli.py <fv1-cli> <workdir>")
    cli = Path(sys.argv[1])
    work = Path(sys.argv[2])
    pack = work / "pack"
    work.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        str(cli), "validation-pack", str(pack),
        "--host-rate", "16000", "--seconds", "0.05", "--level", "0.2", "--seed", "99",
    ], check=True)
    expected = [
        "01-impulse.wav", "02-multitone.wav", "03-log-sweep.wav",
        "04-sine-1khz.wav", "05-white-noise.wav", "06-pink-noise.wav",
        "manifest.json", "README.txt",
    ]
    for name in expected:
        path = pack / name
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f"missing/empty validation-pack file: {path}")
    manifest = json.loads((pack / "manifest.json").read_text())
    if manifest.get("schema") != "spin-fv1-hardware-validation-pack-1":
        raise RuntimeError("unexpected validation-pack schema")
    if manifest.get("sample_rate") != 16000 or len(manifest.get("stimuli", [])) != 6:
        raise RuntimeError("unexpected validation-pack manifest content")
    print("phase5b validation-pack CLI regression passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
