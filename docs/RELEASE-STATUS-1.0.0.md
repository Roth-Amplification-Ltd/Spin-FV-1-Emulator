# FV-1 Lab 1.0.0 Release Closure Record

**Closed:** August 24, 2026  
**Tag:** `v1.0.0`  
**Release commit:** `6bcab5966d71520a7321178f116352b3ad347fef`

This document is a post-tag closure record. It intentionally lives on `main`
after the immutable `v1.0.0` tag and must **not** be used as a reason to move or
recreate that tag.

## Product closure

```text
Linux     — Qt 6 FV-1 Lab            COMPLETE
macOS     — Native SwiftUI FV-1 Lab  COMPLETE (Phase 8D)
Windows   — Qt 6 FV-1 Lab            COMPLETE (Phase 9C)
```

The standalone desktop-porting roadmap is closed at 1.0.0.

## Frozen release contracts

The following are release-stable boundaries:

- FV-1 execution semantics and observable virtual-chip state;
- FV1SDK public C ABI v1 and its opaque-handle ownership rules;
- manual-Start behavior: loading/compiling a program does not start audio;
- realtime discipline after host/session construction: no GUI work, filesystem
  access, logging, blocking locks or unbounded allocation in the audio callback;
- platform frontends remain consumers of the virtual chip/runtime rather than
  owners of emulator semantics.

A bug found against documented behavior can be fixed. A deliberate incompatible
semantic or ABI change requires an explicit versioning decision rather than
being hidden inside a platform-maintenance patch.

## Windows final evidence

The final Windows release gate accepted:

- exact `1.0.0` binary/manifest/BUILD-INFO agreement;
- clean pushed release commit identity;
- all 43 Windows regression tests;
- packaged GUI smoke coverage;
- all bundled SpinASM programs;
- Unicode and extended-length paths;
- recording/export transactional cleanup;
- 100/125/150/200% DPI coverage;
- stable WASAPI hardware behavior;
- 100 host/runtime lifecycle cycles;
- 1800-second continuous realtime soak;
- zero output underruns;
- zero analyzer drops;
- `device-lost=no`;
- Qt-neutral clean-package verification;
- final human extracted-package launch/visual check.

Final Windows x64 artifact:

```text
FV1Lab-1.0.0-windows-x64.zip
SHA-256: 26a8f2be228afdd46195e1d5f10df5870ca7eab9523d6d68c647b64583d24f26
```

## Artifact provenance rule

Any binary uploaded to the GitHub Release as version **1.0.0** must be built
from `v1.0.0` / `6bcab5966d71520a7321178f116352b3ad347fef` and should carry a
checksum. A later `main` build is not a 1.0.0 release artifact even if its
source changes are documentation-only.

## Deferred fidelity work

Physical FV-1 silicon validation remains independent. Use the existing
validation pack and capture/report tooling when suitable hardware is available.
Measured differences should become minimal regression vectors and model fixes,
never per-effect compatibility hacks.

## Next

The governing post-release plan is [`POST-1.0-ROADMAP.md`](POST-1.0-ROADMAP.md).
