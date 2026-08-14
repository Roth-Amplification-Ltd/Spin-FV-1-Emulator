# Phase 1 Linux Test Report

Date: 2026-08-13

## Host used for this validation

```
Linux 6.18.35 x86_64 GNU/Linux
gcc (Debian 14.2.0-19) 14.2.0
clang version 17.0.0 (https://github.com/swiftlang/llvm-project.git 10999b6d034fe318f3d56c83bddb6572593a8bb0)
cmake version 3.31.6
Ninja 1.12.1
Python 3.13.5
```

## GCC release-style build

Configuration:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Result:

```
4/4 tests passed

fv1-core-tests ................. PASS
compile-steal-this-bank ........ PASS
cli-inspect-gravity-clerk .......... PASS
render-steal-this-bank ......... PASS
```

The render regression compiles and executes all eight Steal This DSP example programs against a deterministic two-second stereo stimulus. Every rendered output is checked for finite samples, normalized bounds and non-silent RMS/peak output.

## Clang + AddressSanitizer + UndefinedBehaviorSanitizer

Configuration:

```bash
CC=clang CXX=clang++ cmake -S . -B build-clang-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-clang-asan -j2
ctest --test-dir build-clang-asan --output-on-failure
```

Result:

```
4/4 tests passed under ASan + UBSan
```

No sanitizer failure was reported.

## Install staging

```bash
cmake --install build --prefix stage
```

Installed artifacts:

- `lib/libfv1-core.a`
- `bin/fv1-cli`
- `include/fv1/fv1.h`
- `include/fv1/fv1.hpp`
- `libexec/spin-fv1-emulator/fv1_assembler.py`

The staged `fv1-cli` was then launched outside the source directory and successfully assembled `examples/simple_passthrough.spn`, verifying that the installed CLI can locate its installed assembler helper on Linux.

## Current interpretation of "passes"

These tests establish software correctness against the current Phase-1 model and regression corpus. They do **not** yet establish bit-exact equivalence to physical FV-1 silicon. Hardware fidelity is an explicit later validation phase, particularly for delay-RAM encoding, CHO edge cases, converter behavior and POT hysteresis.
