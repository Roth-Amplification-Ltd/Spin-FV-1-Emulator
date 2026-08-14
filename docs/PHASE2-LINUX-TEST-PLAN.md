# Phase 2 Linux validation plan

Phase 2 is intentionally validated in two layers: deterministic headless tests first,
then real audio-interface tests on the developer workstation.

## Automated gate

```bash
./bootstrap-dev.sh --clean
```

Expected CTest groups include:

- Phase-1 core instruction/state tests;
- Phase-2 runtime/source/analyzer tests;
- all eight Steal This DSP program compile/render regressions;
- 48 kHz host -> 32.768 kHz virtual FV-1 SRC regression;
- `fv1-live` command-line smoke test.

The SRC regression renders two seconds at 48 kHz through a virtual FV-1 clocked at
32.768 kHz and requires exactly 65,536 virtual FV-1 samples while preserving the
48 kHz output duration.

## Confirm production backends

The bootstrap installs `libspeexdsp-dev` and `libminiaudio-dev` on supported apt
hosts. During CMake configure, look for:

```text
-- Phase 2 SRC: SpeexDSP enabled
-- Phase 2 audio: miniaudio enabled from /usr/include
```

`fv1-live` also prints the SRC backend when a session starts. A Linux production
bring-up should report `SpeexDSP`, not the deterministic linear fallback.

## Enumerate Cortana audio devices

```bash
./build/fv1-live devices
```

Record the capture and playback indices for the intended interface.

## Test 1: generated sine, no capture device required

Use a low output level at the physical interface/speakers for initial bring-up.

```bash
./build/fv1-live run \
  examples/simple_passthrough.spn \
  --sine 440 \
  --host-rate 48000 \
  --buffer 256 \
  --clock 32768 \
  --seconds 5 \
  --meter
```

Expected:

- audible clean 440 Hz tone;
- analyzer dominant frequency near 440 Hz;
- callback CPU comfortably below 100%;
- output underrun count should settle at zero after startup or remain negligible.

## Test 2: looped audio-file source

```bash
./build/fv1-live run \
  examples/steal-this-dsp-programs/03_pitch_maw.spn \
  --file ~/Music/fv1-test.wav \
  --loop-start 0 \
  --loop-end 8 \
  --host-rate 48000 \
  --buffer 256 \
  --clock 32768 \
  --pot0 0.60 --pot1 0.50 --pot2 0.70 \
  --seconds 20 \
  --meter
```

File playback is sample-rate-correct before the FV-1 clock bridge. A 44.1 kHz or
96 kHz source therefore plays at its intended pitch/time while the virtual FV-1
remains at the selected crystal/sample rate.

## Test 3: live audio interface

After `devices`, substitute the actual indices:

```bash
./build/fv1-live run \
  examples/steal-this-dsp-programs/03_pitch_maw.spn \
  --live \
  --input-device 0 \
  --output-device 0 \
  --host-rate 48000 \
  --buffer 256 \
  --clock 32768 \
  --pot0 0.60 --pot1 0.50 --pot2 0.70 \
  --meter
```

Without `--seconds`, press Enter to stop.

## Latency/buffer sweep

Once 256 frames is stable, test:

```text
512 -> 256 -> 128 -> 64 frames
```

Do not treat a requested period as guaranteed hardware latency: the selected backend
and device may negotiate another internal period. The GUI phase will expose the
reported/observed device configuration and xrun telemetry more explicitly.

## Phase-2 exit criteria

Phase 2 is accepted on Linux when:

1. all automated tests pass with SpeexDSP/miniaudio enabled;
2. `fv1-live devices` enumerates the intended interface;
3. generated sine/test signals process audibly;
4. an arbitrary-rate WAV can loop continuously through an FV-1 program;
5. live capture processes through the same runtime and playback device;
6. sustained operation has no runaway underruns, analyzer queue drops, crashes, or
   allocations/locks introduced into the realtime callback.

Headless implementation validation is recorded in [`PHASE2-HEADLESS-TEST-REPORT.md`](PHASE2-HEADLESS-TEST-REPORT.md).
