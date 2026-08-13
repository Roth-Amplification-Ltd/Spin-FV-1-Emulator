#!/usr/bin/env python3
import math
import pathlib
import struct
import subprocess
import sys
import wave

cli = pathlib.Path(sys.argv[1])
program_dir = pathlib.Path(sys.argv[2])
work = pathlib.Path(sys.argv[3])
work.mkdir(parents=True, exist_ok=True)

rate = 32768
input_wav = work / "input.wav"
with wave.open(str(input_wav), "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(rate)
    frames = rate * 2
    buf = bytearray()
    for i in range(frames):
        l = 0.22 * math.sin(2 * math.pi * 220 * i / rate) + 0.10 * math.sin(2 * math.pi * 660 * i / rate)
        r = 0.20 * math.sin(2 * math.pi * 220 * i / rate + 0.3)
        buf += struct.pack("<hh", int(l * 32767), int(r * 32767))
    w.writeframes(buf)

programs = sorted(program_dir.glob("*.spn"))
assert len(programs) == 8

for program in programs:
    output = work / f"{program.stem}.wav"
    subprocess.run([
        str(cli), "render", str(program), str(input_wav), str(output),
        "--pot0", "0.63", "--pot1", "0.47", "--pot2", "0.71"
    ], check=True, stdout=subprocess.DEVNULL)
    b = output.read_bytes()
    data_pos = b.find(b"data")
    assert data_pos >= 0
    n = struct.unpack_from("<I", b, data_pos + 4)[0]
    vals = struct.unpack_from("<" + "f" * (n // 4), b, data_pos + 8)
    assert vals
    assert all(math.isfinite(x) for x in vals), f"non-finite output: {program.name}"
    peak = max(abs(x) for x in vals)
    rms = math.sqrt(sum(x*x for x in vals) / len(vals))
    assert peak > 1e-5 and rms > 1e-6, f"silent output: {program.name}"
    assert peak <= 1.000001, f"output escaped normalized range: {program.name}: {peak}"
    print(f"{program.name}: peak={peak:.6f} rms={rms:.6f}")

print("all eight example programs execute/render: PASS")
