# FV-1 SDK Consumer Requirements Audit

## Purpose

Phase 6B treats the SDK as an observer boundary. The question is not whether FV-1 Lab can call the
library; the question is whether an unrelated host can observe and control the virtual FV-1 without
learning private implementation details.

The audit covers four consumers:

| Consumer | Must have from shared SDK | Host-owned / deliberately outside core ABI |
|---|---|---|
| Native macOS application | create/reset/load, SpinASM compile, POTs, float process, snapshot, delay inspection, resource report, debug stepping, version/capabilities | SwiftUI/AppKit, CoreAudio/AVAudioEngine, Metal/plot rendering, host-rate SRC, settings |
| Native Windows application | same virtual-chip/debug surface | WinUI/Win32, WASAPI, native rendering, host-rate SRC, settings |
| Future FV-1 IDE | compiler diagnostics, program readback, resource report, state snapshot, instruction stepping/trace, delay inspection, opcode names | editor/project system, source UI, EEPROM authoring UI, breakpoint policy, source-map UX |
| Third-party host | tiny create/load/control/process/destroy path, deterministic result codes, version/capability discovery | audio devices, threads, plugin API, files, UI, host allocation policy |

## Phase-6B decisions

### Core ABI v1 candidate: keep

The core `<fv1/sdk.h>` candidate contains only capabilities that are broadly useful to native apps and
third-party hosts:

- version/capability discovery;
- opaque engine lifetime and reset;
- 512-byte program load and readback;
- native in-memory SpinASM compilation;
- all-POT and single-POT updates;
- single-sample, planar-block, and interleaved-block float processing;
- state snapshot, delay-word inspection, and static resource analysis;
- stable architectural register indices for native inspectors/debuggers.

### Debug ABI candidate: separate module

`<fv1/sdk_debug.h>` carries IDE/testbench-oriented primitives without making them conceptually part of
the minimum realtime host surface:

- begin virtual sample;
- step one executed instruction and receive a versioned trace record;
- finish the virtual sample and retrieve output;
- state digest for deterministic comparison;
- stable opcode-name lookup;
- stable register-name lookup for the architectural register map.

A host can implement PC breakpoints by stopping before/after step calls. Source-mapped breakpoints and
compiler source maps are intentionally **not required to freeze the core ABI**; they can be additive
compiler/debug APIs later without changing existing engine/process semantics.

### Keep outside ABI v1

The following are intentionally not promoted just because FV-1 Lab uses them internally:

- OS audio-device ownership;
- host-rate sample-rate conversion;
- Qt/Win32/AppKit/SwiftUI types;
- scope/spectrum/spectrogram drawing;
- file dialogs/settings/session objects;
- physical-hardware validation UI;
- C++ reference/conformance model classes;
- save-state serialization of private engine internals.

State serialization is especially deferred: freezing a private memory layout would prevent later model
corrections. A future portable state format must be an explicit semantic format, not a dumped C++
object.

## Threading and realtime assumptions

One `fv1_sdk_engine` is a mutable virtual device and is not concurrently callable from multiple
threads. A host may own multiple independent engines. Global version/result-string discovery is
process-global immutable data.

Only the documented `fv1_sdk_engine_process_*()` path carries the realtime no-allocation/no-lock/no-I/O
promise. Creation, destruction, compilation, reports, debug stepping, and inspection are setup/offline
operations unless later documented otherwise.

## Completeness conclusion

The candidate is sufficient for the first native Windows/macOS applications and for a third-party
virtual-FV-1 module without exposing application internals. The optional debug header supplies the
primitive machine-control surface a future IDE needs while leaving higher-level editor/project/source-
map behavior additive. Missing IDE conveniences such as source maps, disassembly formatting, breakpoint
collections, state mutation, and EEPROM-bank authoring can be added as new debug/compiler functions
without changing the frozen engine/process ABI; none is required for a native frontend to run the chip.

This conclusion is a **software API completeness decision**, not physical-silicon validation.
