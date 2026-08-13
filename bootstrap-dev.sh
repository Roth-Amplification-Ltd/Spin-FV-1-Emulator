#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
#
# Spin FV-1 Emulator development environment bootstrap.
#
# Project convention: every software repository should provide a root-level
# bootstrap-dev.sh that installs/verifies the build environment and can bring a
# fresh development machine to a known-good build/test state with one command.

set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BUILD_TYPE="RelWithDebInfo"
COMPILER="gcc"
DO_INSTALL=1
DO_BUILD=1
DO_TEST=1
CLEAN_BUILD=0
CHECK_ONLY=0

usage() {
    cat <<'USAGE'
Usage: ./bootstrap-dev.sh [options]

Set up and validate the Linux development environment for Spin-FV-1-Emulator.
Default behavior: install missing prerequisites, configure, build, and run tests.

Options:
  --check              Only report environment status; change nothing.
  --install-only       Install/verify prerequisites, but do not build.
  --no-test            Configure/build, but skip CTest.
  --clean              Remove ./build before configuring.
  --compiler gcc       Build with GCC/G++ (default).
  --compiler clang     Build with Clang/Clang++.
  --build-type TYPE    CMake build type (default: RelWithDebInfo).
  -h, --help           Show this help.

Supported bootstrap hosts for Phase 1:
  Pop!_OS, Ubuntu, Debian, and apt-compatible derivatives.

Examples:
  ./bootstrap-dev.sh
  ./bootstrap-dev.sh --clean
  ./bootstrap-dev.sh --compiler clang --clean
  ./bootstrap-dev.sh --check
USAGE
}

while (($#)); do
    case "$1" in
        --check)
            CHECK_ONLY=1
            DO_INSTALL=0
            DO_BUILD=0
            DO_TEST=0
            shift
            ;;
        --install-only)
            DO_BUILD=0
            DO_TEST=0
            shift
            ;;
        --no-test)
            DO_TEST=0
            shift
            ;;
        --clean)
            CLEAN_BUILD=1
            shift
            ;;
        --compiler)
            [[ $# -ge 2 ]] || { echo "error: --compiler requires gcc or clang" >&2; exit 2; }
            COMPILER="$2"
            shift 2
            ;;
        --build-type)
            [[ $# -ge 2 ]] || { echo "error: --build-type requires a value" >&2; exit 2; }
            BUILD_TYPE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

case "$COMPILER" in
    gcc)
        CC_BIN="gcc"
        CXX_BIN="g++"
        ;;
    clang)
        CC_BIN="clang"
        CXX_BIN="clang++"
        ;;
    *)
        echo "error: unsupported compiler '$COMPILER' (choose gcc or clang)" >&2
        exit 2
        ;;
esac

printf '\n=== Spin FV-1 Emulator: developer bootstrap ===\n'
printf 'Project:     %s\n' "$ROOT_DIR"
printf 'Build type:  %s\n' "$BUILD_TYPE"
printf 'Compiler:    %s\n' "$COMPILER"
printf 'Platform:    Linux-first Phase 1\n\n'

# Commands required for development. Package installation below supplies them.
required_commands=(cmake python3 git)
if [[ "$COMPILER" == "gcc" ]]; then
    required_commands+=(gcc g++)
else
    required_commands+=(clang clang++)
fi

missing_commands=()
for cmd in "${required_commands[@]}"; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        missing_commands+=("$cmd")
    fi
done

# Ninja is the preferred generator. We install it where possible, but the build
# can safely fall back to Unix Makefiles on a machine where Ninja is unavailable.
if ! command -v ninja >/dev/null 2>&1; then
    missing_commands+=(ninja)
fi

if (( CHECK_ONLY )); then
    echo "=== Environment check ==="
    if ((${#missing_commands[@]})); then
        printf 'Missing: %s\n' "${missing_commands[*]}"
        echo "Status: NOT READY"
        exit 1
    fi

    "$CC_BIN" --version | head -n 1
    "$CXX_BIN" --version | head -n 1
    cmake --version | head -n 1
    ninja --version | sed 's/^/ninja /'
    python3 --version
    git --version
    echo "Status: READY"
    exit 0
fi

if (( DO_INSTALL )); then
    if ((${#missing_commands[@]})); then
        if ! command -v apt-get >/dev/null 2>&1; then
            echo "error: missing development tools: ${missing_commands[*]}" >&2
            echo "Phase 1 automatic installation currently supports apt-based Linux distributions." >&2
            echo "Install a C/C++ compiler, CMake, Ninja, Python 3, Git, pkg-config, GDB, and Valgrind, then rerun." >&2
            exit 1
        fi

        if [[ $EUID -eq 0 ]]; then
            SUDO=()
        elif command -v sudo >/dev/null 2>&1; then
            SUDO=(sudo)
        else
            echo "error: package installation requires root privileges or sudo" >&2
            exit 1
        fi

        packages=(
            build-essential
            cmake
            ninja-build
            pkg-config
            git
            python3
            python3-pip
            gdb
            valgrind
        )
        if [[ "$COMPILER" == "clang" ]]; then
            packages+=(clang)
        fi

        echo "=== Installing missing development environment ==="
        "${SUDO[@]}" apt-get update
        "${SUDO[@]}" apt-get install -y "${packages[@]}"
    else
        echo "=== Development prerequisites already present ==="
    fi
fi

# Re-check after package installation and fail early with a useful diagnostic.
post_missing=()
for cmd in "${required_commands[@]}"; do
    command -v "$cmd" >/dev/null 2>&1 || post_missing+=("$cmd")
done
if ((${#post_missing[@]})); then
    echo "error: environment is still missing: ${post_missing[*]}" >&2
    exit 1
fi

printf '\n=== Toolchain ===\n'
"$CC_BIN" --version | head -n 1
"$CXX_BIN" --version | head -n 1
cmake --version | head -n 1
python3 --version
git --version
if command -v ninja >/dev/null 2>&1; then
    ninja --version | sed 's/^/ninja /'
fi

if (( ! DO_BUILD )); then
    echo
    echo "Development environment is ready. Build skipped (--install-only)."
    exit 0
fi

if (( CLEAN_BUILD )); then
    echo
    echo "=== Cleaning previous build ==="
    rm -rf -- "$BUILD_DIR"
fi

if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
else
    GENERATOR="Unix Makefiles"
fi

printf '\n=== Configure (%s) ===\n' "$GENERATOR"
CC="$CC_BIN" CXX="$CXX_BIN" cmake \
    -S "$ROOT_DIR" \
    -B "$BUILD_DIR" \
    -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

printf '\n=== Build ===\n'
cmake --build "$BUILD_DIR" --parallel

if (( DO_TEST )); then
    printf '\n=== Test ===\n'
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

printf '\n=== READY ===\n'
printf 'CLI: %s/fv1-cli\n' "$BUILD_DIR"
printf 'Try: %s/fv1-cli inspect %s/examples/steal-this-dsp-programs/03_pitch_maw.spn\n' "$BUILD_DIR" "$ROOT_DIR"
