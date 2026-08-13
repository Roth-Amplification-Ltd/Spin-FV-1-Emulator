# Architecture

## Dependency direction

The fundamental rule is that the virtual DSP never depends on an application framework.

```text
frontends (future Qt / SwiftUI)
             |
             v
       fv1-runtime       audio sources / SRC / devices (Phase 2)
             |
             +----------> fv1-analysis (Phase 2)
             |
             v
         fv1-core        zero GUI/audio/OS dependencies
             ^
             |
       compiler/loader
```

`fv1-core` contains no JUCE, Qt, GTK, miniaudio, PipeWire, CoreAudio or UI types.

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

Phase 1 keeps the existing MPL-2.0 Python SpinASM-compatible assembler as an isolated tool and uses it from `fv1-cli` for `.spn` inputs. Binary execution itself has no Python dependency.

This lets the emulator core stabilize independently. A future native compiler library can replace the bridge without changing the C ABI of `fv1-core`.

## Phase-2 audio-source boundary

Realtime/live and imported-file processing will not be separate DSP paths. Both will implement one source interface and feed the same runtime:

```text
LiveInputSource ---+
FileLoopSource ----+--> FV-1 clock bridge --> libfv1-core --> output/analyzer
TestSignalSource --+
```

This is required so a looped recording is a deterministic substitute for repeatedly playing an instrument while debugging an effect.
