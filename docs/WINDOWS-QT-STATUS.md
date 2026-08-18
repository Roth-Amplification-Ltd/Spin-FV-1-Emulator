# Windows Qt Frontend Status

## Phase 9B — Windows Workflow + Platform Parity

Phase 9A established the Windows product as the same Qt 6 Widgets FV-1 Lab
frontend used by Linux and proved the MSVC/WASAPI build, 39/39 automated tests,
Release packaging and standalone portable execution.

Phase 9B keeps that architecture intact and focuses on Windows product workflow,
desktop integration and regression quality rather than reimplementing features.

## Architecture remains unchanged

```text
Linux / Windows
       |
       v
same src/gui Qt 6 Widgets frontend
       |
       +-- program / audio workflow
       +-- analyzer/testbench
       +-- debugger / Delay RAM / validation
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

The legacy `fv1-lab-win32` shell remains an opt-in diagnostic harness only.

## Phase 9B workflow additions

- persistent main-window geometry and dock layout;
- View -> Reset Workspace Layout;
- persistent source mode, generator selection/frequency and POT0/POT1/POT2;
- Open Recent Program and Open Recent Audio Loop menus;
- remembered native file-dialog directories;
- drag-and-drop `.spn`, `.bin` and `.wav` files;
- command-line/open-with handling for `.spn`, `.bin` and `.wav`;
- common keyboard shortcuts for Open, Start, Stop and related workflow actions;
- Windows-aware audio backend labeling (`WASAPI via miniaudio`);
- audio-device refresh that preserves the currently selected endpoint when it
  is still present;
- Unicode-safe conversion in both directions between `QString` and
  `std::filesystem::path`;
- Windows PerMonitorV2 DPI-awareness manifest;
- Windows long-path-aware manifest;
- bootstrap-downloaded miniaudio ignored as local dependency material;
- command-line program-open smoke coverage;
- stronger portable verification that removes `QTDIR`, `QT_PLUGIN_PATH` and Qt
  development-kit paths before launching the packaged executable.

## Manual Start invariant

Phase 9B does not change the audio-start contract.

Opening a program by dialog, recent-file menu, drag-and-drop, command line or
Windows Open With loads/inspects the program but does **not** start realtime
audio. The user must press Start.

## Windows build/test

```powershell
Set-ExecutionPolicy -Scope Process Bypass

.\tools\test-windows.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

Complete Phase 9B automated gate:

```powershell
.\tools\phase9b-windows-gate.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

## Portable verification

`tools/package-windows.ps1` now builds the Release package and invokes
`tools/check-windows-portable.ps1`.

The verifier extracts the ZIP to a new temporary directory, removes Qt
development environment variables, reduces PATH to Windows system directories,
changes the working directory away from the repository, then verifies:

- `FV1Lab.exe`;
- `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`;
- `platforms\qwindows.dll`;
- installed splash and icon assets;
- application version metadata;
- normal Qt smoke;
- splash smoke;
- About smoke;
- command-line `.spn` loading.

This catches packages that appear portable only because a Qt SDK happens to be
installed on the development machine.

## Remaining acceptance

Automated success is not the same as live audio acceptance. Complete
`docs/WINDOWS-PHASE9B-CHECKLIST.md` before committing the phase-complete
checkpoint.
