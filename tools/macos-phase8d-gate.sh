#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${FV1_APPLE_HOST_TEST_BUILD_DIR:-$ROOT/build-phase8-apple-host}"
SOAK_SECONDS="${FV1_MACOS_SOAK_SECONDS:-60}"
PACKAGE_MODE="${FV1_MACOS_PACKAGE_MODE:-adhoc}"
REPORT_DIR="${FV1_PHASE8D_REPORT_DIR:-$ROOT/build-phase8d-macos}"
PROJECT="$ROOT/apple/FV1Lab.xcodeproj"
SCHEME="FV1 Lab macOS"

need() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "error: Phase 8D gate requires '$1'" >&2
        exit 2
    }
}

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: Phase 8D macOS gate must run on macOS" >&2
    exit 2
fi

for tool in git python3 cmake ninja xcodebuild plutil otool codesign; do
    need "$tool"
done

mkdir -p "$REPORT_DIR"
REPORT="$REPORT_DIR/phase8d-gate.txt"
: > "$REPORT"

log() {
    printf '%s\n' "$*" | tee -a "$REPORT"
}

run() {
    log ""
    log "\$ $*"
    "$@" 2>&1 | tee -a "$REPORT"
}

log "FV-1 Lab macOS Phase 8D completion gate"
log "======================================="
log "Date: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
log "Commit: $(git -C "$ROOT" rev-parse HEAD)"
log "Soak seconds: $SOAK_SECONDS"
log "Package mode: $PACKAGE_MODE"

run git -C "$ROOT" diff --check

run "$ROOT/tools/test-apple.sh" host

run cmake --build "$BUILD_DIR" --parallel \
    --target \
    fv1-apple-release-program-tests \
    fv1-apple-realtime-soak

run "$BUILD_DIR/fv1-apple-release-program-tests" "$ROOT"

log ""
log "=== Accelerated Apple realtime bridge soak ==="
FV1_APPLE_SOAK_SECONDS="$SOAK_SECONDS" \
    "$BUILD_DIR/fv1-apple-realtime-soak" 2>&1 |
    tee -a "$REPORT"

run "$ROOT/tools/build-apple.sh" macos Debug
run "$ROOT/tools/build-apple.sh" macos Release

SETTINGS="$(
    xcodebuild \
      -project "$PROJECT" \
      -scheme "$SCHEME" \
      -configuration Release \
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

APP="$TARGET_BUILD_DIR/$FULL_PRODUCT_NAME"

run "$ROOT/tools/verify-macos-bundle.sh" "$APP"

run "$ROOT/tools/package-macos.sh" "$PACKAGE_MODE"

log ""
log "=== AUTOMATED PHASE 8D MACOS GATE PASSED ==="
log ""
log "Automated coverage:"
log "  - full host CTest suite"
log "  - Apple frontend boundary"
log "  - all shipped SpinASM demo programs"
log "  - accelerated Apple realtime bridge soak"
log "  - macOS Debug build"
log "  - macOS Release build"
log "  - bundle/linkage validation"
log "  - DMG packaging"
log ""
log "Still required before declaring a production release:"
log "  - real Core Audio live-soak checklist"
log "  - final visual regression checklist"
log "  - Developer ID signing + notarization/stapling for public distribution"
log ""
log "Report: $REPORT"
