# Phase 7 — Native Windows FV-1 Lab

Phase 7 consolidates the Windows work into one product milestone. The Windows application is a native
Win32/WASAPI client of the public FV-1 SDK; it does not link Qt, miniaudio, the Linux runtime, the
private emulator core, or the private SpinASM implementation.

## Product path

The application supports:

- native Unicode Win32 controls and application resources;
- SpinASM editing, compile/load, and raw 512-byte FV-1 program loading through `FV1::sdk`;
- POT0/POT1/POT2 and reset control;
- offline virtual-chip snapshot/resource inspection;
- event-driven shared-mode WASAPI full-duplex audio;
- a native 32.768 kHz stereo float FV-1 stream, with the Windows Audio Engine performing endpoint
  sample-format/channel/rate conversion in shared mode;
- a separate public-SDK engine on the realtime path so GUI/debug operations never touch the active
  audio engine;
- actual realtime output scope data rather than a synthetic scope while streaming;
- capture/render frame counters, output underflow/overflow counters, recovery count, endpoint names,
  and last-error reporting;
- automatic stream recreation after endpoint invalidation or a default capture/render-device change;
- MMCSS `Pro Audio` scheduling for the WASAPI worker thread;
- native MSVC shared/static CI and staged-product verification.

Deep register/snapshot inspection intentionally stays off the WASAPI thread. While live audio is
running the UI reports realtime-safe audio telemetry; stop audio to inspect a coherent deep chip
snapshot.

## Build on Windows

```powershell
cmake -S . -B build-win32 `
  -DFV1_BUILD_TESTS=ON `
  -DFV1_BUILD_GUI=OFF `
  -DFV1_ENABLE_LIVE_AUDIO=OFF `
  -DFV1_BUILD_WINDOWS_FRONTEND=ON

cmake --build build-win32 --config RelWithDebInfo --parallel
ctest --test-dir build-win32 -C RelWithDebInfo --output-on-failure
cmake --install build-win32 --config RelWithDebInfo --prefix stage-win32
```

The staged product executable is `stage-win32/bin/fv1-lab-win32.exe`. A shared SDK build also stages
`stage-win32/bin/fv1-sdk.dll` beside it.

## Acceptance boundary

Linux can continuously prove the platform-neutral frontend session/realtime wrappers and public-SDK
boundary. The `Native Windows Frontend` GitHub Actions matrix is the authoritative compile/install
proof for Windows SDK headers, Win32, WASAPI, MSVC, and both shared/static SDK linkage.
