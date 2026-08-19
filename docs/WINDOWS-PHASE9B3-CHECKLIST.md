# Windows Phase 9B.3 Manual Acceptance Checklist

Use the freshly built FV-1 Lab after the automated Windows and filesystem gates
are green.

## A. Unicode open/workflow

- [ ] Create or use `C:\Users\<user>\Desktop\FV1-Ünicode-測試\`.
- [ ] Copy a `.spn` program into it with a Unicode filename.
- [ ] Open the program through **File → Open FV-1 Program…**.
- [ ] Program loads and remains **STOPPED** until Start.
- [ ] Reopen it through **Open Recent Program**.
- [ ] Drag/drop the Unicode `.spn` into FV-1 Lab.
- [ ] Copy/load a Unicode-named WAV from the same directory.
- [ ] Reopen it through **Open Recent Audio Loop**.
- [ ] Drag/drop the Unicode WAV.

## B. Recording finalization

Run a realtime session before each recording test.

- [ ] Record **Processed output** into the Unicode directory.
- [ ] Stop recording and verify the final WAV opens/plays.
- [ ] Record **Raw input** into the Unicode directory.
- [ ] Stop recording and verify the final WAV opens/plays.
- [ ] Record **Raw + processed**.
- [ ] Verify both `-raw.wav` and `-processed.wav` retain the Unicode base name.
- [ ] Verify recording telemetry reports zero drops under normal settings.
- [ ] Search the destination after Stop: no `.partial-*` files remain.
- [ ] Confirm the default proposed recording name contains a timestamp.
- [ ] Start/Stop recording repeatedly at least 10 times.

## C. Recording failure behavior

- [ ] Choose a writable destination and confirm normal recording still works.
- [ ] Intentionally choose a destination that cannot be written to.
- [ ] FV-1 Lab reports the error without crashing.
- [ ] No bogus completed final WAV is presented.
- [ ] Subsequent recording to a valid directory still works.

## D. Validation filesystem workflow

In the VALIDATION tab:

- [ ] Load reference/capture WAVs from Unicode paths.
- [ ] Analyze them successfully.
- [ ] Export a report bundle into a Unicode directory.
- [ ] Verify `.json`.
- [ ] Verify `.md`.
- [ ] Verify `-frequency.csv`.
- [ ] Verify `-residual.wav`.
- [ ] Generate a validation stimulus to a Unicode filename.
- [ ] Generate a hardware validation pack inside a Unicode directory.
- [ ] Verify the pack WAVs, `manifest.json`, and `README.txt`.
- [ ] Reopen each dialog and confirm the appropriate directory is remembered.
- [ ] No `.partial-*` artifacts remain after successful exports.

## E. Long-path behavior

The automated script already exercises >260-character `.spn` and `.wav` opens.

- [ ] Manually inspect the generated long-path test if desired using
      `-KeepArtifacts`.
- [ ] Run:
      `.\tools\windows-filesystem-acceptance.ps1 -KeepArtifacts`
      and confirm the printed long path is greater than 260 characters.
- [ ] Clean up the kept test tree afterward.

## F. Packaged Release

After `tools\phase9b-windows-gate.ps1` or the current packaging gate succeeds:

- [ ] Launch the packaged Release build, not the build-tree executable.
- [ ] Open a Unicode `.spn`.
- [ ] Open a Unicode WAV.
- [ ] Make a short recording to a Unicode path.
- [ ] Export a validation report bundle to a Unicode path.
- [ ] Confirm the packaged app does not require the Qt SDK.

## Acceptance

- [ ] All applicable items above pass.
- [ ] `git diff --check` is clean.
- [ ] Windows regression suite is green.
- [ ] `windows-filesystem-acceptance.ps1` is green.
- [ ] Phase 9B.3 is committed and pushed before DPI work begins.
