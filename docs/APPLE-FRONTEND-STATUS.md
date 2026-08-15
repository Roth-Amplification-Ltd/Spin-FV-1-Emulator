# Native Apple Frontend Status

## macOS bring-up checkpoint — August 15, 2026

The native FV-1 Lab macOS frontend has reached its first usable interactive
bring-up milestone.

### Working

- Native SwiftUI macOS application builds successfully with Xcode.
- Application launches directly into FV-1 Lab rather than behaving as a
  document-based application.
- Branded FV-1 Lab startup splash is displayed.
- Main interface has been redesigned around the Linux engineering-dashboard
  layout rather than the earlier page-oriented Apple prototype.
- Persistent program, source, controls, analyzer, telemetry, console, resource,
  DSP-status, and chip-inspection areas are present.
- SpinASM text programs can be opened, compiled, and loaded.
- Raw 512-byte FV-1 programs remain supported.
- Program loading recognizes textual SpinASM by content rather than requiring
  only one filename extension.
- Native Apple realtime bridge is functional.
- Built-in test generator is functional.
- Test generator supports:
  - Sine
  - Logarithmic sweep
  - White noise
  - Pink noise
  - Impulse
- Generator frequency and amplitude controls are exposed.
- Test Generator is the default source.
- Audio Interface remains available as a separate live-input source.
- CoreAudio render/input callbacks no longer incorrectly inherit MainActor
  execution and the previous microphone-permission realtime-thread crash has
  been corrected.
- FV-1 program processing through the native Apple audio path is functional.
- Realtime scope and telemetry are connected to the Apple realtime bridge.
- POT0/POT1/POT2 controls are connected to the virtual FV-1.
- Start/Stop is manual: loading a program does not automatically begin audio.
- macOS application bundle metadata is valid.

### Current interaction model

1. Launch FV-1 Lab.
2. Splash is displayed.
3. Dashboard opens in the stopped/ready state.
4. Select Test Generator or Audio Interface.
5. Open/load an FV-1 program.
6. Press Start manually.
7. Press Stop manually when finished.

This intentionally matches laboratory/testbench behavior: loading a program
does not implicitly start audio.

### Known Apple frontend work remaining

- Complete visual and functional parity with all Linux analyzer panels.
- Port/wire Spectrum analyzer.
- Port/wire Spectrogram.
- Port/wire Levels analysis.
- Port/wire Validation/testbench panel.
- Expand Delay RAM viewer to the full Linux implementation.
- Continue polishing console, resource, and chip-inspection parity.
- Add explicit macOS audio-device selection rather than relying only on system
  default devices.
- Perform longer-duration realtime audio stability testing.
- Validate more SpinASM programs against Linux/reference behavior.
- Complete and validate the landscape-only iPadOS frontend.
- Test native audio on physical iPad hardware.
- Add production code signing, entitlements, packaging, and notarization.
- Run final Apple regression testing before release-candidate status.

## Architecture

The Apple frontend remains a client of the public FV-1 SDK/realtime bridge.
The emulator core and public SDK ABI remain platform-independent.

The realtime audio path is kept separate from SwiftUI and MainActor work.
Generator/audio processing remains in the realtime-safe native bridge rather
than being implemented as UI-thread DSP.
