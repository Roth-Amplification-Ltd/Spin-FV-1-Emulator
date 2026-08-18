# Windows Qt Frontend Status

## Phase 9A — Windows Qt Desktop Parity Bring-up

The Windows product frontend is now intentionally the **same Qt 6 Widgets
frontend used by Linux**.

The earlier hand-written `fv1-lab-win32` application remains available as an
opt-in WASAPI/SDK reference harness, but it is no longer the default Windows
product UI.

## Architecture

```text
Linux / Windows
       |
       v
same src/gui Qt 6 Widgets frontend
       |
       +-- MainWindow / menus / themes / splash
       +-- Scope / Spectrum / Spectrogram / Levels
       +-- File Loop / generator / audio-interface workflow
       +-- debugger / Delay RAM / validation
       |
       v
shared runtime / analysis / validation / audio
       |
       +-- Linux: miniaudio Linux backend
       +-- Windows: miniaudio forced to WASAPI
       |
       v
locked FV-1 engine / native SpinASM compiler
```

The Qt layer is kept platform-neutral. `tools/check_qt_frontend_boundary.py`
rejects Win32, WASAPI, X11/ALSA/Pulse, and Apple-native headers from
`src/gui` and `include/fv1/gui`.

## Phase 9A mega chunk

This phase establishes:

- Qt 6 Widgets as the Windows product frontend.
- MSVC 2022 x64 build path.
- same themes, icons, splash and engineering dashboard as Linux.
- Test Generator, Audio Interface and Audio File Loop UI inherited from Linux.
- raw/processed analyzers inherited from Linux.
- debugger, Delay RAM and validation inherited from Linux.
- miniaudio forced to WASAPI on Windows.
- pinned miniaudio 0.11.21 bootstrap.
- optional SpeexDSP parity through vcpkg.
- Unicode-safe Windows filesystem paths for WAV/report workflows.
- Windows `.exe` icon and version resource.
- cross-platform product-install CTest.
- Windows Debug/Release build helpers.
- Qt `windeployqt` portable deployment.
- Windows portable ZIP + SHA-256 generation.

## First build on Windows 11

Open PowerShell in the repository root:

```powershell
Set-ExecutionPolicy -Scope Process Bypass

.\tools\bootstrap-windows.ps1
.\tools\test-windows.ps1
.\tools\run-windows.ps1
```

If Qt is not autodetected:

```powershell
.\tools\bootstrap-windows.ps1 -QtDir "C:\Qt\<version>\msvc2022_64"
.\tools\test-windows.ps1 -QtDir "C:\Qt\<version>\msvc2022_64"
```

## Portable package

```powershell
.\tools\package-windows.ps1
```

The package is written beneath `dist\windows` and includes the application,
shared FV-1 SDK DLL, Qt runtime/plugin deployment, icons/splash assets, build
metadata, license, README and SHA-256.

## Existing native Win32 harness

The older native frontend is still buildable explicitly:

```powershell
cmake -S . -B build-win32-reference `
  -G "Visual Studio 17 2022" -A x64 `
  -DFV1_BUILD_GUI=OFF `
  -DFV1_BUILD_WINDOWS_FRONTEND=ON

cmake --build build-win32-reference --config Debug
```

It is retained for low-level SDK/WASAPI diagnosis only.

## Next Windows work after first real build

Phase 9A is deliberately large because the Linux Qt UI already contains the
product functionality. The first Windows machine pass should therefore focus on
real platform defects rather than rebuilding GUI features:

- MSVC compile corrections, if any;
- Qt Windows rendering/DPI quirks;
- WASAPI device enumeration/selection;
- actual capture/render stability;
- Windows taskbar/icon behavior;
- filesystem dialogs and Unicode paths;
- recording/export on NTFS;
- packaged execution outside the build tree.

Once those are clean, later Windows work is primarily platform polish,
regression/soak testing and installer/signing rather than feature reimplementation.
