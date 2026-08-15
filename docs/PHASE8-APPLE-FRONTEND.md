# Phase 8 — Native Apple FV-1 Lab (macOS + iPadOS)

Phase 8 makes FV-1 Lab a first-class Apple-platform product while preserving Linux as the reference
host and leaving Windows parked for later completion. There is deliberately **no iPhone target**.

## Product architecture

- Swift 6 and SwiftUI own the application UI on both macOS and iPadOS.
- AppKit is available only for genuinely macOS-specific integrations; the shared UI does not depend on it.
- The application is document-based with SwiftUI `DocumentGroup`/`FileDocument` for SpinASM source.
- The frontend talks to the existing FV-1 public C ABI. No frontend Swift source imports private core,
  runtime, SpinASM, debugger, validation, reference-model, Qt, or miniaudio interfaces.
- Xcode compiles the existing SDK implementation sources into each native app target; Swift sees only
  `fv1/sdk.h` through the bridging header.
- A platform-neutral C11 realtime bridge owns a dedicated SDK engine, fixed-capacity SPSC output FIFO,
  32.768 kHz clock-domain conversion, atomic POT handoff, telemetry, and scope history.

## Realtime audio

`AVAudioEngine` owns Apple device I/O. Input arrives as non-interleaved Float32, crosses the C11 bridge
into the FV-1's fixed 32.768 kHz virtual sample domain, is processed through the public SDK, and is
resampled into an `AVAudioSourceNode` feeding the Apple output graph. The render callback never blocks
or allocates in project code. Missing output becomes silence plus underflow telemetry.

On iPadOS the app configures an `AVAudioSession` for `playAndRecord` + `measurement`, exposes available
input ports through the native route APIs, and follows the system-selected output route. macOS uses the
system default AVAudioEngine input/output devices in this candidate.

Program load/reset operations stop the audio engine before mutating the realtime SDK instance. POT
changes cross an atomic handoff and are applied by the audio/input callback itself between process calls,
keeping the SDK engine single-thread-owned.

## Native UI

The shared SwiftUI workspace provides:

- SpinASM document editing and native open/save behavior;
- Compile & Load through `fv1_sdk_compile_spinasm_v1`;
- raw 512-byte program import;
- POT0/POT1/POT2 controls and reset;
- start/stop audio, route/rate and xrun telemetry;
- SwiftUI Canvas stereo realtime scope;
- virtual-chip snapshot display;
- static resource analysis;
- compile/diagnostic console;
- adaptive `NavigationSplitView` presentation suitable for desktop and iPad.

## Xcode workflow

Open:

```text
apple/FV1Lab.xcodeproj
```

Shared schemes:

- `FV1 Lab macOS`
- `FV1 Lab iPadOS`

For a connected iPad, select your development team in Signing & Capabilities and choose the device in
Xcode. The iPad target uses device family **2 only**; no iPhone product is generated.

## Command-line workflow

Host SDK proving build:

```bash
./tools/build-apple-sdk.sh
```

Native Apple builds on macOS:

```bash
./tools/build-apple.sh macos Debug
./tools/build-apple.sh ipados Debug
./tools/build-apple.sh all Release
```

Full host regression plus native Xcode builds:

```bash
./tools/test-apple.sh
```

The Xcode project is generated deterministically by `tools/generate_apple_xcodeproj.py` and committed,
so both normal Xcode development and reproducible `xcodebuild` CI operate on the same project.

## Current acceptance status

The platform-neutral Phase-8 realtime bridge and Apple dependency boundary are continuously testable on
Linux. Native SwiftUI/AVFAudio application compilation requires Xcode/macOS and is authoritative in the
`Native Apple Frontend` GitHub Actions workflow and on physical Apple hardware.

Physical iPad audio-interface testing remains necessary for route/USB-interface behavior; simulator CI
only proves the app target and Apple API integration compile correctly.
