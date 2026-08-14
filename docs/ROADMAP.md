# Spin FV-1 Emulator Roadmap

## Product definition

**Spin FV-1 Emulator is first and foremost a standalone FV-1 emulator and electronic testbench.**
It should feel like a useful virtual DSP instrument: load a program, feed it a live interface,
repeatable audio loop, or generated stimulus, and inspect what the virtual chip is doing.

Some IDE-like conveniences are welcome when they directly improve emulation or debugging (for
example instruction stepping, register inspection, disassembly, or convenient `.spn` loading), but
this application is **not intended to become the future full FV-1 IDE**. The dedicated IDE will be
a separate application that reuses the platform-neutral libraries developed here.

Development remains **Linux-first** while the core/runtime APIs mature. Keep platform boundaries
clean so Windows and a native macOS frontend can be added later without changing the chip model.

The approved GUI identity is the existing wide engineering dashboard: program/source/parameter
controls at left; scope, spectrum, spectrogram, delay/resource/status instrumentation in the center;
and a large vertically stacked Console + virtual-chip Inspector on the right. Refine this layout
rather than replacing it.

## Phase 1 — Emulator foundation — COMPLETE

- zero-GUI `libfv1-core`;
- C API + C++ convenience wrapper;
- FV-1 instruction decoder/interpreter;
- fixed-point state, delay RAM and LFOs;
- SpinASM-compatible assembler/program loader;
- instruction-debug primitives and snapshots;
- resource analyzer;
- deterministic `fv1-cli` and offline WAV rendering;
- Linux CMake/CTest regression suite.

## Phase 2 — Realtime audio, file loops and analysis — ACCEPTED / CAPTURE TEST DEFERRED

- `fv1-runtime` host-rate ↔ virtual-FV-1 clock bridge;
- SpeexDSP production SRC with fractional virtual-clock support;
- miniaudio Linux device backend;
- interchangeable Live Input, File Loop and Test Generator sources;
- WAV import and repeatable looping;
- background analysis worker, scope data, FFT, meters and correlation;
- lock-free realtime telemetry;
- `fv1-live` headless realtime host.

Generator/file-loop playback, production SpeexDSP clock bridging, analyzer telemetry and real
miniaudio playback were accepted on Cortana. External capture/duplex-interface validation remains
a documented deferred acceptance test until suitable hardware is available.

## Phase 3 — Qt standalone testbench — COMPLETE

- Qt 6 dashboard matching the approved layout;
- LIVE / FILE LOOP / TEST source selection;
- POT0/1/2 and virtual-clock controls;
- oscilloscope, spectrum, spectrogram and levels;
- Delay RAM, Resource Usage and DSP Status panels;
- large Console / Inspector side panel;
- Dark, Light, Midnight, Amber CRT, Green Phosphor, Slate and High Contrast themes;
- independent accent selection;
- DAW-style Audio Settings dialog;
- persistent `QSettings` preferences;
- realtime **DSP/FX ON — PROCESSED / DSP/FX BYPASS — RAW** monitoring;
- task-local right-click context menus.

The Phase-3/3.1 build is accepted on Cortana and documented with screenshots and a screencast.

## Phase 4 — Emulator / testbench completion — COMPLETE

- simultaneous raw-input and processed-output analyzer overlays;
- scope time zoom, gain, trigger source/level/slope/mode, freeze and single-shot;
- spectrum log/linear axis, dB range, peak hold and interpolated dominant frequency;
- spectrogram history and dynamic-range controls;
- WAV-loop play/pause/stop/seek, loop region and boundary crossfade;
- expanded test-generator controls;
- realtime-safe raw/processed WAV recording;
- plot image copy/save and CSV export;
- reusable `fv1-debugger` plus offline virtual-chip inspector;
- register/Delay-RAM inspection and expanded resource reporting;
- permanent `© 2026 Roth Amplification LTD` footer.

Phase 4 passed 11/11 tests on Cortana and was committed as `Complete Phase 4 FV-1 emulator testbench`.

## Phase 5A — Validation framework — COMPLETE

Build the measurement framework before physical hardware is available so the same tools can be
self-tested against synthetic captures and later used unchanged with real FV-1 recordings.

- reusable GUI-independent `fv1-validation` library;
- deterministic validation stimulus WAV generation (multitone, sweep, sine, white/pink noise, impulse);
- reference/capture WAV loader supporting common PCM/float formats;
- automatic capture time alignment by normalized correlation;
- capture latency in frames and milliseconds;
- raw gain error plus optional gain matching **only for residual/SNR measurements**;
- per-channel correlation, residual RMS/peak and SNR;
- FFT-based magnitude-error and phase-error measurements on active reference bins;
- configurable acceptance thresholds and PASS/FAIL output;
- residual-audio generation;
- JSON, Markdown and frequency-CSV report export;
- `fv1-cli stimulus` and `fv1-cli validate` commands;
- FV-1 Lab **VALIDATION** workspace using the same library;
- regression tests using synthetic captures with deliberately known delay/gain/error;
- four switchable FV-1 Emulator application icons plus Linux desktop metadata.

Phase-5A exit test passed: identical reference/capture files produced zero delay, unity correlation
and numerically silent residual; synthetic delayed/gain-scaled captures recovered their planted
values; the software validation/report pipeline is accepted.

## Phase 5B — Physical FV-1 silicon validation — CURRENT / SILICON MEASUREMENT PENDING

The Phase-5B software-preparation pass adds the fixture workflow needed before physical captures:

- deterministic multi-file hardware-validation packs plus machine-readable manifests;
- `fv1-cli validation-pack` and matching GUI generation workflow;
- File → **Paste SpinASM…** scratchpad that compiles through the same assembler/program-image path;
- software-rendered startup splash with dedicated FV-1/waveform/DIP foreground, real startup milestones,
  progress percentage, and an optional future theme-tinted monochrome background-image hook;
- Linux application identity hardening and correctly transparent rounded-badge icon corners;
- Pop!_OS/Ubuntu 22.04+ bootstrap compatibility, including pinned miniaudio fallback handling.

Actual silicon acceptance remains pending until an FV-1 board and capture interface are connected.
At that point, use the generated pack unchanged for both virtual and physical runs.

When the audio interface and FV-1 hardware rig are available, feed **the exact same deterministic
stimulus** to the emulator and the physical board and capture both at one host sample rate.

Quantify and document:

- ADC/DAC and board latency;
- waveform residual and correlation;
- magnitude/phase response;
- delay-RAM timing and precision behavior;
- SIN/RAMP LFO timing and CHO behavior;
- POT quantization/hysteresis;
- clipping/saturation boundaries;
- LOG/EXP and instruction edge cases;
- crystal-derived rates including 46.6084 kHz behavior.

Use measurements to tighten `libfv1-core`; unknown silicon behavior must remain explicitly labeled
until measured rather than being replaced with invented precision.

## Future dedicated FV-1 IDE — SEPARATE APPLICATION

A future IDE may consume `fv1-core`, `fv1-runtime`, `fv1-analysis`, `fv1-audio`, `fv1-debugger`,
`fv1-validation`, and the SpinASM compiler/loader boundary. Source editors, source-mapped
breakpoints, project management and EEPROM-bank authoring belong there rather than becoming the
primary identity of this emulator/testbench.

## Later platform frontends

After the shared APIs and silicon model are mature:

- Windows desktop frontend;
- native macOS shell using SwiftUI + Metal/MetalKit + CoreAudio/AVAudioEngine;
- both reuse the same platform-neutral libraries.

DAW/VST/AU/CLAP plugins are **not part of this standalone application project**. If desired later,
they should be separate consumers of the emulator libraries.
