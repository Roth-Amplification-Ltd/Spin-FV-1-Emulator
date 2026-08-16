#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$ROOT/apple/FV1Lab.xcodeproj"
SCHEME="FV1 Lab macOS"
CONFIG="Release"
DIST="${FV1_MACOS_DIST_DIR:-$ROOT/dist/macos}"
MODE="${1:-adhoc}"

need() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "error: macOS packaging requires '$1'" >&2
        exit 2
    }
}

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: macOS packaging requires macOS" >&2
    exit 2
fi

for tool in xcodebuild codesign hdiutil plutil shasum ditto; do
    need "$tool"
done

case "$MODE" in
    unsigned|adhoc|developer-id) ;;
    *)
        echo "usage: $0 [unsigned|adhoc|developer-id]" >&2
        exit 2
        ;;
esac

if [[ "$MODE" == "developer-id" && -z "${FV1_CODESIGN_IDENTITY:-}" ]]; then
    echo "error: developer-id mode requires FV1_CODESIGN_IDENTITY" >&2
    exit 2
fi

rm -rf "$DIST"
mkdir -p "$DIST"

"$ROOT/tools/build-apple.sh" macos "$CONFIG"

SETTINGS="$(
    xcodebuild \
      -project "$PROJECT" \
      -scheme "$SCHEME" \
      -configuration "$CONFIG" \
      -destination 'platform=macOS' \
      -showBuildSettings
)"

TARGET_BUILD_DIR="$(
    printf '%s\n' "$SETTINGS" |
    awk -F ' = ' '/^[[:space:]]*TARGET_BUILD_DIR = / {print $2; exit}'
)"

FULL_PRODUCT_NAME="$(
    printf '%s\n' "$SETTINGS" |
    awk -F ' = ' '/^[[:space:]]*FULL_PRODUCT_NAME = / {print $2; exit}'
)"

if [[ -z "$TARGET_BUILD_DIR" || -z "$FULL_PRODUCT_NAME" ]]; then
    echo "error: unable to resolve Xcode product path" >&2
    exit 1
fi

SOURCE_APP="$TARGET_BUILD_DIR/$FULL_PRODUCT_NAME"
if [[ ! -d "$SOURCE_APP" ]]; then
    echo "error: built app not found: $SOURCE_APP" >&2
    exit 1
fi

VERSION="$(
    /usr/libexec/PlistBuddy \
      -c 'Print :CFBundleShortVersionString' \
      "$SOURCE_APP/Contents/Info.plist" \
      2>/dev/null || echo "unknown"
)"

STAGE="$DIST/stage"
VOLUME="$DIST/volume"
APP="$VOLUME/FV-1 Lab.app"

mkdir -p "$STAGE" "$VOLUME"
ditto "$SOURCE_APP" "$APP"
ln -s /Applications "$VOLUME/Applications"

xattr -cr "$APP"

case "$MODE" in
    unsigned)
        echo "Packaging unsigned app."
        ;;
    adhoc)
        codesign \
          --force \
          --deep \
          --sign - \
          "$APP"
        codesign \
          --verify \
          --deep \
          --strict \
          --verbose=2 \
          "$APP"
        ;;
    developer-id)
        codesign \
          --force \
          --deep \
          --options runtime \
          --timestamp \
          --sign "$FV1_CODESIGN_IDENTITY" \
          "$APP"

        codesign \
          --verify \
          --deep \
          --strict \
          --verbose=2 \
          "$APP"
        ;;
esac

DMG="$DIST/FV1-Lab-${VERSION}-macOS.dmg"

hdiutil create \
  -volname "FV-1 Lab" \
  -srcfolder "$VOLUME" \
  -ov \
  -format UDZO \
  "$DMG"

if [[ "$MODE" == "developer-id" ]]; then
    codesign \
      --force \
      --timestamp \
      --sign "$FV1_CODESIGN_IDENTITY" \
      "$DMG"

    if [[ -n "${FV1_NOTARY_PROFILE:-}" ]]; then
        need xcrun

        echo
        echo "Submitting DMG to Apple notarization service…"
        xcrun notarytool submit \
          "$DMG" \
          --keychain-profile "$FV1_NOTARY_PROFILE" \
          --wait

        xcrun stapler staple "$DMG"
        xcrun stapler validate "$DMG"
    else
        echo
        echo "note: FV1_NOTARY_PROFILE is not set; notarization skipped."
    fi
fi

plutil -lint "$APP/Contents/Info.plist"

SHA_FILE="$DMG.sha256"
shasum -a 256 "$DMG" > "$SHA_FILE"

echo
echo "macOS package complete"
echo "  Mode: $MODE"
echo "  App:  $APP"
echo "  DMG:  $DMG"
echo "  SHA:  $SHA_FILE"
