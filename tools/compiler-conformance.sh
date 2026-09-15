#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

COMMAND="${1:-run}"
if [[ $# -gt 0 ]]; then shift; fi

find_cli() {
  local p
  for p in \
    "$ROOT/build/fv1-cli" \
    "$ROOT/build-release/fv1-cli" \
    "$ROOT/build-linux/fv1-cli" \
    "$ROOT/build-compiler-conformance/fv1-cli"
  do
    if [[ -x "$p" ]]; then
      printf '%s\n' "$p"
      return 0
    fi
  done
  return 1
}

if [[ "$COMMAND" == "prepare-official" ]]; then
  exec python3 "$ROOT/tools/compiler_conformance.py" prepare-official \
    --repo "$ROOT" "$@"
fi

CLI="$(find_cli || true)"
if [[ -z "$CLI" ]]; then
  echo "Native fv1-cli not found; building the minimal CLI target..."
  BUILD="$ROOT/build-compiler-conformance"
  GEN=()
  if command -v ninja >/dev/null 2>&1; then
    GEN=(-G Ninja)
  fi
  cmake -S "$ROOT" -B "$BUILD" "${GEN[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DFV1_BUILD_GUI=OFF \
    -DFV1_ENABLE_LIVE_AUDIO=OFF \
    -DFV1_BUILD_TESTS=OFF \
    -DFV1_BUILD_WINDOWS_FRONTEND=OFF
  cmake --build "$BUILD" --target fv1-cli --parallel
  CLI="$(find_cli || true)"
fi

[[ -n "$CLI" && -x "$CLI" ]] || {
  echo "ERROR: unable to locate/build fv1-cli" >&2
  exit 2
}

case "$COMMAND" in
  run)
    exec python3 "$ROOT/tools/compiler_conformance.py" run \
      --repo "$ROOT" --native "$CLI" "$@"
    ;;
  check)
    exec python3 "$ROOT/tools/compiler_conformance.py" check \
      --repo "$ROOT" --native "$CLI" "$@"
    ;;
  *)
    echo "usage: $0 {prepare-official|run|check} [options]" >&2
    exit 2
    ;;
esac
