# FV-1 Lab Platform Status

**Snapshot:** August 24, 2026  
**Release:** `v1.0.0`  
**Release commit:** `6bcab5966d71520a7321178f116352b3ad347fef`

The standalone FV-1 Lab desktop line is complete on all three target desktop
platforms. Historical phase documents remain in `docs/` for implementation and
regression detail.

| Platform | UI | Host audio | 1.0 state | Ongoing work |
|---|---|---|---|---|
| Linux | Qt 6 Widgets | miniaudio/system backend | Complete reference desktop | maintenance + validation |
| macOS | Native SwiftUI | Core Audio / AVAudioEngine | Phase 8D complete | maintenance + release operations |
| Windows 11 | Shared Linux/Windows Qt 6 Widgets | miniaudio/WASAPI | Phase 9C complete; 1.0.0 released | maintenance |

## Immutable 1.0 release identity

The annotated tag `v1.0.0` points to:

```text
6bcab5966d71520a7321178f116352b3ad347fef
```

Do not move or recreate that tag. Documentation commits made after release may
advance `main`; release artifacts must still be traceable to the tagged commit.

## Shared product contract

All shipping FV-1 Lab frontends preserve the same laboratory workflow:

1. load/compile a program;
2. remain stopped until the user explicitly starts audio;
3. choose Test Generator, WAV Loop or Audio Interface;
4. process through the same virtual FV-1 semantics;
5. inspect analyzers, chip state, Delay RAM and resource usage;
6. record/export/validate without making GUI/filesystem code part of the
   virtual-chip ABI.

The **FV-1 execution model** and **FV1SDK ABI v1** are release-stable platform
boundaries. GUI, packaging and host-integration maintenance must not silently
change them.

## Release artifact status

The Windows x64 final artifact was generated and accepted directly from the
release commit:

```text
FV1Lab-1.0.0-windows-x64.zip
SHA-256: 26a8f2be228afdd46195e1d5f10df5870ca7eab9523d6d68c647b64583d24f26
```

Its release gate verified exact manifest/commit/version agreement, a Qt-neutral
portable package, packaged GUI smokes and all bundled SpinASM programs.

Linux package helpers and the macOS DMG/signing/notarization pipeline are
complete. If Linux or macOS binaries are attached to a GitHub Release, build
those artifacts from the exact `v1.0.0` tag and record checksums; do not attach
an artifact built from a later documentation/maintenance commit under the 1.0.0
release name.

## Windows acceptance evidence

Accepted Windows evidence includes real WASAPI full-duplex capture → FV-1 →
playback, 44.1/48 kHz coverage, requested 128/256/512/1024 buffers, Unicode and
>260-character paths, transactional recording/export, 100/125/150/200% DPI
smokes, all bundled SpinASM programs, 100 host lifecycle cycles, a 1800-second
realtime soak with zero output underruns/zero analyzer drops/`device-lost=no`,
and independent clean-package verification.

## Screenshots

Canonical release-documentation captures:

```text
docs/media/fv1-lab-linux-current.png
docs/media/fv1-lab-macos-current.png
docs/media/fv1-lab-windows-current.png
```

See [`GUI-SCREENSHOTS.md`](GUI-SCREENSHOTS.md).
