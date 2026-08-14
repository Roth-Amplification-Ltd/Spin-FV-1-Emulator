# Phase 4 — Emulator / Testbench Completion

Phase 4 deliberately keeps **Spin FV-1 Emulator** focused on emulation, measurement and virtual-chip
inspection. It adds instrument-grade controls to the existing Phase-3 dashboard without converting
the application into a full source-code IDE.

## Signal comparison

The realtime host maintains two analyzer taps:

```text
selected source ───────► RAW analyzer ───────────────┐
        │                                             │
        └──► host→FV-1 SRC → virtual FV-1 → SRC ─► PROCESSED analyzer
                                                      │
                                                      ▼
                                              testbench displays
```

The oscilloscope and spectrum can display raw and processed traces simultaneously. The existing
DSP/FX bypass still routes raw source audio directly to output; the overlay is independent of bypass
and is intended for A/B measurement.

## Oscilloscope

The scope supports:

- raw + processed overlay;
- time zoom;
- vertical gain;
- trigger Off / Auto / Normal / Single;
- left/right trigger source;
- rising/falling trigger slope;
- adjustable trigger level;
- single-shot re-arm;
- freeze and clear;
- image copy/save and CSV sample export.

Triggering is a display/testbench function and never stalls the realtime audio path.

## Spectrum and spectrogram

Spectrum controls include log/linear frequency display, configurable dB range, peak hold, freeze,
raw/processed comparison and CSV export. Dominant-frequency telemetry uses interpolated neighboring
FFT bins so stable test tones report much closer to their true frequency than simple largest-bin
reporting.

The spectrogram retains configurable history columns and shares the analyzer dB-range controls.

## Audio-file test workflow

`FileLoopSource` remains the same runtime source used by the headless host. The GUI adds:

- play / pause / stop;
- seek position;
- whole-file or selected loop region;
- configurable loop-boundary crossfade;
- realtime-safe loop state changes while the source is active.

This is the preferred repeatable FX-testing workflow: record or import one performance and loop the
identical input while changing FV-1 programs or POT values.

## Recording and export

`AudioRecorder` is GUI-independent and realtime-safe. The callback only copies frames to fixed-size
SPSC queues; a background writer thread performs filesystem I/O. It can record:

- processed output;
- raw input/source;
- raw and processed simultaneously as two stereo 32-bit float WAV files.

The GUI also supports copying/saving analyzer images and exporting the current plot data to CSV.

## Offline virtual-chip inspector

`fv1-debugger` owns a private `fv1-core` instance, completely separate from the realtime audio engine.
The right-side inspector can therefore:

- reset the virtual chip;
- step one FV-1 instruction;
- complete one virtual sample;
- inspect PC, instruction/opcode, ACC, PACC, LR, ADDR_PTR and LFO state;
- inspect REG0–REG31;
- inspect a physical Delay RAM window around the current circular delay pointer.

This is intentionally a **chip inspector**, not a source-code IDE. The reusable debugger library is
kept GUI-independent so a future dedicated IDE can build richer source-level tooling on top of it.

## Product footer

The status bar permanently displays:

`© 2026 Roth Amplification LTD`

at the bottom-right of the application window across all built-in themes.
