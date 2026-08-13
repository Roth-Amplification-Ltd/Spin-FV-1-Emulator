#!/usr/bin/env python3
import importlib.util
import pathlib
import sys

assembler_path = pathlib.Path(sys.argv[1])
program_dir = pathlib.Path(sys.argv[2])
out_dir = pathlib.Path(sys.argv[3])
out_dir.mkdir(parents=True, exist_ok=True)

spec = importlib.util.spec_from_file_location("fv1_assembler", assembler_path)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

programs = sorted(program_dir.glob("*.spn"))
assert len(programs) == 8, f"expected 8 example programs, got {len(programs)}"

for source in programs:
    target = out_dir / (source.stem + ".bin")
    count, max_delay = mod.assemble_file(source, target)
    data = target.read_bytes()
    assert len(data) == 512
    assert 0 < count <= 128
    assert 0 <= max_delay <= 32767
    print(f"{source.name}: {count}/128 instructions, delay max {max_delay}, 512 bytes")

print("all eight Steal This DSP example programs compile: PASS")
