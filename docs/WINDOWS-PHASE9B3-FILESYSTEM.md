# Windows Phase 9B.3 — Unicode + Recording/Export Hardening

Base checkpoint:

- `266b8cc` — Harden Windows WASAPI device handling
- Windows frontend: shared Qt 6 Widgets FV-1 Lab
- Windows audio: miniaudio / WASAPI
- FV-1 execution model: unchanged
- FV1SDK C ABI: unchanged

Phase 9B.3 hardens every user-facing file boundary that is practical to exercise
on Windows before the DPI/release-polish pass.

## Goals

### Unicode-safe paths

The Windows frontend already converts `QString` paths into native
`std::filesystem::path` values. Phase 9B.3 carries that discipline through the
shared recorder and validation writers, removing narrow `path.string()` path
construction from output-name generation.

Target examples:

```text
C:\Users\adam\Desktop\FV1-Ünicode-測試\
C:\Users\adam\Desktop\Áudio Projects\
C:\Users\adam\Desktop\日本語\FV-1\
```

### Transactional recording finalization

Realtime audio is still pushed only into fixed-capacity queues. The recorder
worker writes to a same-directory `.partial-*` file.

On normal Stop:

1. queued frames drain;
2. the final RIFF/WAVE header is written;
3. the stream is closed;
4. the temporary file is atomically/best-effort-replaced into the requested
   final name from the non-realtime thread.

A failed preparation/start/finalization removes the temporary artifact and
reports the failure instead of presenting a half-written final WAV as complete.

### Transactional validation output

Validation WAVs and report-bundle members are staged before becoming final
files. The report bundle writes:

- JSON;
- Markdown;
- frequency CSV;
- residual WAV.

Each final file is replaced only after its staged file is complete, so a write
failure cannot leave a truncated final member masquerading as a completed
report.

### Collision-resistant recording suggestions

FV-1 Lab now proposes timestamped recording names such as:

```text
fv1-capture-20260819-013355.wav
```

The user still controls the final path through the native Save dialog.

### Remembered validation directories

The validation tab remembers directories independently for:

- reference WAVs;
- capture WAVs;
- report bundles;
- generated stimuli;
- hardware validation packs.

## Automated test procedure

First run the complete Windows regression gate:

```powershell
Set-ExecutionPolicy -Scope Process Bypass

.\tools\test-windows.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

Then run:

```powershell
.\tools\windows-filesystem-acceptance.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

The filesystem acceptance script creates disposable Unicode and >260-character
path trees and proves that the current Debug FV-1 Lab can `--smoke-open` both
SpinASM and WAV files from those locations. It also reruns the recorder and
validation tests that now contain Unicode output coverage.

## Manual procedure

Use `docs/WINDOWS-PHASE9B3-CHECKLIST.md`.

Important live checks include:

- normal native file-dialog open from a Unicode directory;
- recent-file reopening from that directory;
- drag/drop from that directory;
- raw, processed and raw+processed recording;
- Stop finalizes playable WAVs and leaves no `.partial-*` file;
- validation stimulus/report/pack export into Unicode directories;
- error behavior when a destination is intentionally unwritable;
- packaged Release build outside the repository.

## Commit boundary

Once the automated gate and applicable manual checks pass, commit/push Phase
9B.3 before beginning the Windows DPI/desktop-polish phase.
