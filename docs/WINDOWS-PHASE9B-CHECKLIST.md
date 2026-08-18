# FV-1 Lab Windows Phase 9B Acceptance Checklist

Run this checklist against the **portable Release package**, preferably after
`tools/phase9b-windows-gate.ps1` passes.

## Automated gate

- [ ] Windows CTest suite passes.
- [ ] normal Qt smoke passes.
- [ ] HiDPI smoke passes.
- [ ] splash smoke passes.
- [ ] About smoke passes.
- [ ] command-line `.spn` open smoke passes.
- [ ] Release build succeeds.
- [ ] `windeployqt` succeeds.
- [ ] portable ZIP verifier succeeds without Qt development environment.

## Startup / workspace

- [ ] Cold launch shows splash before dashboard.
- [ ] Dashboard opens at a useful size.
- [ ] Move/resize main window, quit, relaunch: geometry is restored.
- [ ] Rearrange docks, quit, relaunch: dock layout is restored.
- [ ] View -> Reset Workspace Layout restores the default dock arrangement.
- [ ] Theme persists.
- [ ] Accent persists.
- [ ] selected application icon persists.
- [ ] source mode persists.
- [ ] generator kind/frequency persists.
- [ ] POT0/POT1/POT2 persist.

## Program workflow

- [ ] Ctrl+O opens a native Windows file dialog.
- [ ] `.spn` opens and compiles.
- [ ] 512-byte `.bin` opens.
- [ ] Paste SpinASM works.
- [ ] loaded program does not auto-start realtime audio.
- [ ] Open Recent Program menu records successful program loads.
- [ ] missing recent files are removed cleanly.
- [ ] Clear Recent Programs works.
- [ ] drag a `.spn` onto FV-1 Lab and verify it loads.
- [ ] drag a `.bin` onto FV-1 Lab and verify it loads.
- [ ] run `FV1Lab.exe path\to\program.spn` and verify it loads while STOPPED.

## Audio-file workflow

- [ ] Open Audio Loop uses a native Windows file dialog.
- [ ] WAV loop loads.
- [ ] recent audio-loop menu updates.
- [ ] drag a `.wav` onto FV-1 Lab and verify Audio File Loop is selected.
- [ ] run `FV1Lab.exe path\to\audio.wav` and verify the loop loads without
      auto-starting the global audio session.
- [ ] Play / Pause / Stop.
- [ ] Seek.
- [ ] Loop enable/disable.
- [ ] Loop region.
- [ ] Crossfade.

## WASAPI

- [ ] Audio Settings reports `WASAPI via miniaudio`.
- [ ] OS Default playback appears.
- [ ] OS Default capture appears.
- [ ] explicit playback endpoints appear.
- [ ] explicit capture endpoints appear.
- [ ] Refresh Audio Devices preserves a still-connected selected endpoint.
- [ ] unplug/replug or disable/enable an endpoint and Refresh does not crash.
- [ ] Test Generator starts/stops repeatedly.
- [ ] Audio Interface starts/stops repeatedly.
- [ ] real capture -> FV-1 -> playback path works.
- [ ] 44.1 kHz.
- [ ] 48 kHz.
- [ ] at least two buffer sizes.
- [ ] no sustained analyzer drops under sane settings.

## Analyzer / recorder regression

- [ ] Scope raw + processed.
- [ ] Spectrum raw + processed.
- [ ] Spectrogram.
- [ ] Levels.
- [ ] FFT 1024 / 2048 / 4096 / 8192.
- [ ] DSP bypass.
- [ ] raw recording.
- [ ] processed recording.
- [ ] raw + processed recording.
- [ ] analyzer image export.
- [ ] analyzer CSV export.

## Unicode and path handling

Use a directory with non-ASCII characters, for example:

```text
C:\Users\<user>\Desktop\FV1-Ünicode-測試\
```

- [ ] open `.spn` from Unicode path.
- [ ] open `.wav` from Unicode path.
- [ ] record WAV to Unicode path.
- [ ] export analyzer CSV/image to Unicode path.
- [ ] validation report export to Unicode path.
- [ ] logs display paths correctly.

## DPI / Windows desktop behavior

- [ ] 100% display scaling.
- [ ] 125% or 150% display scaling.
- [ ] move window between monitors with different scaling if available.
- [ ] no clipped controls after per-monitor DPI transition.
- [ ] taskbar/window icon appears correctly.
- [ ] Alt+Tab icon appears correctly.
- [ ] maximize/restore.
- [ ] minimize/restore.

## Portable package

- [ ] extract ZIP outside repository.
- [ ] launch without Qt Creator.
- [ ] launch without Visual Studio.
- [ ] launch from a shell whose current directory is not the repository.
- [ ] splash assets present.
- [ ] icon assets present.
- [ ] command-line `.spn` open works from portable package.
- [ ] normal Test Generator session works from portable package.
- [ ] recording/export works from portable package.

## Phase completion

After every applicable item passes:

- [ ] commit Phase 9B.
- [ ] push `main`.
- [ ] proceed to Phase 9C Windows Completion / Release.
