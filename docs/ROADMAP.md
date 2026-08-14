# Spin FV-1 Emulator Roadmap

## Project constraint

Development is **Linux-first**. Do not spend Phase 1–3 engineering time maintaining Windows or macOS application ports. Keep platform boundaries clean so those ports can be added after the emulator/runtime has matured.

The target application GUI is the wide engineering-dashboard concept established for the project: program/parameter controls at left, scopes/analyzers and delay/resource views in the center, and a large vertically stacked Console + Step Debugger side panel. The future GUI must support Dark, Light and additional configurable color themes rather than hard-coded colors.

## Phase 1 — Emulator foundation — COMPLETE

Deliverables:

- zero-GUI `libfv1-core`;
- C API + C++ wrapper;
- FV-1 instruction decoder/interpreter;
- fixed-point state, delay RAM and LFOs;
- assembler/program loader;
- instruction debugger primitives;
- resource analyzer;
- deterministic `fv1-cli`;
- offline WAV rendering at the virtual rate;
- Linux CMake/CTest CI and regression vectors.

Exit test: all unit tests pass, all eight project example effects compile, execute and render finite non-silent audio on Linux.

## Phase 2 — Realtime audio, file loops and analysis — ACCEPTED / CAPTURE TEST DEFERRED

Add the reusable runtime around `libfv1-core`:

- miniaudio Linux device backend initially;
- host audio-interface capture/playback;
- an `AudioSource` abstraction with Live Input, File Loop and Test Generator implementations;
- WAV import in the initial Linux runtime; broader formats remain an extension point;
- loop entire file or a selected region;
- seamless loop-region wrapping; optional crossfade controls follow with the GUI;
- explicit virtual FV-1 clock independent of host interface rate;
- SpeexDSP host↔FV-1 SRC, with a deterministic linear build fallback;
- lock-free SPSC audio/telemetry queues;
- background spectrum/measurement engine behind a swappable FFT backend;
- scope data, meters, correlation and runtime/xrun statistics.

Exit status: automated clock/source/analyzer tests pass; production SpeexDSP/miniaudio playback and looped-file processing are accepted on Cortana. Guitar/line capture through an external duplex interface remains a documented deferred acceptance test until hardware is available.

## Phase 3 — Linux Qt application — CURRENT

Build the full Linux desktop application on the stable Phase-1/2 libraries:

- Qt 6 dockable interface matching the latest approved mockup;
- source selector: LIVE / FILE LOOP / TEST;
- transport and loop-region controls;
- POT0/1/2 and global controls;
- oscilloscope, spectrum and spectrogram;
- Delay RAM Viewer;
- Virtual DSP Resource Usage panel;
- large readable Console and Step Debugger stacked in a resizable side panel;
- register/memory inspector;
- Dark, Light, Midnight, Amber CRT, Green Phosphor, Slate and High Contrast themes;
- independent accent-color selection;
- semantic theme palette rather than hard-coded widget colors;
- DAW-style audio preferences dialog;
- realtime DSP/FX bypass with explicit raw-signal scope/analyzer monitoring;
- right-click context menus for high-value local engineering actions.

## Phase 4 — Integrated FV-1 development environment

Add:

- SpinASM editor and syntax highlighting;
- compile diagnostics mapped to source lines;
- live recompile/reload;
- source breakpoints;
- instruction and sample stepping;
- register watches;
- delay-memory inspection;
- eight-slot bank management/export;
- reproducible project/test-session files;
- compare-two-runs tools.

## Phase 5 — Hardware validation and later platforms

First validate the virtual chip against real FV-1 hardware using synchronized interface capture and deterministic stimuli. Quantify differences in impulse/frequency response, modulation, delay timing, POT behavior and instruction edge cases.

Only after the shared APIs are mature:

- Windows application port;
- native macOS shell using SwiftUI + Metal/MetalKit + CoreAudio/AVAudioEngine;
- both reuse the same platform-neutral emulator/runtime/analysis libraries.

DAW/VST/AU/CLAP plugins are **not part of this standalone application project**. If desired later, they should be separate consumers of the core library.

## Current status — Phase 3 operational on Cortana

Phase 2 is accepted on Linux for generator/file-loop playback, production SpeexDSP clock bridging, analyzer telemetry and real miniaudio playback. External capture/duplex interface validation is deferred until hardware is available.

Phase 3 is operational on Cortana with Qt 6.4.2: the approved dashboard launches, programs load and analyze, test-generator and file-loop sessions run through the Phase-2 runtime, plots consume live analyzer data, themes switch, and the offscreen GUI smoke test passes. The current refinement adds DAW-style Audio Settings, DSP/FX bypass/raw monitoring, and task-local context menus.

The next larger Phase-3 increment is the dedicated offline debugger session, richer delay/register visualization, loop-region editing, and source/compiler diagnostics.

