#!/usr/bin/env bash
# Phase 6C heavyweight release-candidate verification gate.
# This intentionally lives outside the fast bootstrap/CTest path so normal
# developer builds stay quick while release candidates receive deeper abuse.
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${FV1_RELEASE_GATE_DIR:-$ROOT/build-release-gate}"
FUZZ_RUNS="${FV1_FUZZ_RUNS:-5000}"
JOBS="${FV1_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

need() {
    command -v "$1" >/dev/null 2>&1 || { echo "error: Phase 6C release gate requires '$1'" >&2; exit 2; }
}
for tool in cmake ninja gcc g++ clang clang++ python3; do need "$tool"; done

rm -rf -- "$WORK"
mkdir -p -- "$WORK"

configure_build_test() {
    local name="$1" cc="$2" cxx="$3"; shift 3
    local dir="$WORK/$name"
    echo
    echo "=== $name: configure ==="
    CC="$cc" CXX="$cxx" cmake -S "$ROOT" -B "$dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DFV1_BUILD_GUI=OFF \
        -DFV1_ENABLE_LIVE_AUDIO=OFF \
        "$@"
    echo "=== $name: build ==="
    cmake --build "$dir" --parallel "$JOBS"
    echo "=== $name: tests ==="
    ctest --test-dir "$dir" --output-on-failure
}

configure_build_test gcc gcc g++

configure_build_test sdk-shared gcc g++ \
    -DFV1_SDK_ONLY=ON -DFV1_SDK_BUILD_SHARED=ON

configure_build_test sdk-static gcc g++ \
    -DFV1_SDK_ONLY=ON -DFV1_SDK_BUILD_SHARED=OFF

SAN_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
echo
echo "=== clang-asan-ubsan: configure ==="
CC=clang CXX=clang++ cmake -S "$ROOT" -B "$WORK/clang-sanitize" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DFV1_BUILD_GUI=OFF \
    -DFV1_ENABLE_LIVE_AUDIO=OFF \
    -DFV1_SDK_BUILD_SHARED=OFF \
    -DCMAKE_C_FLAGS="$SAN_FLAGS" \
    -DCMAKE_CXX_FLAGS="$SAN_FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="$SAN_FLAGS" \
    -DCMAKE_SHARED_LINKER_FLAGS="$SAN_FLAGS"
cmake --build "$WORK/clang-sanitize" --parallel "$JOBS"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    ctest --test-dir "$WORK/clang-sanitize" --output-on-failure

echo
echo "=== clang libFuzzer targets ==="
CC=clang CXX=clang++ cmake -S "$ROOT" -B "$WORK/fuzz" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DFV1_BUILD_GUI=OFF \
    -DFV1_ENABLE_LIVE_AUDIO=OFF \
    -DFV1_BUILD_TESTS=OFF \
    -DFV1_BUILD_FUZZERS=ON
cmake --build "$WORK/fuzz" --parallel "$JOBS" --target \
    fv1-conformance-fuzzer fv1-spinasm-fuzzer fv1-sdk-fuzzer

mkdir -p "$WORK/fuzz-corpus/conformance" "$WORK/fuzz-corpus/spinasm"
# A legal 512-byte all-zero program makes the differential fuzzer enter the
# execution model immediately instead of spending a smoke run below its minimum
# program length. SpinASM starts from one real source seed.
python3 - <<'PY_FUZZ_SEED' "$WORK/fuzz-corpus/conformance/zero.bin"
from pathlib import Path
import sys
Path(sys.argv[1]).write_bytes(bytes(512))
PY_FUZZ_SEED
printf 'RDAX ADCL, 1.0\nWRAX DACL, 0\n' > "$WORK/fuzz-corpus/spinasm/passthrough.spn"

for fuzzer in fv1-conformance-fuzzer fv1-spinasm-fuzzer fv1-sdk-fuzzer; do
    echo "=== $fuzzer: $FUZZ_RUNS runs ==="
    if [[ "$fuzzer" == fv1-conformance-fuzzer ]]; then
        "$WORK/fuzz/$fuzzer" -runs="$FUZZ_RUNS" -max_len=521 "$WORK/fuzz-corpus/conformance"
    elif [[ "$fuzzer" == fv1-spinasm-fuzzer ]]; then
        "$WORK/fuzz/$fuzzer" -runs="$FUZZ_RUNS" -max_len=65536 "$WORK/fuzz-corpus/spinasm"
    else
        "$WORK/fuzz/$fuzzer" -runs="$FUZZ_RUNS" -max_len=4096
    fi
done

if command -v valgrind >/dev/null 2>&1; then
    echo
    echo "=== Valgrind SDK abuse smoke ==="
    valgrind --quiet --error-exitcode=99 --leak-check=full \
        "$WORK/gcc/fv1-sdk-abuse-tests"
else
    echo
    echo "note: valgrind not installed; skipping optional Valgrind smoke"
fi

echo
echo "=== PHASE 6C RELEASE GATE PASSED ==="
echo "Fuzz iterations per target: $FUZZ_RUNS"
echo "Artifacts: $WORK"
