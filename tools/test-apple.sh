#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${FV1_APPLE_HOST_TEST_BUILD_DIR:-$ROOT/build-phase8-apple-host}"

python3 "$ROOT/tools/generate_apple_xcodeproj.py"
python3 "$ROOT/tools/check_apple_frontend_boundary.py"

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DFV1_BUILD_TESTS=ON \
  -DFV1_BUILD_GUI=OFF \
  -DFV1_ENABLE_LIVE_AUDIO=OFF \
  -DFV1_BUILD_WINDOWS_FRONTEND=OFF
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure

if [[ "$(uname -s)" == "Darwin" ]]; then
  "$ROOT/tools/build-apple.sh" all Debug
else
  echo "Linux host validation complete; Xcode macOS/iPadOS build skipped because this is not macOS."
fi
