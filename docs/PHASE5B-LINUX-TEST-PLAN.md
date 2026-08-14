# Phase 5B Linux acceptance plan

## Automated gate

A production Linux build should pass all existing tests plus `phase5b-validation-pack-cli` and the
Qt `fv1-lab-smoke` test. Headless builds contain 14 tests; a Qt-enabled workstation should contain
15 tests.

## GUI acceptance

1. Start `fv1-lab`. Confirm the software-rendered splash remains visible briefly, advances through
   meaningful status messages, reaches 100%, then reveals the main window. Confirm the splash uses
   standalone FV-1/waveform/DIP artwork rather than a large application-icon badge, and that the
   background remains the intentional dark/blank surface until a future collage is supplied.
2. Open each menu from the menu bar and move the pointer across top-level and dropdown items. Confirm
   hovered/selected entries use the currently selected application accent color with readable
   contrasting text; change the accent and confirm the menu highlight follows it immediately.
3. Confirm the selected FV-1 application icon has transparent pixels outside its existing rounded
   badge and no square black canvas in the Pop!_OS/GNOME dock or application switcher.
4. Use **File → Paste SpinASM…**, paste `examples/simple_passthrough.spn`, and choose
   **Compile & Load**. Confirm success reports 4 / 128 instructions and the normal PROGRAM,
   resource and inspector views update.
5. Reopen **Paste SpinASM…** and enter an invalid mnemonic. Confirm the dialog stays open and shows
   a concise line-numbered compiler error rather than a Python traceback.
6. Start a Test Generator session with the pasted program and verify the realtime path treats it the
   same as a program opened from a `.spn` file.
7. In **VALIDATION**, choose **Generate Hardware Test Pack…** and create a short pack. Confirm six
   WAVs, `manifest.json`, and `README.txt` are produced.
8. Use one generated WAV as both reference and capture. Confirm the Phase-5A null comparison still
   reports PASS, zero delay, unity correlation and numerical-floor residual.

## Physical acceptance (pending hardware)

Route the generated pack through the physical FV-1 fixture and compare captures with emulator
renders using identical program, clock, POT and host-rate settings. Preserve raw captures and report
bundles. Physical results are the gate for claiming silicon-level accuracy.
