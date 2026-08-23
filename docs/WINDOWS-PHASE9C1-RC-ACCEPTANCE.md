# Windows Phase 9C.1 — RC Torture + Clean-Machine Acceptance

This checkpoint is the second half of Phase 9C.

The automated Release gate is already committed. This pass attacks runtime
lifecycle stability and validates the actual portable release candidate.

It does not change the FV-1 execution model or FV1SDK ABI.

## Quick preflight

```powershell
.\tools\windows-phase9c-rc-torture.ps1 -Quick
```

Expected:

```text
=== PHASE 9C.1 WINDOWS RC TORTURE AUTOMATION PASSED ===
```

## Full backend/session torture

```powershell
.\tools\windows-phase9c-rc-torture.ps1
```

Default coverage:

- every bundled `.spn` through the realtime host;
- 100 host/runtime lifecycle cycles;
- 1800-second continuous realtime soak;
- zero output underruns;
- zero analyzer drops;
- `device-lost=no` (normal timed shutdown may increment backend device-stop telemetry);
- timestamped evidence report under `build-phase9c-windows`.

Optional real capture:

```powershell
.\tools\windows-phase9c-rc-torture.ps1 `
  -OutputId '<PLAYBACK-ID>' `
  -InputId '<CAPTURE-ID>' `
  -RunLiveInput
```

Prevent acoustic feedback.

## Clean-machine RC verification

Copy these four files to a Windows 11 system without Qt or the repository:

```text
FV1Lab-<version>-windows-x64.zip
FV1Lab-<version>-windows-x64.zip.sha256
FV1Lab-<version>-windows-x64.zip.manifest.json
windows-phase9c-clean-machine.ps1
```

Run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass

.\windows-phase9c-clean-machine.ps1 `
  -ZipPath .\FV1Lab-<version>-windows-x64.zip `
  -KeepExtracted
```

Expected:

```text
=== PHASE 9C CLEAN-MACHINE RC VERIFICATION PASSED ===
```

Then launch the retained `bin\FV1Lab.exe` and perform the remaining interactive
GUI checks from `WINDOWS-PHASE9C-CHECKLIST.md`.

## What remains manual

Automation cannot truthfully prove:

- visual correctness on real 100/125/150/200% displays;
- mixed-DPI monitor movement;
- actual dock/splitter usability;
- human-visible splash/About/menu/tool-tip polish;
- interactive 100-click GUI Start/Stop behavior;
- endpoint unplug/replug UX;
- audible live-I/O correctness;
- recording/export usability;
- final clean-machine human launch.

## Commit boundary

Commit this checkpoint after:

1. `-Quick` passes;
2. full torture passes;
3. clean-machine verifier passes.

This checkpoint was accepted while the product still reported `1.0.0-rc1`.

Final version promotion and release tagging are handled by Phase 9C.2.
