# Phase 5C — FV-1 Model Hardening and Differential Conformance

## Goal

Phase 5C strengthens the emulator engine without changing the finished Qt testbench. It implements
the research-capsule methodology of explicit state/time, separate reference and production models,
differential testing, boundary testing, deterministic reproducibility and fuzzing.

Physical FV-1 validation is deliberately **not** claimed here. Hardware-independent hardening can be
completed now; silicon-only questions remain tagged for later bench work.

## New architecture

```text
                    +----------------------+
program + vectors ->| fv1-reference        |  Model A: simple/auditable
                    +----------+-----------+
                               |
                               | instruction/state differential comparison
                               v
                    +----------+-----------+
program + vectors ->| fv1-core             |  Model B: production emulator
                    +----------+-----------+
                               |
                               | future capture/conformance
                               v
                    +----------+-----------+
                    | physical Spin FV-1   |  Model C: deferred
                    +----------------------+
```

`fv1-reference` intentionally does not link `fv1-core`, and it owns a separate decoder/arithmetic/
execution implementation. This catches implementation divergence. Shared agreement is still not a
substitute for physical evidence where the public specification is ambiguous.

## Production-engine hardening

Phase 5C adds explicit virtual-time coordinates to snapshots and traces:

- completed `sample_counter`;
- executed `instruction_counter` inside the active sample;
- per-trace `sample_index` and `instruction_index`.

Normal sample processing and debugger stepping now execute through the same internal instruction
stepper. This removes an entire class of bugs where the debugger and realtime/offline engine could
quietly implement different semantics.

A deterministic state digest covers architectural state and all semantic delay-memory words. The
digest is diagnostic, not cryptographic and not a persistence format.

The hardening pass also removes signed-left-shift undefined-behavior hazards found in fixed-point and
signed-offset paths.

## Differential conformance harness

`fv1-conformance` drives identical deterministic audio/POT vectors through Model A and Model B.
After every executed instruction it compares:

- PC before/after and raw instruction/opcode;
- ACC, PACC and LR;
- skip/sample-finished state;
- full register bank;
- delay pointer;
- SIN/COS/RAMP states;
- program/first-run/debug state;
- explicit sample/instruction coordinates.

At every sample boundary it also compares canonical architectural and full delay-memory digests, then
compares the completed stereo output bit-for-bit at the software boundary.

The report records opcode execution counts so the randomized suite can assert that every currently
implemented raw opcode family `0x00..0x14` was actually exercised.

Run it manually with:

```bash
./build/fv1-cli conformance \
  examples/steal-this-dsp-programs/00_55_gallon_saint.spn \
  --samples 256 --seed 0x4656315c2026
```

Useful options:

```text
--clock Hz
--full-delay-24
--no-delay-digest
```

On failure the first divergent sample, executed-instruction index, PC, field and production/reference
values are printed.

## Test hierarchy

Phase 5C uses progressively larger observers:

1. numeric/time/state boundary tests;
2. instruction semantics already covered by core tests plus explicit boundary cases;
3. deterministic instruction-sequence tests;
4. randomized production/reference differential programs;
5. all eight bundled Steal This DSP programs;
6. Clang/libFuzzer arbitrary-program hardening;
7. future physical-silicon conformance.

The bundled effects are system-level stress programs, not golden silicon oracles.

## Current automated coverage

The normal headless suite contains 18 tests, including:

- `fv1-numeric-boundary-tests`;
- `fv1-instruction-contract-tests` — specification-derived RDAX/RDFX/MULX and LOG/EXP scaling checks;
- `fv1-conformance-tests` — 48 deterministic randomized programs, alternating both delay models;
- `phase5c-demo-bank-conformance` — all eight current Steal This DSP demos;
- all pre-existing core/runtime/audio/debugger/validation/assembler/render tests.

The randomized conformance test aggregates executed opcodes and fails unless all implemented raw
opcode families `0x00..0x14` execute at least once.

## Fuzzing

Fuzz targets are opt-in because they require Clang/libFuzzer:

```bash
cmake -S . -B build-fuzz -G Ninja \
  -DFV1_BUILD_GUI=OFF \
  -DFV1_BUILD_FUZZERS=ON \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz
mkdir -p /tmp/fv1-fuzz-corpus
./build/fv1-cli assemble examples/simple_passthrough.spn /tmp/fv1-fuzz-corpus/passthrough.bin
./build-fuzz/fv1-conformance-fuzzer -runs=10000 -max_len=521 /tmp/fv1-fuzz-corpus
```

The fuzzer converts arbitrary 512-byte program images into known opcode families, derives deterministic
input state from the remaining bytes, runs both engines and traps on the first divergence. The core,
reference model and conformance layer are compiled with AddressSanitizer and UndefinedBehaviorSanitizer
in this configuration.

## Interpretation of a PASS

A conformance PASS means:

> `fv1-core` and the independently implemented `fv1-reference` agree for the tested vectors under the
> current Hardware Emulation Contract.

It does **not** mean:

> every undocumented FV-1 silicon corner is proven bit-exact.

That distinction stays explicit until Model C can be measured.

## Phase 5C exit criteria

Before the shared API is frozen for Windows/macOS work:

- [x] separate reference model exists and does not link the production core;
- [x] production sample/debug execution shares one instruction state machine;
- [x] virtual sample/instruction coordinates are observable;
- [x] deterministic architectural + delay state digests exist;
- [x] per-instruction differential comparison exists;
- [x] numeric/state boundary regression tests exist;
- [x] randomized legal-ish program differential testing exists;
- [x] all eight demo programs run through differential conformance;
- [x] libFuzzer/ASan/UBSan target exists;
- [x] fidelity/oracle assumptions are documented;
- [ ] extended soak/fuzz campaign before Linux 1.0 API freeze;
- [ ] physical-silicon closure of items marked SILICON-PENDING (deferred until hardware exists).
