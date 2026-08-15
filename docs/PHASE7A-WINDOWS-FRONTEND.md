# Phase 7A — Native Windows frontend foundation

> Historical Phase 7A foundation note: this document records the first Windows-shell checkpoint. Current Windows work is consolidated under `docs/PHASE7-WINDOWS-FRONTEND.md` as **Phase 7**.

## Objective

Begin the first non-Linux product frontend **without cloning or weakening the emulator model**.
The native Windows application is an SDK consumer. It must not include private core, runtime,
reference-model, validation, Qt, miniaudio, or Linux frontend headers.

Phase 7A is deliberately the native-shell and integration foundation. It proves that the frozen-candidate
public SDK is sufficient to build a useful Windows chip-monitor application before realtime duplex
WASAPI streaming is layered on top.

## Delivered surface

- `fv1-lab-win32`: native Unicode Win32 desktop application;
- public `FV1::sdk` is its only emulator/DSP dependency;
- native File/Open workflow for SpinASM and 512-byte program images;
- editable SpinASM scratchpad and Compile & Load path through `fv1_sdk_compile_spinasm_v1`;
- POT0/POT1/POT2 trackbars through the public control API;
- deterministic inaudible 440 Hz virtual-input probe through public sample processing;
- owner-drawn GDI virtual output scope;
- live public-SDK snapshot and resource-meter views;
- reset control and native About presentation;
- default WASAPI capture/render endpoint discovery using `MMDevice` + `IAudioClient` mix-format queries;
- Windows `.ico` generated directly from the accepted blue 512×512 application artwork (no redesign);
- portable frontend-session regression test that also runs on Linux;
- Windows Server 2025 / Visual Studio MSVC CI for both shared and static SDK linkage.

## Architectural gate

The following imports are forbidden from `src/windows/` frontend code:

- `fv1/fv1.h` or `fv1/fv1.hpp`;
- `fv1/runtime.hpp`;
- `fv1/spinasm.hpp`;
- `fv1/debugger.hpp`;
- `fv1/validation.hpp`;
- any Qt header;
- miniaudio or Linux audio headers.

The frontend may use `fv1/sdk.h` and, in a later debugger increment, `fv1/sdk_debug.h`.

## Audio scope

Phase 7A **does not claim realtime Windows audio acceptance**. It probes the native WASAPI endpoint
contract and keeps the emulator running against a deterministic virtual stimulus. The consolidated Phase 7 adds
full-duplex event-driven WASAPI streaming, explicit host↔virtual-chip sample-rate conversion, device
selection/recovery, underrun/overrun telemetry, and realtime soak tests without changing the SDK ABI.

This staging is intentional: do not hide a poor clock/SRC implementation behind a GUI just to make
sound come out early.

## Acceptance

Linux/headless development gate:

```bash
cmake -S . -B build-phase7a -G Ninja \
  -DFV1_BUILD_GUI=OFF \
  -DFV1_ENABLE_LIVE_AUDIO=OFF
cmake --build build-phase7a --parallel
ctest --test-dir build-phase7a --output-on-failure
```

Windows/MSVC gate after push:

- `fv1-lab-win32` builds with shared SDK;
- `fv1-lab-win32` builds with static SDK;
- `fv1-native-frontend-session-tests` passes in both configurations;
- Phase 6C Linux, hardening, and SDK-portability gates remain green;
- no public SDK symbol/layout change occurs.

That gate is the handoff into the consolidated Phase 7 realtime WASAPI implementation.
