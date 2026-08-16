#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-host}"
CONFIG="${2:-Debug}"
BUILD_DIR="${FV1_APPLE_HOST_TEST_BUILD_DIR:-$ROOT/build-phase8-apple-host}"

python3 "$ROOT/tools/generate_apple_xcodeproj.py"
python3 "$ROOT/tools/check_apple_frontend_boundary.py"

run_host() {
    cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DFV1_BUILD_TESTS=ON \
      -DFV1_BUILD_GUI=OFF \
      -DFV1_ENABLE_LIVE_AUDIO=OFF \
      -DFV1_BUILD_WINDOWS_FRONTEND=OFF

    cmake --build "$BUILD_DIR" --parallel
    ctest --test-dir "$BUILD_DIR" --output-on-failure
}

require_macos() {
    if [[ "$(uname -s)" != "Darwin" ]]; then
        echo "error: '$MODE' requires macOS/Xcode" >&2
        exit 2
    fi
}

case "$MODE" in
    host)
        run_host
        ;;
    macos)
        run_host
        require_macos
        "$ROOT/tools/build-apple.sh" macos "$CONFIG"
        ;;
    ipados|ipad)
        run_host
        require_macos
        "$ROOT/tools/build-apple.sh" ipados "$CONFIG"
        ;;
    all)
        run_host
        require_macos
        "$ROOT/tools/build-apple.sh" macos "$CONFIG"
        "$ROOT/tools/build-apple.sh" ipados "$CONFIG"
        ;;
    *)
        echo "usage: $0 [host|macos|ipados|all] [Debug|Release]" >&2
        exit 2
        ;;
esac
