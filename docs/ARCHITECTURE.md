# Architecture

## Dependency direction

The fundamental rule is that the virtual DSP never depends on an application framework. Phase 6A
adds an even stronger rule: **frontends embed the virtual chip through a narrow SDK boundary rather
than inheriting the Linux application's object graph.**

```text
 Linux Qt frontend      Windows native frontend      macOS native frontend      other host
        |                       |                           |                      |
        +-----------------------+---------------------------+----------------------+
                                        |
                                  <fv1/sdk.h>
                                  FV1SDK::sdk
                                        |
                               private C++ internals
                                 /              \
                           fv1-core          fv1-spinasm

Linux application-only services remain outside the SDK ABI:

     fv1-runtime -> fv1-analysis / fv1-audio / fv1-debugger / fv1-validation

Verification-only dependency direction:

      fv1-reference      independent readable Model A
             \            /
              \          /
               fv1-conformance
                     |
                     +---- compares against fv1-core
```

The candidate SDK is C-shaped and contains no Qt, miniaudio, SpeexDSP, STL, exception, filesystem,
CoreAudio, WASAPI, PipeWire or UI types. `fv1-runtime` now dogfoods `FV1SDK::sdk` for its virtual-chip
create/load/POT/process path while keeping host-rate SRC outside the ABI. The current auxiliary C++
libraries remain free to evolve until they are either kept private or deliberately wrapped by a
future additive SDK module.

See `SDK-ARCHITECTURE.md` and `SDK-ABI-POLICY.md`.

## Phase-5C reference/conformance boundary

`fv1-reference` is intentionally independent of `fv1-core`: it does not link the production library
and does not reuse its decoder or arithmetic helpers. `fv1-conformance` is the only layer that knows
about both models and compares identical deterministic vectors instruction-by-instruction.

Both engines expose explicit virtual sample/instruction coordinates and deterministic state digests.
Normal production processing and debugger stepping share the same internal production stepper so the
GUI/debug path cannot quietly acquire different DSP semantics from realtime/offline rendering.

The reference model is an implementation-divergence oracle, **not** a substitute for physical FV-1
evidence. See `HARDWARE-EMULATION-CONTRACT.md`.

## Phase-1 virtual sample

A virtual sample proceeds in this order:

1. quantize and load POT0/POT1/POT2;
2. convert stereo ADC input to signed Q1.23;
3. execute up to 128 predecoded FV-1 instructions;
4. expose DACL/DACR;
5. decrement the circular delay-RAM base pointer;
6. advance the two SIN/COS and two RAMP LFOs.

The debugger uses the same state machine but permits the instruction execution step to be externally paused and inspected.

## Program representation

External programs are 128 32-bit words / 512 bytes. `fv1_load_bytes()` accepts the standard big-endian program image. Decode occurs once when the program is loaded. The hot sample loop therefore dispatches already-decoded operands instead of repeatedly extracting bitfields.

## Numeric representation

Core registers use signed 24-bit values stored in `int32_t` containers. Intermediate multiplication uses 64-bit integers and explicitly saturates back into the FV-1 range.

This is preferable to a float-only emulator because the FV-1 architecture is a 24-bit linear processor and many algorithm corner cases are quantization/saturation dependent.

## Delay memory

The hardware delay RAM is lower precision than the processor/register datapath and uses a special floating representation. Phase 1 deliberately exposes two models:

- `FV1_DELAY_REFERENCE_16`: reduced-precision reference model used by default;
- `FV1_DELAY_FULL_24`: diagnostic full-precision storage.

Neither is labeled as the final bit-exact hardware model. Hardware-validation vectors will decide the final implementation.

## Compiler boundary

Phase 6A replaces the runtime Python bridge with `fv1-spinasm`, a native C++20 compiler used by the
CLI, live host and Qt application. External SDK consumers access the same compiler through
`fv1_sdk_compile_spinasm_v1()` and receive a caller-owned 512-byte program image plus versioned
diagnostics metadata.

The historical Python assembler remains a development oracle only. Automated tests require native
and Python output to be byte-identical for all eight bundled programs. No Python interpreter is
required by an installed SDK/application at runtime.

`fv1-spinasm` itself is **not** the stable cross-language ABI; its C++ API remains an internal
implementation detail behind `fv1/sdk.h`.

## Phase-2 audio-source boundary

Realtime/live and imported-file processing are not separate DSP paths. Both will implement one source interface and feed the same runtime:

```text
LiveInputSource ---+
FileLoopSource ----+--> FV-1 clock bridge --> libfv1-core --> output/analyzer
TestSignalSource --+
```

This is required so a looped recording is a deterministic substitute for repeatedly playing an instrument while debugging an effect.


## Phase-2 realtime callback

The device callback owns no emulator policy. It asks the selected `AudioSource` for a host-rate block, passes that block through `Runtime`, writes the returned host-rate block to the device, and enqueues a copy to `AnalyzerWorker`. The runtime and source objects are prepared before the device starts so the callback performs no intentional allocation.

See `docs/AUDIO-RUNTIME.md` and `docs/PHASE2-LINUX-TEST-PLAN.md`.

## Realtime DSP bypass / raw monitor

The UI bypass control is implemented at the `AudioHost` callback boundary rather than by mutating or replacing the virtual FV-1 program. The selected `AudioSource` still renders normally. With DSP enabled, its frames pass through `Runtime`; with DSP bypassed, the host copies those frames directly to the device output and analyzer queue.

This keeps the audio device and source transport alive, makes processed/raw A/B switching realtime-safe, and ensures the oscilloscope can display the exact raw source presented to the emulator. The bypass flag is atomic; toggling it from the Qt thread requires no callback-thread lock or allocation.

Audio preferences remain a frontend concern. Qt stores user selections through `QSettings` and passes concrete playback/capture/rate/buffer/FV-1-clock/SRC-quality values into the platform-neutral runtime/audio APIs when a session starts.

## Phase-4 reusable testbench services

Phase 4 keeps the frontend/application boundary intact. Two additional services are intentionally
usable without Qt:

```text
                    fv1-core
                    /      \
                   v        v
          fv1-debugger     fv1-runtime
                               |
                    +----------+-----------+
                    v                      v
               fv1-analysis             fv1-audio
                                            |
                                      AudioRecorder

Qt fv1-lab consumes these libraries; it does not own their DSP/debug/audio policy.
```

### `fv1-debugger`

`fv1-debugger` owns a private core instance for offline instruction/sample stepping. The Qt chip
inspector is one consumer. A future dedicated FV-1 IDE can consume the same library for source-level
features without moving editor responsibilities into this emulator application.

### Raw and processed analyzer taps

`AudioHost` exposes the source signal to a raw analyzer before the FV-1 clock bridge, while the
processed analyzer receives host-rate output after the virtual chip. Both queues are lock-free and
may be visualized simultaneously. Turning DSP bypass on/off is independent of retaining the raw tap.

### Realtime recording

`AudioRecorder` is attached to `AudioHost` through a non-owning atomic pointer. The callback may push
raw and/or processed `StereoFrame` values into fixed-capacity SPSC rings but performs no filesystem
I/O. A background thread writes stereo IEEE-float WAV files and finalizes their RIFF headers on stop.

### File-loop transport

File playback position, transport state, looping and seek controls use atomics or pre-existing
prepared storage so play/pause/seek/loop changes do not require audio-device reconstruction. Optional
loop-boundary crossfade is rendered inside `FileLoopSource` before the shared FV-1 runtime path.


## Phase-5 validation boundary

`fv1-validation` is an offline measurement layer. It never runs in the realtime audio callback and
does not depend on Qt or a device backend.

```text
validation stimulus -----------------------------+
       |                                         |
       v                                         v
  fv1-runtime / core                      physical FV-1 (later)
       |                                         |
       v                                         v
  virtual reference WAV                    captured WAV
       \                                         /
        +-------------- fv1-validation ---------+
                         |
               alignment + residual
               gain/correlation/SNR
               magnitude/phase error
                         |
                JSON / MD / CSV / WAV
```

Bulk capture latency is estimated before residual comparison and remains a first-class reported
measurement. Optional gain matching applies only to residual/SNR/spectral comparison; raw gain error
is still reported. This distinction prevents a convenient normalization step from hiding analog
level differences in the physical validation rig.

The Qt **VALIDATION** tab and `fv1-cli validate` consume the exact same library. A future automated
hardware runner or dedicated IDE can consume it as well.
