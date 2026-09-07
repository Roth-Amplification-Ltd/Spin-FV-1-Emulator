# FV-1 Lab 1.0.1

FV-1 Lab 1.0.1 is a desktop portability and Linux distribution maintenance
release following the 1.0.0 product release.

## Fixed

- Fixed FV-1 Lab window sizing on HiDPI Linux laptop displays.
- Removed layout constraints that could force the main window taller than the
  available logical desktop.
- Made the program/control and virtual-chip inspector docks responsive to the
  current logical screen size.
- Made the left control workspace vertically scrollable on short displays.
- Made the virtual-chip inspector vertically compressible while preserving
  access to the console, debugger and register views.
- Changed the chip-inspector transport controls to a compact two-row layout.
- Made the Validation workspace internally scrollable so its full engineering
  form no longer dictates the minimum size of every analyzer tab.
- Improved workspace restore behavior across changes in logical resolution,
  DPI and device-pixel ratio.
- Added desktop diagnostics for logical geometry, minimum sizes and dock
  dimensions.

## Linux distribution

The automated release publishes:

- Debian/Ubuntu `.deb`
- AppImage
- Flatpak bundle
- Ubuntu 24.04-compatible `.tar.gz`

with SHA-256 checksums and release provenance metadata.

## Validation

The original layout failure was reproduced on a HiDPI Ubuntu/X11 laptop where
Qt reported a 1920x1048 logical desktop at device-pixel ratio 2.

Before the fix the FV-1 Lab minimum window size reached approximately
1450x1015 logical pixels. After the responsive-layout changes it was reduced
to approximately 1135x689, allowing the complete application workspace to fit
normally while retaining native HiDPI rendering.

The packaged Debian build was installed and tested on the reproduction system
before release promotion.
