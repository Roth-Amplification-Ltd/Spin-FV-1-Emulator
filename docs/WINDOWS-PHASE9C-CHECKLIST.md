# Windows Phase 9C — Completion / Release Checklist

Phase 9C is the final Windows regression/release phase. It adds no emulator
features and does not change FV1SDK ABI.

## Phase 9C.0 — automated release gate — complete

Accepted coverage includes clean Release regression, Unicode/long-path
filesystem acceptance, DPI acceptance, packaging, SHA-256/manifest generation,
portable isolation, packaged GUI smoke and all bundled SpinASM programs.

## Phase 9C.1 — RC torture + clean-package acceptance — complete

Accepted evidence:

- every bundled `.spn` through realtime;
- 100 host/runtime construction/start/stop/destruction cycles;
- 1800-second continuous realtime soak;
- zero output underruns;
- zero analyzer drops;
- `device-lost=no`;
- independent portable package verification and human extracted-app smoke.

Normal timed shutdown can increment backend `device stops`; unexpected loss is
represented by `device-lost=yes` and is the failure condition.

## Phase 9C.2 — final 1.0.0 promotion

Preflight the uncommitted final-version change:

```powershell
.\tools\windows-phase9c-final-release.ps1 `
  -QtDir "C:\Qt\6.11.1\msvc2022_64" `
  -PreflightOnly
```

Review and commit/push the promotion only after the preflight passes.

Then run the final release gate from clean pushed `main`:

```powershell
.\tools\windows-phase9c-final-release.ps1 `
  -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

Expected:

```text
=== PHASE 9C.2 WINDOWS 1.0.0 FINAL RELEASE GATE PASSED ===
```

## Final artifact contract

```text
dist\windows\FV1Lab-1.0.0-windows-x64.zip
dist\windows\FV1Lab-1.0.0-windows-x64.zip.sha256
dist\windows\FV1Lab-1.0.0-windows-x64.zip.manifest.json
```

The final gate requires clean `main`, pushed commit identity, exact `1.0.0`
agreement across binaries/BUILD-INFO/manifest, SHA-256 agreement, no build
debris, packaged GUI smokes, all bundled `.spn` package coverage and local
clean-machine verification.

## Final human closure

Before tagging, launch the freshly generated extracted package and verify splash
and About identify 1.0.0, program loading remains STOPPED, Start/Stop works,
analyzers work, Audio Settings opens, and quit/relaunch is clean.

Then:

```powershell
git tag -a v1.0.0 -m "FV-1 Lab 1.0.0"
git push origin v1.0.0
```

Release notes: `docs/RELEASE-1.0.0.md`.
