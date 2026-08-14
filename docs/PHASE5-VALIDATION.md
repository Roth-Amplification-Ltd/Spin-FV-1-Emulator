# Phase 5A — Validation Framework

## Goal

Phase 5A prepares the emulator for factual comparison with a physical Spin FV-1 without requiring
hardware to be present during development. The validation engine is a reusable C++ library and the
Qt UI/CLI are clients of that library.

The fundamental workflow is:

```text
                    one deterministic stimulus.wav
                         /                 \
                        /                   \
                       v                     v
             fv1-cli render             physical FV-1
                  |                          |
                  v                          v
             virtual.wav                capture.wav
                         \                 /
                          \               /
                           v             v
                         fv1-validation
                              |
                +-------------+-------------+
                |             |             |
              residual     metrics      reports
               WAV       + PASS/FAIL   JSON/MD/CSV
```

The hardware leg is deferred until suitable hardware is available. The software leg is fully
self-testable by creating captures with known delay, gain and injected error.

## Library boundary

`libfv1-validation` deliberately has no Qt, miniaudio or operating-system UI dependency. It provides:

- `ValidationAudio` host-rate stereo buffers;
- common WAV import and float32 WAV export;
- deterministic validation stimulus generation;
- reference/capture alignment;
- per-channel time-domain metrics;
- spectral magnitude/phase comparison;
- report-bundle generation.

The future dedicated FV-1 IDE or an automated hardware test runner can consume this library without
launching `fv1-lab`.

## Alignment convention

`capture_delay_frames > 0` means the captured hardware/reference signal occurs **after** the virtual
reference. The validator searches a configurable ± time window using normalized mono
cross-correlation, then crops the two recordings to their common aligned region.

Alignment removes bulk latency before residual and phase comparison. The measured latency itself is
retained in the result/report because converter and board latency are important validation data.

## Gain handling

Raw capture/reference RMS is always measured and the corresponding gain error is always reported.

Optional **gain matching for residual** computes one common least-squares scalar and applies it only
to residual/SNR/spectral comparison. It does not hide the raw gain error. This is useful when a
hardware test jig has a known analog gain offset but the DSP transfer behavior is what is under test.

## Metrics

For left and right channels:

- reference RMS dBFS;
- capture RMS dBFS;
- raw gain error dB;
- correlation;
- residual RMS dBFS;
- residual peak dBFS;
- SNR dB.

Global/alignment values:

- host sample rate;
- capture delay frames;
- capture delay milliseconds;
- compared frame count;
- optional applied capture gain correction.

The spectral pass uses a Hann-windowed FFT over the highest-energy aligned region and reports active
reference bins only. Each point includes frequency, capture/reference magnitude error, phase error
(after bulk delay alignment), and reference level.

## CLI

Generate a deterministic multitone:

```bash
./build/fv1-cli stimulus /tmp/fv1-validation.wav \
  --kind multitone --seconds 5 --host-rate 48000 --level 0.25
```

Render it through the virtual chip:

```bash
./build/fv1-cli render \
  examples/steal-this-dsp-programs/03_gravity_clerk.spn \
  /tmp/fv1-validation.wav /tmp/gravity-clerk-virtual.wav \
  --clock 32768 --pot0 0.60 --pot1 0.50 --pot2 0.70
```

Later, feed `/tmp/fv1-validation.wav` to the physical test rig and record the result as
`/tmp/gravity-clerk-hardware.wav` at the same host sample rate.

Compare:

```bash
./build/fv1-cli validate \
  /tmp/gravity-clerk-virtual.wav \
  /tmp/gravity-clerk-hardware.wav \
  --max-lag-ms 100 \
  --gain-match \
  --report-prefix /tmp/gravity-clerk-validation
```

The report prefix creates:

```text
gravity-clerk-validation.json
gravity-clerk-validation.md
gravity-clerk-validation-frequency.csv
gravity-clerk-validation-residual.wav
```

## Qt workspace

FV-1 Lab retains its existing engineering-dashboard layout and adds one center tab named
**VALIDATION**. The tab is offline and does not alter the realtime session. It provides:

- Virtual/reference WAV selection;
- Hardware/capture WAV selection;
- maximum alignment search;
- optional residual gain matching;
- FFT size and acceptance limits;
- Analyze Reference vs Capture;
- PASS/FAIL measurement table;
- spectral error table;
- report-bundle export;
- deterministic stimulus generator.

This is a lab/testbench feature, not an IDE project system.

## Reports are data, not claims of silicon accuracy

A PASS only means the selected pair of recordings satisfies the configured numerical thresholds.
Before Phase 5B hardware measurements, it does **not** mean the emulator is bit-exact to silicon.
