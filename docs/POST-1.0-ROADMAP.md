# FV-1 Lab Post-1.0 Roadmap

The standalone Linux/macOS/Windows desktop-porting program closed at `v1.0.0`.
This roadmap prevents routine maintenance or future product work from casually
reopening the stable virtual-chip contracts.

## 1.0 maintenance policy

### `1.0.x` — patch releases

Allowed:

- correctness fixes against documented behavior;
- crashes, hangs, leaks and realtime-safety bugs;
- host-audio/device compatibility fixes;
- OS/toolchain/Qt/Xcode/compiler compatibility;
- packaging, signing, notarization and installer fixes;
- accessibility and UI defects that do not change emulator semantics;
- documentation and test improvements.

Not allowed silently:

- breaking FV1SDK ABI changes;
- intentional FV-1 semantic changes without evidence and regression coverage;
- frontend-specific DSP behavior;
- moving emulator state/logic into GUI code.

### `1.x` — backward-compatible capability

New emulator/testbench capability can be added when it preserves ABI-v1
compatibility and the existing observable behavior of supported programs.
Examples include additional measurement/validation tooling, diagnostics or SDK
conveniences that do not require a breaking contract.

### `2.0` — explicit breaking boundary

Reserve a 2.0 transition for a deliberately reviewed incompatible ABI or product
contract change. Do not use a major-version change merely for UI redesigns or
normal platform maintenance.

## Priority engineering tracks

### 1. Physical FV-1 silicon validation

When suitable FV-1 hardware and capture infrastructure are available:

- run the existing deterministic validation pack;
- quantify converter/board latency separately from DSP behavior;
- measure residual/correlation and magnitude/phase response;
- probe Delay RAM precision/timing;
- characterize LFO/CHO behavior;
- verify POT quantization/hysteresis;
- verify clipping/saturation and LOG/EXP boundary behavior;
- test crystal-derived clock-rate behavior;
- turn every confirmed discrepancy into a minimal regression vector before
  changing the production model.

This work may correct the model when evidence demands it, but must not devolve
into per-effect hacks.

### 2. Emulator/testbench capability

Prefer capabilities that make FV-1 Lab a better electronic instrument and lab:
repeatable measurement, comparison, capture, debugging and hardware-validation
workflows. Preserve manual Start and the separation between GUI, host runtime
and virtual-chip semantics.

### 3. FV1SDK ecosystem

Maintain the public ABI-v1 package and proving hosts. New language bindings or
host examples should consume the public SDK rather than private engine headers.
Breaking ABI work requires an explicit major-version plan.

### 4. Dedicated IDE — separate product

A future SpinASM IDE may consume FV1SDK for compilation, emulation and
inspection. Source editing, projects, source-mapped breakpoints, EEPROM-bank
editing and IDE-specific workflows belong in that separate product rather than
turning FV-1 Lab into a source editor.

### 5. Desktop maintenance

Linux and Windows share the Qt frontend; macOS remains native SwiftUI. Fix
platform-specific host/GUI issues at those boundaries without duplicating DSP
semantics. Add another desktop port only if a concrete product requirement
justifies reopening platform work.

## Release artifact discipline

For every tagged release:

1. tag only a reviewed, tested commit;
2. build binary artifacts from that exact tag;
3. record SHA-256 checksums and build provenance;
4. keep release notes explicit about platform/package coverage;
5. never relabel a later `main` artifact as an older release;
6. never move a published release tag.

## Current baseline

```text
Release:  v1.0.0
Commit:   6bcab5966d71520a7321178f116352b3ad347fef
Windows:  final portable x64 artifact accepted
Linux:    packaging pipeline available
macOS:    DMG/signing/notarization pipeline available
```

See [`RELEASE-STATUS-1.0.0.md`](RELEASE-STATUS-1.0.0.md) for the immutable 1.0
closure record.
