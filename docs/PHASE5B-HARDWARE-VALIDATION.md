# Phase 5B — Physical FV-1 validation preparation

Phase 5B converts the Phase-5A comparison engine into a repeatable physical-chip lab workflow.
Actual silicon acceptance remains pending until an FV-1 fixture and capture interface are connected.

## Hardware validation pack

Generate from the GUI with **VALIDATION → Generate Hardware Test Pack…** or from the CLI:

```bash
./build/fv1-cli validation-pack /tmp/fv1-hardware-pack \
  --host-rate 48000 --seconds 5 --level 0.25
```

The pack contains deterministic impulse, multitone, logarithmic sweep, 1 kHz sine, white-noise and
pink-noise WAVs, plus `manifest.json` and a fixture-workflow README. Do not normalize, limit, trim,
resample or otherwise alter the stimulus/capture files between the virtual and physical runs.

For each stimulus:

1. load the same FV-1 program in the emulator and physical fixture;
2. record the virtual-clock, host rate and POT0/1/2 settings;
3. render the untouched stimulus through the emulator;
4. play the same untouched stimulus through the physical FV-1 board;
5. capture the board output at the manifest host rate;
6. compare emulator/reference and physical capture with `fv1-cli validate` or the VALIDATION tab;
7. preserve the report bundle and original captures as a regression fixture.

## Paste SpinASM scratchpad

**File → Paste SpinASM…** provides a deliberately small convenience surface for the standalone
emulator. It uses the same `tools/fv1_assembler.py` compiler and the same 512-byte program-image
path as a `.spn` file. Compilation errors remain in the dialog with source line numbers. Successful
pasted programs immediately feed resource analysis, the offline chip inspector and realtime sessions.
This is not a project editor and does not replace the future dedicated FV-1 IDE.

## Startup and desktop polish

The startup splash is rendered by Qt rather than stored as a full-screen bitmap. Qt paints the
panel, standalone FV-1 typography, waveform, DIP package, product wording, status text and progress
bar. The application icon is deliberately **not** reused as the splash hero artwork. Phase 5B introduced
the optional background-image hook; Phase 6C now supplies `assets/splash/FV1LabSplashImagebase.png` as
the default black-and-white photograph. It is full-bleed scaled/cropped, accent-tinted and darkened behind
the same foreground composition without redesigning the splash. Progress values come from real initialization milestones;
a short minimum display interval prevents the splash from flashing by on fast systems.

Menu-bar entries and dropdown menu rows use the active application accent color for their standard
hover/selection state, with theme-appropriate contrasting text. Changing the accent updates these
menu interaction states through the same global theme stylesheet.

The four approved application icons are not redesigned. Only pixels outside their existing rounded
badge border receive PNG transparency. Standard freedesktop icon sizes are generated from the
corrected Silver master, and the application declares its desktop-file identity for Linux dock
matching.

## Physical acceptance gate

The emulator must not claim silicon-equivalent accuracy until real measurements quantify at least:

- end-to-end ADC/DAC/board latency;
- gain, correlation and waveform residual;
- frequency-magnitude and phase response;
- delay-RAM timing/precision behavior;
- SIN/RAMP LFO timing and CHO corner cases;
- POT quantization/hysteresis;
- clipping/saturation behavior;
- LOG/EXP and instruction edge cases;
- 32.768 kHz and crystal-derived ~46.6084 kHz operation where applicable.
