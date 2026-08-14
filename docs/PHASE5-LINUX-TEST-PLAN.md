# Phase 5A Linux Acceptance Plan

## 1. Clean bootstrap

```bash
cd ~/GitHub/Spin-FV-1-Emulator
./bootstrap-dev.sh --clean
```

Expected on the fully provisioned Cortana host:

```text
Phase 2 SRC: SpeexDSP enabled
Phase 2 audio: miniaudio enabled
Phase 5A validation/testbench GUI: Qt 6.4.2 enabled
```

CTest should include the existing core/runtime/audio/debugger tests plus:

```text
fv1-validation-tests
phase5-validation-cli
fv1-lab-smoke
```

## 2. CLI zero-residual self-check

```bash
./build/fv1-cli stimulus /tmp/fv1-validation.wav \
  --kind multitone --seconds 1 --host-rate 48000 --level 0.30

./build/fv1-cli validate \
  /tmp/fv1-validation.wav /tmp/fv1-validation.wav \
  --report-prefix /tmp/fv1-identical
```

Expected:

- PASS;
- delay `0` frames;
- L/R correlation `1.0` to numerical precision;
- residual near the validator floor;
- JSON, Markdown, CSV and residual WAV files created.

## 3. Emulator-side reference creation

```bash
./build/fv1-cli render \
  examples/steal-this-dsp-programs/03_gravity_clerk.spn \
  /tmp/fv1-validation.wav /tmp/gravity-clerk-virtual.wav \
  --clock 32768 --pot0 0.60 --pot1 0.50 --pot2 0.70
```

This creates the virtual reference that will later be compared with physical hardware.

## 4. GUI

```bash
./build/fv1-lab
```

Open the **VALIDATION** tab. First compare `/tmp/fv1-validation.wav` with itself and confirm PASS.
Export a report bundle and verify all four output files.

Also verify **View → Application Icon** can switch among:

- Silver;
- Dark Cyan;
- Blue;
- Amber.

The selection should persist on restart. The Linux launcher uses Silver as the default installed icon.

## 5. Deferred Phase 5B test

When a physical FV-1 interface rig is available, capture the hardware response to the same stimulus
at the same host sample rate, then use the exact CLI/GUI validation path above. No validation-engine
rewrite should be required.
