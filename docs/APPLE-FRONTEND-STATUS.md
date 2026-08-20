# Native Apple Frontend Status

## macOS Phase 8D — COMPLETE

The native FV-1 Lab macOS frontend completed all three approved Apple desktop
phases:

1. **Phase 8B — Mac Testbench Parity**
2. **Phase 8C — Mac Workflow + Inspector Parity**
3. **Phase 8D — Mac Completion / Release**

The Phase 8D completion checkpoint is commit `c85e670`.

## Working macOS product surface

- Native SwiftUI macOS application.
- Branded staged startup splash with replay from Help.
- Manual workflow: loading a program never auto-starts audio.
- SpinASM open/native compile, raw 512-byte program load and Paste SpinASM.
- Test Generator, Audio Interface and WAV Audio File Loop sources.
- Native Core Audio device selection with Aggregate Device support.
- Persistent device/sample-rate/buffer preferences.
- POT0/POT1/POT2.
- DSP/FX bypass.
- Raw + processed Scope/Spectrum/Spectrogram/Levels.
- Selectable analyzer FFT sizes 1024/2048/4096/8192.
- Raw/processed/dual recording and CSV analyzer export.
- WAV-loop transport, seek, region, looping and crossfade.
- Offline instruction/sample debugger.
- Full register/LFO/DAC/PC state inspection.
- Physical circular Delay RAM viewer.
- Validation comparison and deterministic validation-pack generation.
- Linux-equivalent themes, accents and four application icons.
- Native File / Audio / Analysis / View / Help organization.

## Completion/release gate

`tools/macos-phase8d-gate.sh` remains the canonical Mac regression/release gate.

It covers text hygiene, the Apple frontend boundary, host CTest, every shipped
SpinASM program, accelerated Apple realtime bridge soak, Debug + Release builds,
bundle metadata/linkage, DMG construction, and optional Developer ID
signing/notarization/stapling.

Production signing/notarization is a release-operation choice; it is not a
reason to reopen Phase 8 feature development.

## Architecture

The completed macOS frontend remains a consumer of the public FV-1 SDK plus the
separate Apple/testbench bridge. Realtime processing stays outside SwiftUI and
MainActor work.

Phase 8 does not alter the locked FV-1 execution model or public FV1SDK ABI.
