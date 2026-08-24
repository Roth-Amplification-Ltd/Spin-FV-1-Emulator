# Windows Qt Frontend Status

## Current state — `v1.0.0` RELEASED / PHASE 9C COMPLETE

Windows uses the **same Qt 6 Widgets FV-1 Lab frontend as Linux**, built
natively with MSVC and backed by miniaudio/WASAPI.

Release identity:

```text
Tag:     v1.0.0
Commit:  6bcab5966d71520a7321178f116352b3ad347fef
Version: 1.0.0
Qt:      6.11.1
```

Completed checkpoints:

- **Phase 9A — Windows Qt Desktop Parity**
- **Phase 9B.0 — Workflow/platform foundation**
- **Phase 9B.2 — WASAPI hardware hardening**
- **Phase 9B.3 — Unicode + Recording/Export Hardening**
- **Phase 9B.4 — DPI + Windows Desktop Polish**
- **Phase 9C.0 — automated completion/release gate**
- **Phase 9C.1 — RC torture + clean-package acceptance**
- **Phase 9C.2 — final 1.0.0 promotion and artifact verification**

## Architecture

```text
Linux / Windows
       |
       v
same src/gui Qt 6 Widgets frontend
       |
       +-- program/audio workflow
       +-- analyzers/testbench
       +-- debugger/Delay RAM/validation
       +-- persistent desktop workspace
       |
       v
shared runtime / analysis / validation / audio
       |
       +-- Linux: miniaudio system backend
       +-- Windows: miniaudio -> WASAPI
       |
       v
locked FV-1 engine / native SpinASM compiler
```

The legacy `fv1-lab-win32` shell remains an opt-in diagnostic/proving harness,
not the shipping Windows UI.

## Accepted Windows evidence

- native MSVC 2022 / Qt 6 build and expanded regression suite;
- portable `windeployqt` deployment independent of the Qt SDK;
- manual-Start invariant across dialog/Recent/drag-drop/command-line workflows;
- stable WASAPI endpoint IDs and real capture → FV-1 → playback;
- 44.1/48 kHz and 128/256/512/1024 requested-buffer coverage;
- Unicode and >260-character `.spn`/`.wav` filesystem paths;
- transactional recording and validation/report output;
- 100/125/150/200% DPI smokes and PerMonitorV2 desktop handling;
- all bundled SpinASM programs through packaged and realtime paths;
- 100 repeated host/runtime lifecycle cycles;
- 1800-second continuous realtime soak with zero output underruns,
  zero analyzer drops and `device-lost=no`;
- exact portable ZIP SHA-256/manifest/commit verification;
- independent clean-machine package verification;
- final human extracted-package launch/visual acceptance.

## Final artifact

```text
dist\windows\FV1Lab-1.0.0-windows-x64.zip
SHA-256: 26a8f2be228afdd46195e1d5f10df5870ca7eab9523d6d68c647b64583d24f26
```

The `.sha256` and `.manifest.json` sidecars are part of the release evidence.
The manifest records the exact release commit above.

## Maintenance state

Windows desktop feature development is closed for the 1.0 line. Future 1.0.x
work is bug-fix, compatibility, packaging and documentation maintenance unless
a separately approved backward-compatible feature belongs to the shared
emulator/testbench product. No maintenance patch may silently change FV-1
execution semantics or the FV1SDK ABI v1 contract.
