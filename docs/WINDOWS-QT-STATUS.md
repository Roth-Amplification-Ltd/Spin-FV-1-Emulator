# Windows Qt Frontend Status

## Current state — Phase 9C final release / 1.0.0

Windows uses the **same Qt 6 Widgets FV-1 Lab frontend as Linux**, built
natively with MSVC and backed by miniaudio/WASAPI.

Completed checkpoints:

- **Phase 9A — Windows Qt Desktop Parity**
- **Phase 9B.0 — Workflow/platform foundation**
- **Phase 9B.2 — WASAPI hardware hardening**
- **Phase 9B.3 — Unicode + Recording/Export Hardening**
- **Phase 9B.4 — DPI + Windows Desktop Polish**
- **Phase 9C.0 — automated completion/release gate**
- **Phase 9C.1 — RC torture + clean-package acceptance**

Final checkpoint:

- **Phase 9C.2 — final 1.0.0 promotion and tag-ready artifact verification**

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
- exact portable ZIP SHA-256/manifest/commit verification.

## Final 1.0.0 procedure

Preflight the proposed promotion before commit:

```powershell
.\tools\windows-phase9c-final-release.ps1 `
  -QtDir "C:\Qt\6.11.1\msvc2022_64" `
  -PreflightOnly
```

After the promotion commit is reviewed, committed and pushed:

```powershell
.\tools\windows-phase9c-final-release.ps1 `
  -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

The successful final artifacts are:

```text
dist\windows\FV1Lab-1.0.0-windows-x64.zip
dist\windows\FV1Lab-1.0.0-windows-x64.zip.sha256
dist\windows\FV1Lab-1.0.0-windows-x64.zip.manifest.json
```

Only after the final gate and human packaged-app check pass should the release
commit receive annotated tag `v1.0.0`.
