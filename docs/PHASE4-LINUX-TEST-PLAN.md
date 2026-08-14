# Phase 4 Linux Test Plan

Target host for acceptance: Cortana / Ubuntu 24.04 with the repository `bootstrap-dev.sh` environment.

## 1. Clean build

```bash
./bootstrap-dev.sh --clean
```

Expected production paths on Cortana:

- SpeexDSP enabled;
- miniaudio enabled;
- Qt 6 GUI enabled;
- all CTest cases pass, including the offscreen `fv1-lab-smoke` test.

## 2. GUI launch and footer

```bash
./build/fv1-lab
```

Confirm the accepted Phase-3 dashboard remains recognizable and the bottom-right status bar shows
`© 2026 Roth Amplification LTD`.

## 3. Program/resource inspection

Open `examples/steal-this-dsp-programs/03_pitch_maw.spn`. Confirm Resource Usage populates and the
offline chip inspector resets/loads the same program.

## 4. Raw vs processed scope

Use Test Generator / Sine / 440 Hz, host 48 kHz, buffer 256, virtual FV-1 32768 Hz. Start Pitch Maw.
Enable **RAW + FX OVERLAY**. Confirm the raw and processed traces are visually distinct and that DSP
bypass still switches the audible/primary monitor to the raw signal without stopping the device.

Right-click the scope and exercise:

- time zoom;
- vertical gain;
- Auto trigger;
- Normal trigger;
- Single trigger + re-arm;
- left/right source;
- rising/falling slope;
- trigger level;
- freeze/clear;
- Copy/Save Plot Image;
- CSV export.

## 5. Spectrum/spectrogram

Right-click the spectrum and test log/linear frequency display, dB range and peak hold. A 440 Hz
passthrough signal should report a dominant frequency near 440 Hz rather than only the nearest coarse
FFT bin. Test spectrogram history/dynamic range and freeze/clear.

## 6. File-loop transport

Generate or choose a WAV. Use Audio File Loop and exercise play, pause, stop, seek, loop region and a
short boundary crossfade (for example 5 ms). Confirm no device teardown occurs when transport changes.

## 7. Recording

During an active session choose **Record Raw / Processed Audio…** and record Raw + Processed. Stop the
recording and confirm both WAV files exist, contain audio, and the DSP Status panel reports zero record
drops during the short acceptance run.

## 8. Offline chip inspector

With Pitch Maw loaded, set debug input L/R and press Step Instruction repeatedly. Confirm PC,
instruction/opcode, ACC/PACC/LR, registers and Delay RAM update. Step Sample should complete one virtual
sample without affecting realtime playback if a realtime session is also running.

## Deferred hardware test

External capture/duplex interface validation remains deferred until an interface is physically
available. This does not block Phase-4 generator/file-loop/testbench acceptance.
