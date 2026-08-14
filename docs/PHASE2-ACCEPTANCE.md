# Phase 2 acceptance status

Phase 2 is accepted for continued development on Linux with one intentionally deferred hardware test.

## Accepted on Cortana

The production Ubuntu 24.04 paths were exercised with system SpeexDSP and miniaudio. The following paths completed without output underruns or analyzer queue drops:

- test generator -> 48 kHz host -> 32.768 kHz virtual FV-1 -> audio playback
- Gravity Clerk -> real Linux audio playback
- looped WAV source -> source-rate conversion -> 48 kHz host -> 32.768 kHz virtual FV-1 -> audio playback
- runtime/analyzer regression suite

Observed callback CPU load on the tested laptop playback device was approximately 3.4-3.6 percent at a requested 256-frame period.

## Deferred

A separate external capture/playback audio interface was not physically available during Phase 2 acceptance. The capture/duplex path remains implemented but must be exercised when suitable hardware is available. This is tracked as a deferred acceptance item, not a blocker for Phase 3 GUI development.

## Post-acceptance cleanup

Before Phase 3, two small behaviors were tightened:

1. The analyzer suppresses dominant-frequency output for effectively silent FFT blocks.
2. `fv1-live --seconds N` now configures an exact host-frame target. The final audio device period is partially processed and zero-filled so runtime host-frame accounting is deterministic.
