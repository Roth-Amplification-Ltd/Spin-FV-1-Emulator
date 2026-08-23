# Spin FV-1 Emulator Roadmap

> **Status snapshot: August 22, 2026**
>
> The FV-1 execution model and public FV1SDK ABI are treated as locked platform
> boundaries. Current work is finishing the standalone FV-1 Lab desktop
> products, not redesigning the virtual chip.

## Product definition

**Spin FV-1 Emulator is a standalone FV-1 emulator and electronic testbench.**

FV-1 Lab behaves like a virtual DSP instrument: load SpinASM or a program image,
feed the virtual chip from a real audio interface, repeatable WAV loop, or
deterministic test generator, and inspect/measure the virtual FV-1.

Debugger/inspection conveniences belong here when they directly support
emulation and measurement. A future full source-code IDE remains a separate
application that consumes the public SDK.

DAW/VST/AU/CLAP plugins are **not part of this standalone application project**.

## Current platform status

| Platform | Frontend | Audio | Status |
|---|---|---|---|
| Linux | Qt 6 Widgets | miniaudio + system backend | **Feature-complete reference desktop** |
| macOS | Native SwiftUI | Core Audio / AVAudioEngine | **Phase 8D complete** |
| Windows 11 | Same Qt 6 Widgets frontend as Linux | miniaudio → WASAPI | **Phase 9C final release / 1.0.0** |

Linux and Windows intentionally share the same Qt desktop frontend. macOS is a
native SwiftUI product consuming the same platform-neutral FV-1 SDK boundary.

## Completed foundation — Phases 1 through 6C

### Phase 1 — Emulator foundation — COMPLETE

- platform-neutral C++20 virtual FV-1 engine;
- 128-word / 512-byte program loading;
- SpinASM assembler/program loader;
- fixed-point registers, accumulator state, Delay RAM and LFOs;
- instruction stepping/snapshots;
- resource analysis;
- deterministic CLI/offline rendering;
- regression-tested CMake build.

### Phase 2 — Realtime runtime and analysis — COMPLETE

- independent host/FV-1 clock domains;
- SpeexDSP production SRC with deterministic fallback;
- live-input, WAV-loop and test-generator sources;
- miniaudio host audio abstraction;
- lock-free analyzer/runtime telemetry;
- scope/spectrum/levels data paths;
- `fv1-live` headless host.

### Phase 3 — Qt FV-1 Lab desktop — COMPLETE

- wide engineering dashboard;
- program/source/POT/virtual-clock controls;
- oscilloscope, spectrum, spectrogram and levels;
- Delay RAM, resource usage and DSP status;
- Console + virtual-chip Inspector;
- themes, accents, Audio Settings and persistent settings.

### Phase 4 — Testbench completion — COMPLETE

- simultaneous raw/processed analyzers;
- scope trigger/zoom/freeze;
- configurable spectrum/spectrogram;
- WAV-loop transport and crossfade;
- realtime-safe recording;
- image/CSV export;
- reusable debugger and full chip inspection.

### Phase 5A/5B/5C — Validation + model hardening — COMPLETE

- deterministic validation stimuli and packs;
- reference/capture alignment and error metrics;
- JSON/Markdown/CSV/residual-WAV reports;
- hardware-validation workflow;
- independent reference model and differential conformance harness;
- randomized/deterministic model stress.

**Physical FV-1 silicon closure remains intentionally deferred.** Agreement
between the production and reference software models is not presented as proof
of undocumented silicon behavior.

### Phase 6A/6B/6C — FV1SDK extraction and ABI-v1 hardening — COMPLETE

- installable public C ABI;
- C++ convenience wrapper;
- native SpinASM compilation;
- optional debugger/introspection API;
- C/C++/Python/Swift/Rust/Objective-C proving hosts;
- SDK-only builds;
- portability, misuse, malformed-input, stress and installed-consumer gates;
- public ABI separated from GUI/audio/platform implementation.

## Phase 7 — Native Win32 proving frontend — COMPLETE / DIAGNOSTIC

Phase 7 proved Windows could consume the public SDK with native Win32/WASAPI.
It remains an opt-in diagnostic/reference harness.

It is **not** the shipping Windows FV-1 Lab frontend. Phase 9 moved Windows to
the same Qt 6 Widgets desktop frontend used by Linux.

## Phase 8 — Native Apple FV-1 Lab — COMPLETE

The Apple roadmap intentionally remains three broad phases.

### Phase 8B — Mac Testbench Parity — COMPLETE

- Scope;
- Spectrum;
- Spectrogram;
- Levels;
- raw + processed analyzer paths;
- DSP bypass;
- recording/export;
- staged startup splash.

### Phase 8C — Mac Workflow + Inspector Parity — COMPLETE

- Audio File Loop;
- Core Audio device settings;
- persistent audio preferences;
- full offline instruction/sample inspector;
- complete register/LFO state;
- physical circular Delay RAM viewer;
- validation workflow;
- selectable analyzer FFT sizes;
- Linux-equivalent themes, accents and app icons;
- native Open Program / Paste SpinASM workflows.

### Phase 8D — Mac Completion / Release — COMPLETE

Completed at commit `c85e670`:

- shipped-program regression;
- accelerated Apple realtime bridge soak;
- Debug + Release build verification;
- release-safe bundle checks;
- DMG packaging;
- optional Developer ID signing, notarization and stapling workflow;
- final macOS release checklist.

The macOS frontend remains native SwiftUI and does not alter the locked FV-1
execution model or FV1SDK ABI.

## Phase 9 — Windows Qt FV-1 Lab

Windows uses the **same Qt 6 Widgets FV-1 Lab frontend as Linux**, built with
MSVC and backed by miniaudio/WASAPI.

### Phase 9A — Windows Qt Desktop Parity — COMPLETE

- native MSVC Qt build;
- shared Linux/Windows GUI source;
- WASAPI playback path;
- Windows resources/version metadata;
- Release packaging with `windeployqt`;
- portable standalone ZIP;
- Windows automated regression gate.

### Phase 9B — Windows Workflow + Platform Parity — COMPLETE

#### Phase 9B.0 — Workflow/platform foundation — COMPLETE

- persistent window/dock workspace;
- persistent controls;
- recent program/audio menus;
- remembered file-dialog directories;
- drag/drop;
- command-line/Open-With handling;
- keyboard shortcuts;
- PerMonitorV2 and long-path manifests;
- portable-package isolation checks.

#### Phase 9B.2 — WASAPI hardware hardening — COMPLETE

- stable WASAPI endpoint IDs;
- persistent playback/capture selections;
- real capture → FV-1 → playback;
- 44.1/48 kHz hardware tests;
- 128/256/512/1024 requested-buffer testing;
- negotiated asymmetric native periods;
- endpoint refresh/loss recovery;
- hardware telemetry and acceptance script.

#### Phase 9B.3 — Unicode + Recording/Export Hardening — COMPLETE

Completed at commit `2052794`:

- Unicode-safe program/WAV workflows;
- Windows extended-length paths beyond traditional `MAX_PATH`;
- transactional `.partial-*` recording finalization;
- transactional validation WAV/report output;
- timestamped capture names;
- remembered validation directories;
- truthful GUI smoke-process exit-code checking;
- automated Unicode and >260-character filesystem acceptance;
- full Windows regression suite green.

#### Phase 9B.4 — DPI + Windows Desktop Polish — COMPLETE

No emulator-model work belongs here.

Targets:

- 100%, 125%, 150% and 200% Windows display scaling;
- PerMonitorV2 monitor transitions;
- mixed-DPI monitor movement;
- maximize/restore and persisted geometry;
- dock/splitter restoration across scale changes;
- dialog/menu/tool-tip layout;
- splash/About scaling;
- taskbar and Alt-Tab icon identity;
- Windows keyboard/menu conventions;
- visual regression across themes;
- final native-desktop workflow polish.

### Phase 9C — Windows Completion / Release — FINAL WINDOWS PHASE

9C is a release/regression phase, not a feature-creep phase.

#### Phase 9C.0 — Automated release gate — COMPLETE

Completed at commit `0b94d4c`: clean Release regression, Unicode/long-path and
DPI gates, portable packaging, SHA-256/manifest generation, packaged GUI smoke
and bundled-program artifact verification.

#### Phase 9C.1 — RC torture + clean-package acceptance — COMPLETE

Completed at commit `b4b8776`: all bundled SpinASM programs through realtime,
100 host/runtime lifecycle cycles, a 1800-second continuous realtime soak with
zero underruns/analyzer drops and `device-lost=no`, plus independent portable
package acceptance.

#### Phase 9C.2 — Final 1.0.0 Promotion

The final checkpoint removes the `rc1` release-channel suffix, requires exact
binary/manifest/version agreement, reruns the complete Release gate from a clean
pushed `main`, verifies the final portable ZIP in a Qt-neutral environment and
then creates tag `v1.0.0`.


- all bundled SpinASM programs;
- Test Generator, WAV Loop and Audio Interface;
- Scope/Spectrum/Spectrogram/Levels;
- recording/export;
- debugger/Delay RAM/validation;
- repeated Start/Stop and long-running audio torture;
- endpoint loss/reconnect;
- Unicode/long-path regression;
- DPI regression;
- Release packaging and portable execution;
- clean-system package validation;
- final documentation and release artifact checks.

When Phase 9C closes, the standalone desktop line is considered feature-complete:

```text
Linux     — Qt 6 FV-1 Lab           COMPLETE
macOS     — Native SwiftUI FV-1 Lab COMPLETE
Windows   — Qt 6 FV-1 Lab           COMPLETE
```

## Deferred physical FV-1 acceptance

Physical-chip validation remains an independent fidelity gate. When suitable
hardware/capture infrastructure is available, use the existing deterministic
validation packs and quantify converter/board latency, residual/correlation,
magnitude/phase response, Delay RAM precision/timing, LFO/CHO behavior, POT
quantization/hysteresis, clipping/saturation, LOG/EXP edges and crystal-derived
clock-rate behavior.

Measured differences become minimal regression vectors and model corrections,
not per-effect compatibility hacks.

## After desktop completion

The next major work should be **new emulator/testbench capability**, not another
desktop port.

A future dedicated FV-1 IDE remains a separate SDK consumer. Source editing,
projects, source-mapped breakpoints and EEPROM-bank authoring belong there
unless a small feature directly supports FV-1 Lab's emulator/testbench role.
