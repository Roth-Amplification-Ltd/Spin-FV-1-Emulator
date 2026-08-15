#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLATFORM="${1:-all}"
CONFIG="${2:-Debug}"
PROJECT="$ROOT/apple/FV1Lab.xcodeproj"

python3 "$ROOT/tools/generate_apple_xcodeproj.py"
python3 "$ROOT/tools/check_apple_frontend_boundary.py"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "Apple Xcode build requires macOS. The platform-neutral bridge/boundary checks can run on Linux." >&2
  exit 2
fi

if ! command -v xcodebuild >/dev/null 2>&1; then
  echo "xcodebuild not found. Install Xcode and select it with xcode-select." >&2
  exit 2
fi

build_macos() {
  xcodebuild -project "$PROJECT" \
    -scheme "FV1 Lab macOS" \
    -configuration "$CONFIG" \
    -destination 'platform=macOS' \
    CODE_SIGNING_ALLOWED=NO \
    build
}

build_ipados() {
  xcodebuild -project "$PROJECT" \
    -scheme "FV1 Lab iPadOS" \
    -configuration "$CONFIG" \
    -destination 'generic/platform=iOS Simulator' \
    CODE_SIGNING_ALLOWED=NO \
    build
}

case "$PLATFORM" in
  macos) build_macos ;;
  ipados|ipad) build_ipados ;;
  all) build_macos; build_ipados ;;
  *) echo "usage: $0 [all|macos|ipados] [Debug|Release]" >&2; exit 2 ;;
esac
