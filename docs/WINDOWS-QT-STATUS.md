# Windows Qt Frontend Status

## Current state — Phase 9B.3 complete

Windows uses the **same Qt 6 Widgets FV-1 Lab frontend as Linux**, built
natively with MSVC and backed by miniaudio/WASAPI.

Completed checkpoints:

- **Phase 9A — Windows Qt Desktop Parity**
- **Phase 9B.0 — Workflow/platform foundation**
- **Phase 9B.2 — WASAPI hardware hardening**
- **Phase 9B.3 — Unicode + Recording/Export Hardening**

Next:

- **Phase 9B.4 — DPI + Windows Desktop Polish**

Final Windows phase:

- **Phase 9C — Windows Completion / Release**

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

## Phase 9A — complete

Native MSVC/Qt build, shared Linux/Windows GUI, Windows resources/version
metadata, Release packaging, `windeployqt`, portable ZIP and automated Windows
test gate.

## Phase 9B.0 — complete

Persistent workspace/controls, Recents, remembered dialogs, drag/drop,
command-line/Open-With, shortcuts, PerMonitorV2/long-path manifests and portable
package isolation.

## Phase 9B.2 — complete

Stable WASAPI endpoint IDs, persistent playback/capture selection, endpoint
refresh/loss recovery, real capture → FV-1 → playback, 44.1/48 kHz hardware
testing, 128/256/512/1024 requested buffers, negotiated asymmetric native
periods and hardware telemetry.

## Phase 9B.3 — complete

Commit `2052794`:

- Unicode-safe `.spn`, `.bin` and `.wav` workflows;
- extended-length Windows paths beyond traditional `MAX_PATH`;
- transactional `.partial-*` recording finalization;
- transactional validation WAV/report output;
- timestamped capture suggestions;
- persistent validation directories;
- truthful GUI smoke-process exit checking;
- Unicode and >260-character GUI filesystem acceptance;
- recorder/validation Unicode regression coverage.

The completed Phase 9B.3 gate includes the normal **39/39 Windows suite** plus
the dedicated filesystem acceptance test.

## Manual Start invariant

Opening a program by dialog, Recent, drag/drop, command line or Windows Open
With loads/inspects it but does **not** start realtime audio. The user presses
Start.

## Build/test

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\tools\test-windows.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

WASAPI hardware acceptance:

```powershell
.\tools\windows-audio-acceptance.ps1
```

Filesystem acceptance:

```powershell
.\tools\windows-filesystem-acceptance.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

## Next — Phase 9B.4

Phase 9B.4 is desktop polish, not emulator development: DPI scaling,
PerMonitorV2 transitions, persisted geometry/docks under scale changes,
menus/dialogs/tooltips, splash/About, taskbar/Alt-Tab identity, keyboard
conventions and theme visual regression.

After 9B.4, Phase 9C performs final Windows regression, torture testing,
packaging and release closure.
