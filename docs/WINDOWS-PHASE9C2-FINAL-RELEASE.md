# Windows Phase 9C.2 — Final 1.0.0 Release Promotion

Phase 9C.2 is the final Windows desktop checkpoint.

It does **not** change the FV-1 execution model, DSP semantics, realtime
architecture or FV1SDK ABI. It turns the accepted `1.0.0-rc1` release candidate
into final `1.0.0`, hardens version/artifact agreement, and makes the release
commit tag-ready.

## Apply + preflight before commit

```powershell
.\tools\windows-phase9c-final-release.ps1 `
  -QtDir "C:\Qt\6.11.1\msvc2022_64" `
  -PreflightOnly
```

Expected:

```text
=== PHASE 9C.2 FINAL VERSION PREFLIGHT PASSED ===
```

The preflight clean-builds Release, runs the Windows regression gate and checks:

```text
fv1-cli.exe  -> Spin FV-1 Emulator 1.0.0
fv1-live.exe -> Spin FV-1 Emulator 1.0.0
FV1Lab.exe   -> FileVersion/ProductVersion 1.0.0
```

Then review `git diff --check`, `git status --short`, and `git diff --stat`.
Stage only Phase 9C.2 source/tooling/documentation files; do not stage build,
dist, logs or extracted packages.

## Final clean release gate

After the promotion commit is pushed:

```powershell
.\tools\windows-phase9c-final-release.ps1 `
  -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

Expected:

```text
=== PHASE 9C.2 WINDOWS 1.0.0 FINAL RELEASE GATE PASSED ===
```

The gate requires clean pushed `main`, exact version agreement, the complete
Phase 9C Release regression, final package/SHA/manifest verification, packaged
GUI smoke, bundled-program package coverage and local clean-machine verification.

## Final human package check

Extract `FV1Lab-1.0.0-windows-x64.zip` into a fresh unrelated directory and
launch `bin\FV1Lab.exe`.

Verify splash/About identify 1.0.0, `.spn` loading remains STOPPED, Start/Stop,
analyzers, Audio Settings, and clean quit/relaunch.

## Tag

Only after all final checks are green:

```powershell
git tag -a v1.0.0 -m "FV-1 Lab 1.0.0"
git push origin v1.0.0
```

The tag must point to the same commit recorded by the release manifest.

Physical FV-1 silicon closure remains a separate fidelity program and is not
implied by desktop release status.
