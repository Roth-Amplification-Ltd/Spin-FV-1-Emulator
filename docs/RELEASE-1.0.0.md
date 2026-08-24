# FV-1 Lab 1.0.0

FV-1 Lab 1.0.0 closes the first complete standalone desktop product line for
the Spin FV-1 Emulator project.

## Release identity

```text
Tag:    v1.0.0
Commit: 6bcab5966d71520a7321178f116352b3ad347fef
```

The tag is immutable. Documentation/maintenance commits after release may
advance `main`; 1.0.0 artifacts must remain traceable to the tagged commit.

## Desktop products

| Platform | Frontend | 1.0 state |
|---|---|---|
| Linux | Qt 6 Widgets | Feature-complete reference desktop |
| macOS | Native SwiftUI | Phase 8D complete |
| Windows 11 | Shared Linux/Windows Qt 6 Widgets + WASAPI | Phase 9C complete / 1.0.0 released |

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

## Windows 1.0 acceptance

- native MSVC 2022 / Qt 6.11.1 build;
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
- clean-package verification without the Qt development environment;
- final human extracted-package launch/visual acceptance.

## Windows x64 final artifact

```text
FV1Lab-1.0.0-windows-x64.zip
SHA-256: 26a8f2be228afdd46195e1d5f10df5870ca7eab9523d6d68c647b64583d24f26
```

The `.sha256` and `.manifest.json` sidecars are release evidence and should be
published beside the ZIP when practical.

## Linux and macOS binary publication

The Linux packaging helpers and macOS DMG/signing/notarization workflow are
complete. Binary assets advertised as **1.0.0** must be built from the exact
`v1.0.0` tag. Do not attach binaries built from a later documentation or
maintenance commit to the 1.0.0 GitHub Release.

## Fidelity boundary

1.0.0 does **not** claim undocumented physical FV-1 silicon equivalence.
Physical-chip validation remains an independent evidence-driven fidelity gate.
Agreement between the production and independent software reference models is
conformance to the documented project hardware-emulation contract, not proof of
undocumented silicon behavior.

## After 1.0

See [`POST-1.0-ROADMAP.md`](POST-1.0-ROADMAP.md). The desktop porting program is
closed. Maintenance must preserve the locked execution model and FV1SDK ABI v1;
physical validation and genuinely useful new testbench capability are the next
engineering tracks.
