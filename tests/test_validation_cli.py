#!/usr/bin/env python3
import json
import pathlib
import subprocess
import sys

cli = pathlib.Path(sys.argv[1])
out = pathlib.Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
stim = out / "stimulus.wav"
prefix = out / "identical"

subprocess.run([
    str(cli), "stimulus", str(stim), "--kind", "multitone", "--seconds", "0.5",
    "--host-rate", "48000", "--level", "0.3"
], check=True)
subprocess.run([
    str(cli), "validate", str(stim), str(stim), "--report-prefix", str(prefix),
    "--min-corr", "0.999999", "--max-residual-rms-dbfs", "-120",
    "--max-residual-peak-dbfs", "-120"
], check=True)

report = json.loads((out / "identical.json").read_text())
assert report["passed"] is True
assert report["capture_delay_frames"] == 0
assert report["left"]["correlation"] > 0.999999
assert report["left"]["residual_rms_dbfs"] <= -190
for suffix in [".md", "-frequency.csv", "-residual.wav"]:
    assert pathlib.Path(str(prefix) + suffix).exists(), suffix
print("Phase 5 validation CLI regression passed")
