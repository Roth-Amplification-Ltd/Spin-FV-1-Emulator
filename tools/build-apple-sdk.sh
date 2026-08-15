#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${FV1_APPLE_SDK_BUILD_DIR:-$ROOT/build-apple-sdk}"
CONFIG="${1:-RelWithDebInfo}"

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DFV1_SDK_ONLY=ON \
  -DFV1_BUILD_TESTS=ON \
  -DFV1_BUILD_GUI=OFF \
  -DFV1_ENABLE_LIVE_AUDIO=OFF \
  -DFV1_SDK_BUILD_SHARED=ON
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure
