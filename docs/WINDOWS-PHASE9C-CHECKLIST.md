# Windows Phase 9C — Completion / Release Checklist

Phase 9C is the final Windows regression/release phase. It adds no emulator
features and does not change FV1SDK ABI.

## Automated release gate

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\tools\phase9c-windows-release-gate.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

Expected:

```text
=== PHASE 9C AUTOMATED WINDOWS RELEASE GATE PASSED ===
```

Optional real playback hardware: add `-RunHardware`.

Optional capture → FV-1 → playback: add `-RunLiveInput`. Avoid acoustic
feedback during live-input testing.

## Manual closure

Verify:

- Open Program / Recent / drag-drop / command line / Paste SpinASM all preserve
  the manual Start invariant.
- Every bundled `.spn` loads and runs.
- Test Generator, WAV Loop and Audio Interface.
- Oscilloscope, Spectrum, Spectrogram and Levels.
- Raw / Processed / Raw+Processed overlay.
- DSP bypass, FFT choices, Delay RAM, resource view, offline Inspector.
- Validation and all report outputs.
- Processed, Raw and Raw+Processed recording/export.
- No `.partial-*` debris after successful finalization.
- At least 30 minutes of representative realtime audio.
- At least 100 Start/Stop cycles.
- Endpoint unplug/disable/reconnect recovery.
- Unicode and >260-character path regression.
- 100/125/150/200% DPI regression.
- Mixed-DPI monitor movement where hardware permits.
- Splash/About/dialog/menu/taskbar/Alt-Tab visual identity.

## Release artifacts

The gate must produce:

```text
dist\windows\FV1Lab-<version>-windows-x64.zip
dist\windows\FV1Lab-<version>-windows-x64.zip.sha256
dist\windows\FV1Lab-<version>-windows-x64.zip.manifest.json
```

The manifest records product, version, exact commit, Qt version, compiler,
architecture, audio backend, ZIP filename and SHA-256.

## Clean-machine test

Copy only the ZIP to a Windows 11 machine without the Qt SDK/repository.
Extract and launch `bin\FV1Lab.exe`.

Verify splash, GUI, program loading, Test Generator, WAV Loop, Audio Settings,
analyzers, About and clean exit. Test real hardware if available.

## Version policy

The project currently defaults to `1.0.0-rc1`.

Do not silently remove `rc1`. Final version promotion is a separate explicit
checkpoint after this release candidate passes.

## Commit rule

Commit this 9C release-gate checkpoint after the automated gate passes.
Do final documentation/version/tag/release closure as a separate commit.


## Phase 9C.1 RC torture tooling

After the automated Release gate checkpoint is committed, run:

```powershell
.\tools\windows-phase9c-rc-torture.ps1 -Quick
.\tools\windows-phase9c-rc-torture.ps1
```

Then perform clean-machine artifact verification with:

```text
tools\windows-phase9c-clean-machine.ps1
```

Detailed procedure:

```text
docs\WINDOWS-PHASE9C1-RC-ACCEPTANCE.md
```

Keep final `1.0.0` version promotion separate from the RC torture checkpoint.
