# FV-1 Lab Platform Status

**Snapshot:** August 22, 2026

This is the short authoritative desktop-platform status. Historical phase
documents remain in `docs/` for implementation detail and regression history.

| Platform | UI | Host audio | State | Next |
|---|---|---|---|---|
| Linux | Qt 6 Widgets | miniaudio/system backend | Feature-complete desktop reference | regression/release maintenance |
| macOS | Native SwiftUI | Core Audio / AVAudioEngine | Phase 8D complete | release maintenance |
| Windows 11 | Same Qt 6 Widgets UI as Linux | miniaudio/WASAPI | Phase 9C final release / 1.0.0 | release maintenance |

## Shared product contract

All shipping FV-1 Lab frontends preserve the same laboratory workflow:

1. load/compile a program;
2. remain stopped until the user explicitly starts audio;
3. choose Test Generator, WAV Loop or Audio Interface;
4. process through the same virtual FV-1 semantics;
5. inspect analyzers, chip state, Delay RAM and resource usage;
6. record/export/validate without making GUI/filesystem code part of the
   virtual-chip ABI.

The FV-1 execution model and FV1SDK ABI are locked platform boundaries.

## Windows release evidence

Completed Windows checkpoints:

- Phase 9A — Qt Desktop Parity;
- Phase 9B.0 — workflow/platform foundation;
- Phase 9B.2 — WASAPI hardware hardening;
- Phase 9B.3 — Unicode + recording/export hardening;
- Phase 9B.4 — DPI + Windows desktop polish;
- Phase 9C.0 — automated Release/package gate;
- Phase 9C.1 — RC torture + clean-package acceptance.

Final 1.0.0 closure promotes the release channel from `rc1` to final, reruns
the full Release gate on a clean pushed commit, requires exact version agreement
across `fv1-cli`, `fv1-live`, `FV1Lab.exe`, BUILD-INFO and the release manifest,
and produces `FV1Lab-1.0.0-windows-x64.zip` plus SHA-256 and JSON manifest.

Accepted Windows evidence includes real WASAPI full-duplex capture → FV-1 →
playback, 44.1/48 kHz coverage, requested 128/256/512/1024 buffers, Unicode and
>260-character paths, transactional recording/export, 100/125/150/200% DPI
smokes, all bundled SpinASM programs, 100 host lifecycle cycles and a
1800-second realtime soak with zero output underruns, zero analyzer drops and
`device-lost=no`.

## Screenshots

Canonical screenshot names:

```text
docs/media/fv1-lab-linux-current.png
docs/media/fv1-lab-macos-current.png
docs/media/fv1-lab-windows-current.png
```

See [`GUI-SCREENSHOTS.md`](GUI-SCREENSHOTS.md).
