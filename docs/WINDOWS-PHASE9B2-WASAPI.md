# Windows Phase 9B.2 — WASAPI Hardware Hardening

Phase 9B.2 hardens the shared desktop audio host for real Windows endpoint
changes. It does **not** modify the FV-1 execution model or FV1SDK ABI.

## What changes

### Stable Windows endpoint selection

`AudioDeviceInfo` now exposes a Windows WASAPI endpoint identifier in addition to
the legacy enumeration index. The Qt frontend persists the identifier and uses
it when a session opens.

The old integer indices remain supported for CLI/backward compatibility, but a
saved Windows selection is no longer tied to whatever order endpoints happen to
be enumerated in after a refresh/replug.

For an explicit WASAPI selection, the saved opaque Windows endpoint ID is
converted directly back into miniaudio's `ma_device_id` and supplied to device
initialization. It is **not** re-enumerated and translated back into a transient
array index. This follows the Windows endpoint-ID contract and avoids
enumeration-order/rematching failures between processes or contexts.

### Native endpoint information

Enumeration also reports:

- natively advertised sample rates;
- maximum advertised channel count;
- stable endpoint token on WASAPI.

FV-1 Lab places this information in endpoint tooltips.

### Realtime device-health telemetry

The host now tracks, using only atomics from the miniaudio notification/data
callbacks:

- started notifications;
- stopped notifications;
- reroute notifications;
- interruption notifications;
- unexpected external device stop;
- minimum/maximum observed callback size;
- callback sample rate;
- native playback/capture sample rates;
- native playback/capture period sizes.

No device start/stop/reopen operation is performed from a realtime or miniaudio
notification callback.

### Windows unplug/reroute behavior

If an active endpoint stops unexpectedly, the notification callback only raises
an atomic fault flag. FV-1 Lab's existing 50 ms UI telemetry timer observes the
flag and performs teardown/refresh from the UI thread.

This keeps the realtime boundary safe while preventing the GUI from continuing
to claim that a dead WASAPI stream is running.

Default-device reroutes are counted and logged without tearing down a stream
that miniaudio successfully rerouted.

### Hardware acceptance CLI

`tools/windows-audio-acceptance.ps1` drives `fv1-live.exe` through:

- 48 kHz / 256 frames;
- 44.1 kHz / 256 frames;
- 48 kHz / 128 frames;
- 48 kHz / 512 frames.

It can select an endpoint by legacy index or stable WASAPI ID and can optionally
exercise live capture -> FV-1 -> playback.

## Build/test

```powershell
Set-ExecutionPolicy -Scope Process Bypass

.\tools\test-windows.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

Then:

```powershell
.\tools\windows-audio-acceptance.ps1
```

For a specific endpoint, copy its `id=` value from `fv1-live devices`:

```powershell
.\tools\windows-audio-acceptance.ps1 -OutputId "<WASAPI endpoint id>"
```

For full duplex:

```powershell
.\tools\windows-audio-acceptance.ps1 `
    -OutputId "<playback id>" `
    -InputId "<capture id>" `
    -LiveInput
```

## Manual Phase 9B.2 acceptance

- select a non-default playback endpoint;
- select a non-default capture endpoint;
- press Refresh Audio Devices and verify selections survive;
- start Test Generator and inspect native/callback-rate telemetry;
- exercise 44.1/48 kHz and 128/256/512-frame requests;
- start Audio Interface mode and verify real capture -> FV-1 -> playback;
- while running on an explicit endpoint, disable/unplug it and confirm FV-1 Lab
  reports the unexpected stop and returns safely to STOPPED;
- while running on OS Default, change the Windows default output endpoint and
  confirm a reroute event is logged if WASAPI/miniaudio reports it;
- verify subsequent Start works after endpoints are restored/refreshed.

Commit this phase only after the automated test gate and applicable hardware
checks pass.
