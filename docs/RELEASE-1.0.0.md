# FV-1 Lab 1.0.0

FV-1 Lab 1.0.0 closes the first complete standalone desktop product line for
the Spin FV-1 Emulator project.

## Desktop products

- **Linux:** Qt 6 Widgets reference desktop.
- **macOS:** native SwiftUI FV-1 Lab through Phase 8D.
- **Windows 11:** shared Linux/Windows Qt 6 Widgets frontend with WASAPI,
  completed through Phase 9C.

## Core/testbench capability

- SpinASM/native program loading and compilation;
- virtual FV-1 registers, Delay RAM, LFOs and instruction/sample inspection;
- Test Generator, WAV Loop and live Audio Interface sources;
- independent host/FV-1 clock domains with production SRC;
- raw and processed Scope, Spectrum, Spectrogram and Levels;
- DSP bypass;
- recording/export;
- validation packs, reference/capture comparison and report bundles;
- reusable public FV1SDK ABI v1 boundary.

## Windows release hardening

- native MSVC 2022 build;
- miniaudio/WASAPI playback and capture;
- stable endpoint identity and reconnect behavior;
- Unicode and extended-length path handling;
- transactional recording/validation output;
- PerMonitorV2 and 100/125/150/200% DPI coverage;
- portable `windeployqt` package isolation;
- exact SHA-256/manifest/commit verification;
- all bundled SpinASM programs through packaged and realtime paths;
- 100 host/runtime lifecycle cycles;
- 1800-second continuous realtime soak with zero output underruns,
  zero analyzer drops and `device-lost=no`;
- clean-package verification without the Qt development environment.

## Fidelity boundary

1.0.0 does **not** claim undocumented physical FV-1 silicon equivalence.
Physical-chip validation remains an independent evidence-driven fidelity gate.

## Windows x64 artifacts

```text
FV1Lab-1.0.0-windows-x64.zip
FV1Lab-1.0.0-windows-x64.zip.sha256
FV1Lab-1.0.0-windows-x64.zip.manifest.json
```
