# Native Apple Frontend Status

## macOS Phase 8D completion candidate — August 16, 2026

The native FV-1 Lab macOS frontend has completed the Phase 8B testbench parity
pass and the Phase 8C workflow/inspector parity pass. Phase 8D is the final
release/completion gate rather than a feature-development phase.

## Working macOS product surface

- Native SwiftUI macOS application builds and launches.
- Branded startup splash with staged progress and replay from Help.
- Manual laboratory workflow: loading a program never auto-starts audio.
- SpinASM open, native compile/load, raw 512-byte program load, and clipboard
  SpinASM paste.
- Test Generator, Audio Interface, and WAV Audio File Loop sources.
- Native Core Audio I/O device selection with Aggregate Device support.
- Persistent preferred device, sample rate, and buffer-size preferences.
- POT0/POT1/POT2 control.
- DSP/FX bypass with processed analysis kept alive.
- Raw + processed oscilloscope, spectrum, spectrogram, and level analysis.
- Selectable analyzer FFT sizes: 1024 / 2048 / 4096 / 8192.
- Raw / processed / dual recording and CSV analyzer exports.
- Audio-loop play/pause/stop, seek, loop region, looping, and crossfade.
- Offline instruction/sample debugger.
- PC/opcode/ACC/PACC/LR/ADDR_PTR/DAC/LFO/REG0–31 inspection.
- Physical circular Delay RAM viewer.
- Hardware validation comparison and deterministic validation-pack generation.
- Linux-equivalent themes, accent colors, and four application-icon variants.
- Linux-style File / Audio / Analysis / View / Help menu organization.

## Phase 8D automated release gate

`tools/macos-phase8d-gate.sh` is the canonical macOS completion gate.

It verifies:

1. `git diff --check`.
2. Apple frontend architectural boundary.
3. Full platform-neutral host CTest suite.
4. Native compilation and Apple-bridge execution of every shipped SpinASM demo.
5. Configurable accelerated Apple realtime bridge soak.
6. macOS Debug build.
7. macOS Release build.
8. Application bundle metadata and linkage.
9. DMG construction.
10. Optional production Developer ID signing and Apple notarization/stapling.

The eight Steal This DSP programs plus `examples/simple_passthrough.spn` form the
shipped-program regression set.

## Production release acceptance still requires human/live validation

The automated bridge soak is intentionally not claimed to be a substitute for a
real Core Audio session. Before declaring the macOS build production-ready,
complete `docs/MACOS-RELEASE-CHECKLIST.md`, including:

- real audio-interface live soak;
- Test Generator live soak;
- Audio File Loop live soak;
- analyzer/recording observation during the soak;
- visual regression across every theme and accent;
- Open Program / Paste SpinASM / Open Audio Loop regression;
- debugger and Delay RAM smoke;
- validation-tab smoke;
- final Developer ID signing/notarization/stapling.

## Test commands

Fast host-only Apple regression:

```bash
./tools/test-apple.sh host
```

Host + native macOS Debug build:

```bash
./tools/test-apple.sh macos Debug
```

Full Phase 8D Mac gate with a 60-second accelerated bridge soak:

```bash
FV1_MACOS_SOAK_SECONDS=60 ./tools/macos-phase8d-gate.sh
```

Longer candidate gate:

```bash
FV1_MACOS_SOAK_SECONDS=1800 ./tools/macos-phase8d-gate.sh
```

Ad-hoc DMG:

```bash
./tools/package-macos.sh adhoc
```

Production signing/notarization:

```bash
export FV1_CODESIGN_IDENTITY="Developer ID Application: YOUR NAME (TEAMID)"
export FV1_NOTARY_PROFILE="fv1lab-notary"
./tools/package-macos.sh developer-id
```

## Architecture

Phase 8D does not alter the FV-1 execution model or frozen public SDK ABI.

The macOS frontend remains a client of the public FV-1 SDK plus the deliberately
separate Apple/testbench bridge. Realtime processing remains outside SwiftUI and
MainActor work. Phase 8D adds release verification and distribution tooling
around the already-established engine/frontend boundaries.
