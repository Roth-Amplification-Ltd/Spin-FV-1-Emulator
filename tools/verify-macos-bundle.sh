#!/usr/bin/env bash
set -Eeuo pipefail

APP="${1:-}"

if [[ -z "$APP" || ! -d "$APP" ]]; then
    echo "usage: $0 '/path/to/FV-1 Lab.app'" >&2
    exit 2
fi

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: bundle verification requires macOS" >&2
    exit 2
fi

INFO="$APP/Contents/Info.plist"
EXECUTABLE="$APP/Contents/MacOS/FV-1 Lab"

[[ -f "$INFO" ]] || {
    echo "error: Info.plist missing" >&2
    exit 1
}

[[ -x "$EXECUTABLE" ]] || {
    echo "error: app executable missing" >&2
    exit 1
}

plutil -lint "$INFO"

BUNDLE_ID="$(
    /usr/libexec/PlistBuddy \
      -c 'Print :CFBundleIdentifier' \
      "$INFO"
)"
VERSION="$(
    /usr/libexec/PlistBuddy \
      -c 'Print :CFBundleShortVersionString' \
      "$INFO"
)"

echo "Bundle:  $BUNDLE_ID"
echo "Version: $VERSION"

echo
echo "Linked libraries:"
LINKS="$(otool -L "$EXECUTABLE")"
printf '%s\n' "$LINKS"

#
# `otool -L` prints one non-indented executable/architecture header for each
# Mach-O slice, followed by indented dependency lines. On a universal binary
# those header lines naturally contain the DerivedData path of the executable
# being inspected. They are *not* runtime dependencies.
#
# Only inspect the indented dependency records for accidental build-machine
# paths. Legitimate dependencies should resolve through system locations or
# relocatable loader tokens such as @rpath/@loader_path/@executable_path.
#
DEPENDENCIES="$(
    printf '%s\n' "$LINKS" |
    awk '/^[[:space:]]+/ { sub(/^[[:space:]]+/, ""); print }'
)"

if printf '%s\n' "$DEPENDENCIES" |
   grep -E '/Users/|DerivedData|build-phase|build-release' >/dev/null; then
    echo "error: executable contains a build-machine dependency path" >&2
    printf '%s\n' "$DEPENDENCIES" |
      grep -E '/Users/|DerivedData|build-phase|build-release' >&2 || true
    exit 1
fi

echo
echo "Architecture:"
lipo -info "$EXECUTABLE" || true

echo
echo "Code signature:"
if codesign --verify --deep --strict "$APP" 2>/dev/null; then
    codesign -dv --verbose=2 "$APP" 2>&1 |
      sed -n '1,20p'
else
    echo "Unsigned (acceptable before packaging/signing gate)."
fi

echo
echo "PASS: macOS app bundle structure/linkage looks release-safe."
