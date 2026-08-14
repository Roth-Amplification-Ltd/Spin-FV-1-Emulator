#!/usr/bin/env python3
"""Phase-2 regression: host WAV rate must be independent of virtual FV-1 rate."""
import math
import struct
import subprocess
import sys
import wave
from pathlib import Path

cli = Path(sys.argv[1])
program = Path(sys.argv[2])
outdir = Path(sys.argv[3])
outdir.mkdir(parents=True, exist_ok=True)
input_wav = outdir / "input-48000.wav"
output_wav = outdir / "output-48000-vfv1-32768.wav"

rate = 48000
seconds = 2
with wave.open(str(input_wav), "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(rate)
    for i in range(rate * seconds):
        l = 0.18 * math.sin(2 * math.pi * 220 * i / rate)
        r = 0.16 * math.sin(2 * math.pi * 330 * i / rate + 0.2)
        w.writeframesraw(struct.pack("<hh", int(l * 32767), int(r * 32767)))

cp = subprocess.run([
    str(cli), "render", str(program), str(input_wav), str(output_wav),
    "--clock", "32768", "--pot0", "0.6", "--pot1", "0.5", "--pot2", "0.7"
], text=True, capture_output=True)
if cp.returncode:
    print(cp.stdout)
    print(cp.stderr, file=sys.stderr)
    raise SystemExit(cp.returncode)

if "Virtual FV-1 frames: 65536" not in cp.stdout:
    print(cp.stdout)
    raise SystemExit("expected exactly 65536 virtual samples for two seconds at 32768 Hz")

# fv1-cli intentionally writes IEEE float32 WAV (format tag 3), which the
# Python stdlib wave reader does not accept on every Python version. Verify
# the canonical RIFF header directly instead.
data = output_wav.read_bytes()
if data[:4] != b"RIFF" or data[8:12] != b"WAVE" or data[12:16] != b"fmt ":
    raise SystemExit("output is not a canonical RIFF/WAVE file")
format_tag, channels, out_rate = struct.unpack_from("<HHI", data, 20)
if format_tag != 3 or channels != 2:
    raise SystemExit("output is not stereo float32 WAV")
if out_rate != rate:
    raise SystemExit("output host sample rate changed")
data_pos = data.find(b"data")
if data_pos < 0:
    raise SystemExit("output WAV lacks data chunk")
data_size = struct.unpack_from("<I", data, data_pos + 4)[0]
if data_size != rate * seconds * 2 * 4:
    raise SystemExit("output duration/frame count changed")

print("Phase-2 48k host / 32.768k virtual-clock render regression passed.")
