# Linux build, launch and packaging

`linux.sh` is the product-facing Linux helper. It deliberately uses the same
CMake targets/install rules as the normal developer build so `.deb`, AppImage,
Flatpak and native launches do not become separate application variants.

## First-time setup

```bash
./linux.sh deps
```

This reuses `bootstrap-dev.sh` for the compiler, Qt 6, SpeexDSP and miniaudio
requirements, then installs the package-building tools used by the three Linux
formats. It also configures Flathub and the Qt/KDE Flatpak SDK for the current
manifest runtime.

## Native build and launch

```bash
./linux.sh build
./linux.sh run
```

Run the full regression suite with:

```bash
./linux.sh test
```

Install a user-local desktop copy under `~/.local` with:

```bash
./linux.sh install
```

## Packages

Create every supported Linux package:

```bash
./linux.sh package all
```

Or build a single format:

```bash
./linux.sh package deb
./linux.sh package appimage
./linux.sh package flatpak
```

Artifacts are written to `dist/`.

### Debian / Ubuntu / Pop!_OS package

The `.deb` is built from a staged CMake install tree. `dpkg-shlibdeps` derives the host's exact shared-library dependencies. The
helper deliberately fails rather than emitting a guessed dependency list when
the package database cannot identify the linked libraries.
The SDK development headers/static library are removed from the desktop package.

### AppImage

The AppImage is built from the same staged install tree using pinned
`linuxdeploy` and `linuxdeploy-plugin-qt` releases. The Qt plugin bundles Qt 6
libraries/resources and linuxdeploy produces the final AppImage. The helper sets
`APPIMAGE_EXTRACT_AND_RUN=1`, so the packaging tools themselves do not require a
working FUSE 2 setup on modern Ubuntu/Pop!_OS hosts. For wide binary
compatibility, make public AppImage releases on the oldest Linux distribution
you intend to support; the local helper packages the binaries built on its host.

### Flatpak

The Flatpak manifest is `packaging/linux/com.rothamplification.FV1Lab.yml` and
uses the KDE/Qt runtime. The manifest consumes the *current local working tree*,
not a fresh GitHub clone, so local tested changes are exactly what gets
packaged. A pinned miniaudio source is installed into the build sandbox.

Build, install for the current user and immediately launch the sandboxed app:

```bash
./linux.sh flatpak-run
```

## Cleanup

```bash
./linux.sh clean
```
