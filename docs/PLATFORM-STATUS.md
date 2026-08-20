# FV-1 Lab Platform Status

**Snapshot:** August 20, 2026

This is the short authoritative desktop-platform status. Historical phase
documents remain in `docs/` for implementation detail and regression history.

| Platform | UI | Host audio | State | Next |
|---|---|---|---|---|
| Linux | Qt 6 Widgets | miniaudio/system backend | Feature-complete desktop reference | regression/release maintenance |
| macOS | Native SwiftUI | Core Audio / AVAudioEngine | Phase 8D complete | release maintenance |
| Windows 11 | Same Qt 6 Widgets UI as Linux | miniaudio/WASAPI | Phase 9B.3 complete | Phase 9B.4 DPI/Desktop Polish |

## Shared product contract

All shipping FV-1 Lab frontends preserve the same laboratory workflow:

1. load/compile a program;
2. remain stopped until the user explicitly starts audio;
3. choose Test Generator, WAV Loop or Audio Interface;
4. process through the same virtual FV-1 semantics;
5. inspect analyzers, chip state, Delay RAM and resource usage;
6. record/export/validate without making GUI/filesystem code part of the
   virtual-chip ABI.

The FV-1 execution model and FV1SDK ABI are platform boundaries. Desktop polish
must not destabilize them.

## Linux

Linux remains the reference implementation of the Qt 6 desktop FV-1 Lab.
Windows shares this frontend source.

## macOS

- Phase 8B — Mac Testbench Parity — complete;
- Phase 8C — Mac Workflow + Inspector Parity — complete;
- Phase 8D — Mac Completion / Release — complete.

The Mac uses SwiftUI plus native Apple audio integration while preserving the
same manual-start and virtual-chip behavior.

## Windows

Completed:

- Phase 9A — Qt Desktop Parity;
- Phase 9B.0 — Workflow/platform foundation;
- Phase 9B.2 — WASAPI hardware hardening;
- Phase 9B.3 — Unicode + recording/export hardening.

Current Windows evidence includes native MSVC/Qt builds, the 39/39 regression
gate, real WASAPI full-duplex capture → FV-1 → playback, 44.1/48 kHz coverage,
requested 128/256/512/1024 buffers, portable package verification, Unicode
workflow acceptance and >260-character `.spn`/`.wav` acceptance.

Next: **Phase 9B.4 — DPI + Windows Desktop Polish**.

Final Windows phase: **Phase 9C — Windows Completion / Release**.

## Screenshots

Canonical screenshot names:

```text
docs/media/fv1-lab-linux-current.png
docs/media/fv1-lab-macos-current.png
docs/media/fv1-lab-windows-current.png
```

See [`GUI-SCREENSHOTS.md`](GUI-SCREENSHOTS.md).
