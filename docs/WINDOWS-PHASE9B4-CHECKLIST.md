# Windows Phase 9B.4 — DPI + Desktop Polish Acceptance

Phase 9B.4 hardens the shipping **Qt 6 Widgets FV-1 Lab** desktop experience on
Windows 11. It does not change the FV-1 execution model or FV1SDK ABI.

## Automated gate

```powershell
Set-ExecutionPolicy -Scope Process Bypass
```

```powershell
.\tools\test-windows.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

Phase 9B.4 adds desktop smokes at 100%, 125%, 150% and 200%.

Then run:

```powershell
.\tools\windows-dpi-acceptance.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

Expected:

```text
PHASE 9B.4 AUTOMATED DPI ACCEPTANCE PASSED
```

## Manual baseline

```powershell
& "$HOME\GitHub\Spin-FV-1-Emulator\build-windows\Debug\FV1Lab.exe"
```

Load `examples\steal-this-dsp-programs\00_55_gallon_saint.spn`, select Test
Generator, enable RAW + FX overlay and start audio.

Opening/reloading a program must still leave audio stopped until Start is
pressed.

## Scaling matrix

Test Windows display scaling at 100%, 125%, 150% and 200%.

At each scale verify:

- main window opens on-screen and fits the usable desktop;
- menu/toolbar text is crisp and unclipped;
- PROGRAM / INPUT SOURCE / FV-1 PARAMETERS / AUDIO controls remain usable;
- left/right docks resize and dock normally;
- all analyzer/Validation tabs remain reachable;
- Delay RAM / resource / DSP status rows do not overlap;
- Console and Offline Chip Inspector remain legible;
- combo boxes/spin boxes/sliders/progress bars retain usable hit targets;
- status bar text is not clipped;
- context menus/tooltips open on the correct monitor;
- file dialogs remain usable;
- ordinary window movement does not cause audio underruns or analyzer drops.

## Splash + About

At each practical scale verify startup splash and Help → About FV-1 Lab:

- fit entirely inside the usable screen;
- remain centered on the FV-1 Lab monitor;
- preserve the 16:9 composition;
- keep progress/version/credit text visible.

## Workspace persistence

At 100%, move/resize the main window and change dock widths, then quit.

Relaunch and verify geometry/docks restore.

Change Windows scaling and relaunch. The workspace may be constrained to the
new usable desktop but must not reopen fully off-screen.

Use View → Reset Workspace Layout and verify it fits/centers at the current
scale.

## Mixed-DPI monitors

If two monitors are available, configure different scales.

With audio running:

1. drag FV-1 Lab from monitor A to B;
2. pause with the window spanning both;
3. finish the move to B;
4. maximize/restore;
5. drag back.

Verify crisp redraw, no off-screen jump, no dock overlap, no crash, realtime
audio continuity and analyzer continuity.

Use Help → Copy Desktop Diagnostics before/after the move. Confirm screen name,
logical DPI, DPR and available logical geometry change appropriately.

## Taskbar / Alt-Tab identity

Test View → Application Icon for Silver, Dark Cyan, Blue and Amber.

Verify title-bar/taskbar/Alt-Tab identity is FV-1 Lab and the selected icon
persists after relaunch. Windows may briefly cache shell imagery.

## Menus / keyboard

Verify:

- Ctrl+O — Open FV-1 Program;
- Ctrl+Shift+O — Open Audio Loop;
- Ctrl+Shift+V — Paste SpinASM;
- F5 — Start;
- Shift+F5 — Stop;
- Ctrl+, — Audio Settings;
- Alt menu navigation;
- Alt+F4 clean shutdown.

## Dialog stress

Exercise Audio Settings, Test Generator Settings, Audio Loop Region, Paste
SpinASM, recording destination, analyzer image/CSV export, Validation dialogs,
and About. No dialog should exceed the usable screen at high scaling.

## Theme visual pass

At minimum inspect Dark, Light, Midnight, Amber CRT, Green Phosphor, Slate and
High Contrast at 125% and 150%.

## Pass criteria

Commit Phase 9B.4 when:

- complete Windows tests are green;
- automated DPI acceptance passes all four scale factors;
- manual 100/125/150/200 checks are acceptable;
- mixed-DPI movement is acceptable where hardware permits;
- splash/About fit;
- geometry/dock persistence survives scaling changes;
- taskbar/Alt-Tab identity is acceptable;
- no realtime regression is observed;
- `git diff --check` is clean.

After 9B.4, the remaining Windows phase is **Phase 9C — Completion / Release**.
