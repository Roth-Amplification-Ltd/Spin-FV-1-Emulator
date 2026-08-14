# Phase 3 — Qt FV-1 Lab GUI

Phase 3 introduces the Linux/Windows desktop frontend while keeping every DSP/audio subsystem independent of Qt.

## Layout target

The GUI intentionally follows the latest approved engineering-dashboard mockup:

- **Left:** FV-1 program, source selection, audio-file loop transport, POT0/1/2, dry/wet, output, audio device and virtual-clock controls.
- **Center:** large tabbed oscilloscope, spectrum, spectrogram and level workspace.
- **Center lower row:** Delay RAM Viewer, Virtual DSP Resource Usage, and DSP Status.
- **Right:** large Console/Log above the Step Debugger, with memory/register inspection below.
- All major side panels are Qt dock widgets or splitters so engineering workspaces remain resizable.

## Theme architecture

Colors are semantic rather than hard-coded into individual controls. Initial built-in themes:

- Dark
- Light
- Midnight
- Amber CRT
- Green Phosphor
- Slate
- High Contrast

Theme and accent are independent. Initial accents are Cyan, Blue, Green, Amber, Orange, Red, Purple and Magenta. The selected theme/accent persist through `QSettings`.

## Current Phase 3 increment

The first increment provides:

- `fv1-lab` Qt 6 executable
- complete dock/layout shell matching the approved arrangement
- theme/accent menus with persistence
- audio-device enumeration through the Phase-2 `AudioHost` backend
- program and loop-file choosers
- POT/global control widgets
- virtual-clock/host-rate/buffer selectors
- analyzer visualization widgets fed by live Phase-2 analyzer snapshots
- resource/delay/status panels
- static FV-1 resource analysis when a program is opened
- real Start/Stop session control for Test Generator, Audio File Loop, and Live Input sources
- live POT0/POT1/POT2 updates against the running virtual chip
- host-rate, period-size, virtual-clock, playback-device and capture-device selection
- DAW-style **Audio Settings** dialog reachable from the Audio menu and toolbar
- persistent audio settings, including SpeexDSP SRC quality
- realtime **DSP/FX bypass** that keeps the device open while routing the raw selected source directly to output/analyzers
- explicit PROCESSED vs RAW INPUT analyzer labeling
- context menus on plots, audio controls, program selection and console/log
- large console, debugger and memory/register panels
- offscreen GUI construction smoke test in CTest

Instruction-level debugger stepping remains intentionally offline-only while realtime audio is active. Phase 4 builds on this accepted GUI with testbench instrumentation, loop transport, raw/processed comparison, recording/export, and a reusable offline chip-inspector library. Full source-editor/IDE workflows remain a separate future application; see `PHASE4-TESTBENCH.md`.

## Audio preferences

`Audio → Audio Settings…` is the canonical preferences surface. It exposes:

- audio backend (currently miniaudio/system audio on Linux);
- playback device;
- capture device;
- host sample rate;
- device period/buffer size;
- virtual FV-1 sample clock;
- SpeexDSP SRC quality.

The left-side Audio / Virtual Clock panel remains a useful quick-status/quick-control surface, while the dialog provides the DAW-style preferences workflow. Settings persist with `QSettings` and apply to the next session if changed while audio is already running.

## DSP/FX bypass and raw monitoring

The transport toolbar has a prominent two-state control:

- **DSP/FX ON — PROCESSED**: the selected source crosses into the virtual FV-1 clock domain, executes the loaded program, returns to the host rate, and feeds output/analyzers.
- **DSP/FX BYPASS — RAW**: the selected source is copied directly to output and analyzer telemetry inside the realtime callback; the audio device remains open.

This makes A/B testing immediate and lets the oscilloscope/spectrum/spectrogram display the raw stimulus without reconfiguring the source. Analyzer titles identify the active monitor path.

## Context-menu policy

Right-click is treated as an engineering shortcut surface rather than decorative UI. Initial context menus include:

- analyzer plots: toggle processed/raw DSP path and clear the display/history;
- audio controls: open Audio Settings or refresh device enumeration;
- program label: open another program or rerun resource analysis;
- Console / Log: standard copy/select actions plus Clear Log and Copy Entire Log.

Future context menus should stay task-local: actions should operate on the object under the pointer instead of becoming duplicate global menus.

## Accepted Linux GUI evidence

The Qt 6.4.2 build on Cortana successfully constructed and ran the complete dashboard against the production SpeexDSP and miniaudio paths. See `PHASE3-ACCEPTANCE.md` and `docs/media/` for the recorded screenshots and short screencast.
