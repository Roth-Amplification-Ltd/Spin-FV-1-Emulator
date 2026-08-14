# Phase 2 Headless Test Report

## Status

Phase 2 runtime/source/analyzer implementation is complete for Linux headless validation. Final hardware acceptance is intentionally pending a real Cortana audio-interface run with the production SpeexDSP and miniaudio paths enabled.

## Components exercised

- `libfv1-runtime` host-rate <-> virtual-FV-1 clock bridge
- independent virtual FV-1 sample rate
- deterministic fallback stereo sample-rate converter
- fractional virtual clocks, including 46.6084 kHz
- `LiveInputSource`, `FileLoopSource`, and `TestSignalSource`
- loopable WAV playback at a file rate different from the host rate
- PCM16/24/32, float32, and standard WAVE_FORMAT_EXTENSIBLE PCM/float parsing
- sine / logarithmic sweep / white noise / pink noise / repeating impulse generators
- lock-free SPSC analyzer producer queue
- background RMS, peak, correlation, FFT spectrum and dominant-frequency analysis
- `fv1-cli render` host/virtual clock separation
- `fv1-live` command-line and no-device fallback build

## GCC validation

Final `RelWithDebInfo` build used GCC 14.2 in the build container. CTest result:

```text
7/7 tests passed
fv1-core-tests                 PASS
fv1-phase2-tests               PASS
compile-steal-this-bank        PASS
cli-inspect-pitch-maw          PASS
render-steal-this-bank         PASS
phase2-render-src              PASS
fv1-live-help                  PASS
```

The 48 kHz / 32.768 kHz render regression processes exactly two seconds / 96,000 host frames while executing exactly 65,536 virtual FV-1 samples.

The fractional-clock regression runs five host seconds at a virtual rate of 46,608.4 Hz and verifies approximately 233,042 virtual samples, rather than rounding the virtual clock to an integer-Hz model.

## Sanitizer validation

A clean Clang 17 Debug build was run under AddressSanitizer and UndefinedBehaviorSanitizer with leak detection enabled. All seven CTest tests passed without sanitizer diagnostics.

## Production-dependency compile coverage

The build container does not provide the system SpeexDSP/miniaudio development packages. The normal headless build therefore uses the deterministic linear SRC and no-device audio-host stub.

To prevent the conditional production code from becoming syntactically stale, the SpeexDSP-enabled and miniaudio-enabled source paths were also compiled against narrow API-shape test headers with strict warnings-as-errors. The SpeexDSP path includes the fractional-rate initializer used for non-integer virtual clocks.

This is compile coverage, **not** a substitute for running the real libraries or real hardware.

## Acceptance remaining on Cortana

1. Run `./bootstrap-dev.sh --clean` and confirm CMake reports SpeexDSP and miniaudio enabled.
2. Run `./build/fv1-live devices` and confirm real playback/capture device enumeration.
3. Run generated sine through an output device.
4. Run a looped WAV through a real effect program.
5. Run live interface capture -> virtual FV-1 -> interface playback.
6. Sweep 512/256/128/64-frame requested periods and observe CPU load, underruns and analyzer drops.

See `PHASE2-LINUX-TEST-PLAN.md` for commands.
