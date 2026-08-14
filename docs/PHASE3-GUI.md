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
- large console, debugger and memory/register panels
- offscreen GUI construction smoke test in CTest

Instruction-level debugger stepping remains intentionally offline-only while realtime audio is active. The next increment will add a dedicated offline debug session, live compiler diagnostics, loop-region editing, and more detailed delay/register visualization without introducing races into the audio thread.
