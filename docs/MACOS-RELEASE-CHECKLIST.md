# FV-1 Lab macOS Release Checklist

Use this checklist only after `tools/macos-phase8d-gate.sh` passes.

## Automated gate

- [ ] `git diff --check` passes.
- [ ] Host CTest suite passes.
- [ ] Apple frontend boundary check passes.
- [ ] All nine shipped SpinASM regression programs compile/load/execute.
- [ ] Accelerated Apple realtime bridge soak passes.
- [ ] macOS Debug build succeeds.
- [ ] macOS Release build succeeds.
- [ ] Release app bundle verification passes.
- [ ] Ad-hoc or Developer ID DMG packaging succeeds.

## Startup and application shell

- [ ] Cold launch shows the splash before the main dashboard; no dashboard flash.
- [ ] Splash progress advances smoothly and reaches Ready.
- [ ] Main window opens at the approved three-column desktop geometry.
- [ ] Help → Show Startup Splash replays without hiding/restarting the dashboard.
- [ ] Help → About FV-1 Lab opens correctly.
- [ ] App quits and relaunches cleanly.

## Program workflow regression

- [ ] File → Open FV-1 Program opens a native macOS file panel.
- [ ] Toolbar Open Program works.
- [ ] `.spn` program compiles and loads.
- [ ] Raw 512-byte FV-1 image loads.
- [ ] File → Paste SpinASM compiles clipboard text.
- [ ] Loading/reloading a program leaves audio STOPPED until Start is pressed.
- [ ] Test all eight `examples/steal-this-dsp-programs/*.spn` programs manually.
- [ ] Test `examples/simple_passthrough.spn`.

## Audio sources

### Test Generator

- [ ] Sine.
- [ ] Sweep.
- [ ] White noise.
- [ ] Pink noise.
- [ ] Impulse.
- [ ] Generator Settings persists values.
- [ ] Start/Stop repeatedly without route failure.

### Audio Interface

- [ ] Audio Settings enumerates devices.
- [ ] OS Default device works.
- [ ] Explicit duplex device works.
- [ ] Aggregate Device works when separate physical I/O hardware is required.
- [ ] Preferred sample rate persists.
- [ ] Preferred buffer size persists.
- [ ] Route change/recovery is clean.

### Audio File Loop

- [ ] File → Open Audio Loop works.
- [ ] Play / Pause / Stop.
- [ ] Seek.
- [ ] Loop enable/disable.
- [ ] Loop begin/end.
- [ ] Crossfade.
- [ ] Switching away from File Loop and back remains stable.

## Testbench/analyzers

- [ ] Scope Raw + Processed overlay.
- [ ] Scope gain/time zoom.
- [ ] Trigger Off / Auto / Normal / Single.
- [ ] Trigger channel and slope.
- [ ] Spectrum raw/processed overlay.
- [ ] Spectrum log/linear frequency.
- [ ] Spectrum dB floor.
- [ ] Spectrum peak hold.
- [ ] Spectrogram scrolling/history/freeze/clear.
- [ ] Levels peak/RMS/correlation/dominant-frequency/drop counters.
- [ ] Global Freeze / Unfreeze All Plots.
- [ ] Global Clear Analyzer Displays.
- [ ] FFT 1024.
- [ ] FFT 2048.
- [ ] FFT 4096.
- [ ] FFT 8192.
- [ ] Scope CSV export.
- [ ] Spectrum CSV export.

## DSP bypass and recording

- [ ] DSP ON produces processed output.
- [ ] DSP BYPASS audibly routes raw input.
- [ ] Processed analyzers remain active while bypassed.
- [ ] Processed recording.
- [ ] Raw recording.
- [ ] Raw + Processed recording.
- [ ] Recorded WAV files reopen/play correctly.
- [ ] Recorder drop counters remain zero in normal operation.

## Inspector and Delay RAM

- [ ] Step Instruction.
- [ ] Step Sample.
- [ ] PC/opcode/raw instruction update.
- [ ] ACC / PACC / LR / ADDR_PTR update.
- [ ] LFO state updates.
- [ ] REG0–31 visible.
- [ ] Delay pointer visible.
- [ ] Center Delay RAM on pointer.
- [ ] Jump to Delay RAM address.
- [ ] Delay RAM values update while stepping.
- [ ] Reset returns offline inspector to a sane state.

## Validation

- [ ] Reference WAV selection.
- [ ] Capture WAV selection.
- [ ] PASS comparison.
- [ ] Deliberately mismatched capture produces FAIL.
- [ ] JSON report.
- [ ] Markdown report.
- [ ] frequency CSV.
- [ ] residual WAV.
- [ ] deterministic validation pack generation.

## Appearance

### Themes

- [ ] Dark.
- [ ] Light.
- [ ] Midnight.
- [ ] Amber CRT.
- [ ] Green Phosphor.
- [ ] Slate.
- [ ] High Contrast.

### Accents

- [ ] Cyan.
- [ ] Blue.
- [ ] Green.
- [ ] Amber.
- [ ] Orange.
- [ ] Red.
- [ ] Purple.
- [ ] Magenta.

### Application icon

- [ ] Silver.
- [ ] Dark Cyan.
- [ ] Blue.
- [ ] Amber.
- [ ] Icon choice survives relaunch.

## Real Core Audio live soak

Run at least one candidate soak with a real audio route. Thirty minutes is the
minimum useful acceptance run; longer is preferred.

- [ ] Test Generator source runs continuously for at least 30 minutes.
- [ ] Audio Interface source runs continuously for at least 30 minutes.
- [ ] Audio File Loop source runs continuously for at least 30 minutes.
- [ ] Change POT values periodically.
- [ ] Toggle DSP bypass periodically.
- [ ] Cycle analyzer tabs.
- [ ] Freeze/unfreeze and clear analyzers.
- [ ] Record at least one multi-minute WAV.
- [ ] No crash.
- [ ] No UI lockup.
- [ ] No uncontrolled memory growth.
- [ ] No sustained underflow/overflow growth under a sane device/buffer setup.
- [ ] No non-finite meter/analyzer values.
- [ ] Stop and restart audio successfully after the soak.

## Distribution

### Ad-hoc/internal

- [ ] `./tools/package-macos.sh adhoc`
- [ ] DMG mounts.
- [ ] Applications symlink is present.
- [ ] App copies to `/Applications`.
- [ ] App launches from `/Applications`.
- [ ] SHA-256 file matches the DMG.

### Public Developer ID release

- [ ] Correct `FV1_CODESIGN_IDENTITY`.
- [ ] Hardened-runtime app signing succeeds.
- [ ] DMG signing succeeds.
- [ ] `xcrun notarytool submit --wait` succeeds.
- [ ] Notarization ticket staples.
- [ ] `xcrun stapler validate` succeeds.
- [ ] Gatekeeper accepts the distributed artifact on a clean Mac.

## Completion declaration

Only after every applicable item above passes:

- [ ] Mark macOS FV-1 Lab feature-complete.
- [ ] Tag the release candidate/final release.
- [ ] Publish the notarized DMG and SHA-256.
