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

Exit test: all unit tests pass and all eight project example effects compile, execute and render
finite non-silent audio on Linux.

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

## Phase 4 — Emulator / testbench completion — CURRENT

Preserve the current GUI and deepen the instrument/testbench workflow:

- simultaneous raw-input and processed-output analyzer overlays;
- proper scope controls: time zoom, vertical gain, trigger source/level/slope/mode, freeze and single-shot;
- improved spectrum controls: log/linear axis, dB range, peak hold and better dominant-frequency interpolation;
- spectrogram history and dynamic-range controls;
- polished WAV-loop transport: play, pause, stop, seek, loop region and configurable boundary crossfade;
- expanded test-generator settings for sine, sweep, noise and impulse stimuli;
- realtime-safe recording of raw, processed, or both streams to WAV;
- plot image copy/save and CSV export;
- reusable `fv1-debugger` library plus a deliberately **offline** virtual-chip inspector in the GUI;
- register and Delay RAM inspection without racing the realtime engine;
- expanded static resource reporting and dynamic testbench status;
- permanent `© 2026 Roth Amplification LTD` footer;
- keep useful right-click context menus as first-class engineering shortcuts.

IDE-like scope is intentionally limited here. There is no project/source tree, refactoring system,
multi-file editor workflow, or full IDE project management in this phase.

## Future dedicated FV-1 IDE — SEPARATE APPLICATION

A future IDE may consume:

- `fv1-core`;
- `fv1-runtime`;
- `fv1-analysis`;
- `fv1-audio`;
- `fv1-debugger`;
- the SpinASM compiler/loader boundary.

That future application can provide source editors, source-mapped breakpoints, project management,
EEPROM-bank authoring and deeper code-centric workflows without turning this emulator/testbench
into the IDE itself.

## Phase 5 — Hardware validation and later platforms

Validate the virtual chip against physical FV-1 hardware using identical deterministic stimuli and
synchronized interface capture. Quantify waveform residual, frequency/phase response, delay timing,
modulation timing, POT behavior and instruction edge cases.

After the shared APIs are mature:

- Windows desktop frontend;
- native macOS shell using SwiftUI + Metal/MetalKit + CoreAudio/AVAudioEngine;
- both reuse the same platform-neutral libraries.

DAW/VST/AU/CLAP plugins are **not part of this standalone application project**. If desired later,
they should be separate consumers of the emulator libraries.
