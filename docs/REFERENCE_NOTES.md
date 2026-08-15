# FV-1 Reference Notes

## Primary sources

The emulator treats original Spin Semiconductor material as the preferred written oracle:

- <https://spinsemi.com/knowledge_base/arch.html> — FV-1 Architecture Overview
- <https://www.spinsemi.com/knowledge_base/inst_syntax.html> — FV-1 Instructions and Syntax
- <https://spinsemi.com/knowledge_base/pgm_quick.html> — Programming the FV-1
- <https://www.spinsemi.com/knowledge_base/FV-1_philosophy.html> — FV-1 Design Philosophy
- <https://www.spinsemi.com/knowledge_base/coding_examples.html> — Coding Examples

The public material documents important architectural facts such as the 24-bit fractional processor,
encoded instruction coefficients, 9-bit POT controls with hysteresis, lower-precision/special delay
storage, clock-dependent LFOs, and LOG/EXP scaling conventions. It does not expose every proprietary
bit-level or analog detail needed to claim complete silicon equivalence.

## Research methodology

Phase 5C also adopts the project's Digital Hardware Emulation research capsule as an engineering
methodology. In particular:

- fidelity is separated into functional/state/time/quantization/reset/etc. rather than one vague score;
- an explicit Hardware Emulation Contract declares observer, state, time and oracle;
- a simple golden/reference model is kept separate from the optimized production model;
- the same deterministic vectors drive both models;
- physical hardware remains the final oracle for behavior not completely specified in documentation;
- compatibility fixes belong in the machine model, never in program-specific workarounds.

See `HARDWARE-EMULATION-CONTRACT.md` and `PHASE5C-MODEL-HARDENING.md`.

## Phase 6 observer/API boundary

The same emulation methodology applies to software architecture: the stable SDK describes what an
external observer may provide and observe, while the implementation behind that boundary remains
replaceable. Phase 6 therefore freezes neither Qt nor the internal C++ object graph. It extracts a
small C ABI, tests it from an external consumer, and postpones the compatibility promise until a
dedicated ABI review. This mirrors the Hardware Emulation Contract principle of defining observer,
state and semantics explicitly before optimizing or multiplying implementations.
