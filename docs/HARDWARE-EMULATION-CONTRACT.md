# FV-1 Hardware Emulation Contract

## Purpose

This document defines what the **Spin FV-1 Emulator** attempts to reproduce and, just as importantly,
what it does **not** yet claim to reproduce. The contract follows the project research methodology:
fidelity is multidimensional, state and simulated time are explicit, reference and production models
are kept separate, and undocumented behavior is labeled rather than guessed.

The observer for this project is software or test equipment interacting with an FV-1 program through
its program image, audio inputs/outputs, POT controls, virtual clock, architectural state and delay/LFO
behavior. Electrical pin drive, converter analog transfer functions and physical jitter are outside the
current software-only observer and remain future silicon-validation work.

## Fidelity classes used here

| Class | Meaning in this project |
|---|---|
| Functional | The same program/input/state produces the same intended logical DSP behavior. |
| State | ACC/PACC/LR, registers, delay state, LFOs, RUN state and program flow evolve consistently. |
| Event/order | Observable state transitions occur in one explicit virtual-sample/instruction order. |
| Timing | Behavior scales from the configured virtual FV-1 sample clock rather than host audio rate. |
| Quantization | 24-bit datapath, encoded coefficients, POT codes and selected delay precision are represented. |
| Reset/startup | Reset, first-run behavior and initial state are deterministic and testable. |
| Failure/pathological | Boundary values, malformed programs and unusual legal instruction sequences are regression targets. |
| Electrical/analog | **Deferred:** ADC/DAC analog response, pin thresholds, loading, oscillator jitter and board effects require hardware. |

## Oracle status vocabulary

Every fidelity claim should be classifiable as one of the following:

- **DOCUMENTED** — stated directly in primary Spin Semiconductor material.
- **SPEC-DERIVED** — a direct implementation consequence of documented encodings/equations.
- **PROJECT ASSUMPTION** — a deliberate executable interpretation used consistently but not proven as exact silicon behavior.
- **SILICON-PENDING** — cannot responsibly be called exact until compared with a physical FV-1.

Agreement between `fv1-reference` and `fv1-core` proves that two implementations agree with the
project contract. It does **not** by itself prove that the contract matches undocumented silicon.

## External inputs

- one 128-word / 512-byte external FV-1 program image;
- stereo ADC sample values, represented at the emulator boundary as normalized floating-point input;
- POT0, POT1 and POT2 normalized control values;
- configured virtual sample rate / FV-1 clock-derived processing rate;
- reset/load/control operations exposed by the library API.

## External outputs

- stereo DACL/DACR sample values;
- deterministic architectural snapshots for inspection/debugging;
- deterministic delay-memory inspection;
- instruction traces with virtual sample/instruction coordinates;
- diagnostic state digests used only for conformance/regression tests.

## Architectural state

The executable contract explicitly tracks:

- ACC, PACC and LR;
- the 64-entry exposed register space, including special registers and REG0..REG31;
- 32K delay-memory addresses and the current circular delay pointer;
- SIN0/SIN1 and RMP0/RMP1 generator state;
- program counter and first-run/RUN state;
- whether an instruction-stepped sample is active;
- completed virtual-sample count and current executed-instruction count.

## Virtual-time model

`fv1_process_sample()` and instruction-debug execution use the **same internal instruction-step state
machine**. This is a Phase-5C invariant: normal rendering, realtime processing and debugger stepping
must not implement different instruction semantics.

The current executable sample order is:

1. begin a virtual sample and latch/quantize software-visible ADC/POT inputs;
2. execute the loaded program, instruction by instruction, up to the 128-word program boundary;
3. publish DACL/DACR for that completed sample;
4. advance end-of-sample delay/LFO state and clear first-run state;
5. increment the virtual sample counter.

The fact that the project uses this order is **PROJECT ASSUMPTION / SPEC-DERIVED** where Spin's public
material is not explicit enough to prove a sub-cycle ordering detail. A future silicon test that
contradicts this model changes the machine model and its regression vectors; it must never produce an
effect-specific compatibility hack.

## Numeric contract and evidence status

| Area | Current contract | Status |
|---|---|---|
| Register/converter datapath | signed 24-bit two's-complement fractional values | DOCUMENTED |
| Saturation | processed arithmetic clamps to the signed 24-bit range | DOCUMENTED / SPEC-DERIVED |
| External program size | 128 × 32-bit words = 512 bytes | DOCUMENTED |
| RDA/WRA/WRAP coefficient | signed 11-bit encoded coefficient | DOCUMENTED |
| Register arithmetic coefficients | signed 16-bit field interpreted as Q-format used by SpinASM-compatible encoding | SPEC-DERIVED |
| POT range | 9-bit control codes spanning approximately 0..0.998 | DOCUMENTED |
| POT hysteresis | real device applies hysteresis | DOCUMENTED; exact transition behavior SILICON-PENDING |
| Delay memory | 32K words, reduced precision relative to registers | DOCUMENTED |
| `FV1_DELAY_REFERENCE_16` | keeps upper 16 bits of semantic Q1.23 delay samples | PROJECT ASSUMPTION |
| `FV1_DELAY_FULL_24` | diagnostic model preserving all 24 bits | PROJECT DIAGNOSTIC; not a silicon claim |
| LOG/EXP scaling | LOG result uses the documented /16 convention and EXP is paired to that scale | DOCUMENTED / SPEC-DERIVED |
| LOG/EXP approximation at extreme codes | current mathematical approximation | PROJECT ASSUMPTION / SILICON-PENDING |
| SIN/RAMP generators | rates/ranges are clock-dependent stateful generators | DOCUMENTED |
| exact LFO recurrence/update edge | current executable recurrence and end-of-sample update | PROJECT ASSUMPTION / SILICON-PENDING |
| CHO | LFO-derived delay/address/interpolation behavior and documented flags | partly DOCUMENTED |
| CHO internal address/fraction partition/corners | current executable split | PROJECT ASSUMPTION / SILICON-PENDING |
| RMPA encoding/precision details | current project assembler/core interpretation | PROJECT ASSUMPTION pending stronger primary evidence |
| ADC/DAC analog response and latency | not modeled as a silicon-equivalent analog path | SILICON-PENDING |
| crystal/clock rate scaling | virtual sample rate is explicit/configurable | SPEC-DERIVED; physical clock/analog behavior SILICON-PENDING |

## Primary FV-1 documentation used as specification evidence

- Spin Semiconductor, **FV-1 Architecture Overview**: <https://spinsemi.com/knowledge_base/arch.html>
- Spin Semiconductor, **FV-1 Instructions and Syntax**: <https://www.spinsemi.com/knowledge_base/inst_syntax.html>
- Spin Semiconductor, **Programming the FV-1**: <https://spinsemi.com/knowledge_base/pgm_quick.html>
- Spin Semiconductor, **FV-1 Design Philosophy**: <https://www.spinsemi.com/knowledge_base/FV-1_philosophy.html>
- Spin Semiconductor, **Coding Examples**: <https://www.spinsemi.com/knowledge_base/coding_examples.html>

Primary documentation is the preferred written oracle. The independent executable model is a second
oracle for implementation divergence. Physical FV-1 measurements will become the final oracle for
undocumented or ambiguous silicon behavior.

## Model separation

### Model A — `fv1-reference`

A deliberately simple C++ reference implementation. It does not link `FV1::core` and does not reuse
the production decoder or production arithmetic helpers. Readability and auditability are preferred
over speed.

### Model B — `fv1-core`

The production emulator used by the runtime, debugger, CLI and GUI. It may be optimized, but must
conform to the same contract and pass differential tests against Model A.

### Model C — physical Spin FV-1

Deferred until a board and capture rig are available. The same deterministic conformance/validation
vectors should be reused rather than inventing a separate hardware-only test methodology.

## Project rule: fix the machine, not the effect

If a real program exposes a mismatch, the project must identify the architectural/numeric/timing
semantic responsible and fix that semantic. No code path may special-case `55 Gallon Saint`,
`Gravity Clerk`, another bundled demo, or any third-party FV-1 program to obtain compatibility.

## Definition of done for a fidelity claim

A claim is considered hardened when it has:

1. a declared oracle/status in this contract;
2. deterministic stimulus/vector coverage;
3. a regression test at the lowest useful level (numeric primitive/instruction/state sequence);
4. differential coverage between reference and production models where applicable;
5. later, physical-capture evidence for anything marked SILICON-PENDING.
