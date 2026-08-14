# Phase 2 audio/runtime architecture

## One processing path

All host sources render interleaved stereo at the host device rate and then enter the
same runtime. No FV-1 behavior is duplicated in the live, file, or generator code.

```text
Live capture --------+
Looped WAV ----------+--> AudioSource --> host/FV-1 SRC --> fv1-core --> FV-1/host SRC --> playback
Test generator ------+                                               |
                                                                     +--> SPSC analyzer queue
```

`AudioSource::render()` is realtime-safe. `FileLoopSource::load()` is deliberately a
non-realtime operation: it decodes the source file before playback starts. The audio
callback only reads already-decoded memory.

## Clock domains

`Runtime` owns two explicit sample-rate converters. The audio interface clock and the
virtual FV-1 clock are independent configuration values.

For example:

```text
48,000 host frames/s -> SRC -> 32,768 FV-1 samples/s -> SRC -> 48,000 host frames/s
```

This prevents host interface selection from silently changing delay lengths and LFO
rates inside FV-1 programs.

SpeexDSP is the production Linux SRC backend. A small deterministic linear streaming
fallback exists so the core/runtime can still be compiled and tested on development
containers that do not provide SpeexDSP. The fallback is not the preferred listening
or hardware-validation path.

## File loop source

Current Phase-2 file support is WAV PCM16/24/32 and IEEE float32, mono or stereo.
The entire file is decoded to float stereo before playback. The source renderer uses
fractional source position so file sample rate is independent of host device rate.

Loop regions use source-file frame coordinates internally. The CLI exposes seconds.
Wrapping is continuous and interpolation bridges the final frame to the first frame of
the selected region. Optional loop-boundary crossfades belong to the GUI/development
phase after the basic deterministic transport is proven.

## Realtime rules

The miniaudio callback performs only:

1. render selected source into a preallocated buffer;
2. host -> FV-1 SRC;
3. virtual FV-1 execution;
4. FV-1 -> host SRC;
5. copy output to the device buffer;
6. enqueue output frames to the lock-free analyzer ring.

The callback performs no file I/O, logging, FFT, GUI calls, mutex acquisition, or
intentional heap allocation.

## Analyzer

`AnalyzerWorker` uses a single-producer/single-consumer frame queue. The audio callback
is the producer and never blocks; if the analysis thread falls behind, analysis frames
are dropped and counted rather than stalling audio.

The background worker currently computes:

- stereo peak;
- stereo RMS;
- L/R correlation;
- Hann-windowed mono spectrum;
- dominant-frequency estimate.

The FFT implementation is hidden behind the analysis layer. Phase-2 bring-up uses a
small dependency-free radix-2 FFT so the runtime remains testable in stripped-down CI
containers. Replacing that implementation with the previously selected PFFFT backend
is an internal optimization and does not change the analyzer/UI API.

## Fractional FV-1 clocks

The virtual clock is stored as a floating-point rate and is not rounded to whole Hz. When SpeexDSP is enabled, the runtime uses `speex_resampler_init_frac()` with a millihertz rational representation. This preserves crystal-derived rates such as 46.6084 kHz instead of silently treating them as 46.608 kHz.

## WAV input coverage

`FileLoopSource` currently decodes mono/stereo PCM16, PCM24, PCM32, IEEE float32, and the corresponding standard `WAVE_FORMAT_EXTENSIBLE` PCM/float subtypes when valid bits equal the container width. Other codecs and packed-valid-bit variants remain explicit future work.

