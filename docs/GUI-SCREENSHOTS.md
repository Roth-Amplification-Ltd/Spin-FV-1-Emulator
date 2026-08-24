# FV-1 Lab GUI Screenshots

This page keeps README/platform screenshots tied to **actual running
applications**, not generated mockups.

## FV-1 Lab 1.0 documentation baseline

The canonical Linux, macOS and Windows captures below are the documentation
baseline for the completed `v1.0.0` desktop line. They are real running-app
captures and remain the README comparison set until a later release replaces
them intentionally.

## Canonical files

```text
docs/media/fv1-lab-linux-current.png
docs/media/fv1-lab-macos-current.png
docs/media/fv1-lab-windows-current.png
```

## Recovered captures in this refresh

### Linux

Latest actual Linux FV-1 Lab capture recovered from accessible project/chat
history:

- captured August 15, 2026;
- real Qt 6 FV-1 Lab on Linux;
- full engineering dashboard, with About visible.

The refresh bundle installs this as `fv1-lab-linux-current.png`.

### Windows

Latest actual Windows FV-1 Lab application capture recovered from the Windows
port work:

- captured August 18, 2026;
- real MSVC/Qt FV-1 Lab on Windows 11;
- shared Linux/Windows Qt dashboard.

The refresh bundle installs this as `fv1-lab-windows-current.png`.

### macOS

The canonical macOS documentation set was refreshed August 20, 2026 from the
completed native Phase 8D application.

Primary README hero:

```text
docs/media/fv1-lab-macos-current.png
```

The hero is the active Oscilloscope view with `00_55_gallon_saint.spn` loaded,
Test Generator at 440 Hz, RAW + PROCESSED enabled, AUDIO RUNNING, populated
Delay RAM/resource usage, and the native Console / Offline FV-1 Chip Inspector.

Detailed macOS views:

```text
docs/media/fv1-lab-macos-interface.png
docs/media/fv1-lab-macos-oscilloscope.png
docs/media/fv1-lab-macos-spectrum.png
docs/media/fv1-lab-macos-spectrogram.png
docs/media/fv1-lab-macos-levels.png
docs/media/fv1-lab-macos-validation.png
```

All images are actual running-application captures. The startup splash is not
required for this documentation pass and can be added later as a secondary
image.

## Standard hero state

For a clean three-platform comparison, use this state where practical:

- Dark theme;
- Cyan accent;
- `examples/steal-this-dsp-programs/02_ghost_spring.spn`;
- Test Generator;
- sine at 440 Hz;
- RAW + FX overlay;
- Oscilloscope tab;
- audio running long enough for stable traces;
- entire main window visible;
- no terminal/error dialog obscuring the app.

Exact CPU/load numbers do not need to match.

## Capture — Linux

Where available:

```bash
gnome-screenshot -w -f ~/Desktop/fv1-lab-linux-current.png
```

Otherwise use the desktop screenshot UI and select the FV-1 Lab window.

## Capture — macOS

Bring FV-1 Lab to the front:

```bash
screencapture -iW ~/Desktop/fv1-lab-macos-current.png
```

Click the FV-1 Lab window when prompted.

## Capture — Windows 11

Bring FV-1 Lab to the front and use **Alt + Print Screen**, or use
**Win + Shift + S → Window mode**. Save as:

```text
fv1-lab-windows-current.png
```

## Rules

- actual runtime screenshots only;
- no generated/mockup UI as release documentation;
- preserve application chrome so the platform is identifiable;
- avoid secrets/private keys;
- prefer a running analyzer view over an empty dashboard;
- replace canonical filenames rather than adding date-stamped README links.
