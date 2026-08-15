# Phase 7A — Headless Test Report

Date: 2026-08-15
Baseline: Phase 6C `1.0.0-rc1` / commit candidate `e69dbbf3751872c09ff00cd9431b41db8f82de55`

## Result

**PASS — 34/34 CTest tests passed on the Linux development executor.**

The Phase 7A frontend-session wrapper compiled warning-clean in the normal GCC build and passed its
public-SDK smoke test. The frontend boundary scanner also passed and confirmed that the six native
Windows source/header files use only the allowed public FV-1 SDK headers.

A separate Clang SDK-only build passed **9/9 SDK tests**.

## Phase 6C portability correction carried forward

The first post-Phase-6C Windows/MSVC portability run exposed a test-harness compiler-mode problem:
C test files use C11 `_Static_assert`, while the project previously declared only the C++ language
standard. Phase 7A's baseline therefore makes the existing source requirement explicit:

- `CMAKE_C_STANDARD 11`
- `CMAKE_C_STANDARD_REQUIRED ON`
- `CMAKE_C_EXTENSIONS OFF`

No SDK structure, function, symbol manifest, or public header was changed by this correction.

## ABI preservation check

The following Phase-6C reviewed files are byte-for-byte unchanged from the supplied RC1 full tree:

- `include/fv1/sdk.h`
- `include/fv1/sdk_debug.h`
- `cmake/fv1-sdk-symbols.txt`
- `tests/sdk/test_sdk_abi_layout.c`
- `tests/sdk/abi-baseline/0.9.0/fv1/sdk.h`

The full regression run also passed:

- ABI layout fixture;
- exact shared-library export surface;
- Phase-6B/0.9.0 header compatibility consumer;
- C/C++ and cross-language SDK consumers;
- SDK abuse contract tests;
- malformed input tests;
- differential stress;
- staged product install.

## New Phase 7A tests

### `fv1-native-frontend-session-tests`

Proves the Windows frontend's platform-neutral session layer can, exclusively through the public SDK:

- create an engine;
- compile/load a four-instruction SpinASM passthrough;
- set POT0–POT2;
- process 512 deterministic probe samples;
- obtain a public snapshot;
- obtain public resource analysis;
- reset the virtual chip.

### `phase7a-windows-frontend-boundary`

Scans `src/windows/` and rejects private emulator/runtime/reference/Qt/miniaudio dependencies.

## Windows build status

The current executor is Linux and does not contain the Microsoft Windows SDK, so the actual Win32
GUI and WASAPI probe translation units cannot be compiled locally here. The phase therefore adds
`.github/workflows/windows-frontend.yml`, which builds `fv1-lab-win32` and the session smoke test on
`windows-2025` / native MSVC against **both shared and static** `FV1::sdk` configurations.

Phase 7A should not be called Windows-accepted until that remote workflow is green after push.
Likewise ABI v1 remains a freeze candidate until the corrected Phase-6C SDK Portability matrix is green.

## Local acceptance summary

- GCC full headless build: PASS
- Full CTest suite: 34/34 PASS
- Clang SDK-only build: PASS
- Clang SDK-only CTest: 9/9 PASS
- Public SDK boundary scan: PASS
- Reviewed ABI/public files unchanged: PASS
- Native MSVC Win32 compile/run: PENDING REMOTE CI
- Realtime full-duplex WASAPI: intentionally deferred to Phase 7B
