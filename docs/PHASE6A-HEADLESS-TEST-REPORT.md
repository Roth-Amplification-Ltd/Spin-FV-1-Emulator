# Phase 6A Headless Test Report

## Scope

This report covers the SDK-extraction changes in Phase 6A. It does not claim physical FV-1 silicon
validation and does not declare ABI v1 frozen.

Primary acceptance questions:

1. Does the existing emulator/conformance behavior still pass after SDK extraction?
2. Is SpinASM compilation native at runtime and byte-identical to the historical Python oracle for
   the bundled regression bank?
3. Can a pure C external project consume the **installed** shared SDK without private headers?
4. Does the static package remain usable?
5. Is the shared-library exported symbol surface restricted to the documented C ABI?
6. Do the new SDK/compiler paths survive sanitizer and fuzz smoke testing?

## Environment used for package construction

Headless container build with Linux/GNU toolchain for the normal acceptance suite and Clang for
sanitizer/fuzzer coverage. SpeexDSP/miniaudio were unavailable in the container, so those optional
Linux runtime pieces used their existing deterministic/stub paths. The Qt GUI was intentionally not
part of this headless gate; a Qt workstation adds the existing `fv1-lab-smoke` test.

## GCC / normal shared-SDK build

Configuration characteristics:

- `FV1_BUILD_GUI=OFF`
- `FV1_SDK_BUILD_SHARED=ON`
- strict project warnings enabled
- SDK version `0.8.0`

Result: **23/23 CTest tests passed**.

New Phase-6A tests include:

- `fv1-spinasm-tests`
- `fv1-sdk-c-api-tests`
- `sdk-export-surface`
- `native-spinasm-python-equivalence`
- `installed-sdk-consumer`

No project compiler warnings were emitted in the normal build log.

## Native SpinASM equivalence

All eight bundled Steal This DSP programs were compiled by:

- native `fv1-spinasm` through `fv1-cli assemble`; and
- historical `tools/fv1_assembler.py`.

The regression requires all resulting 512-byte program images to be byte-identical. Result: **PASS**.
The Python assembler is therefore retained as a development oracle, not an installed runtime bridge.

## Installed shared-SDK consumer

CTest installs the build into an empty staging prefix and configures `examples/sdk-host/` with only:

```cmake
find_package(FV1SDK CONFIG REQUIRED)
target_link_libraries(fv1-sdk-host PRIVATE FV1SDK::sdk)
```

The host source is C and includes only `<fv1/sdk.h>`. In shared mode the external project enables only
the C language. It compiles SpinASM, creates a virtual FV-1, loads the 512-byte image, processes
stereo samples, reads a snapshot and exits successfully. Result: **PASS**.

## Shared-library export surface

On ELF/Linux, `nm -D --defined-only` showed only the SDK version marker and `fv1_sdk_*` functions.
No C++ implementation symbols were exported. Result: **PASS**.

The install package exposes `FV1SDK::sdk` as the public target; the static implementation dependencies
appear only as underscore-prefixed `_core_internal` / `_spinasm_internal` targets and are documented
as private.

## Static SDK package

A separate `FV1_SDK_BUILD_SHARED=OFF` build passed the SDK/compiler checks plus representative
runtime clients:

- `fv1-sdk-c-api-tests`
- `native-spinasm-python-equivalence`
- `installed-sdk-consumer`
- `fv1-phase2-tests`
- `phase2-render-src`
- `phase5-validation-cli`

The external host source remains C. Static linking deliberately selects a C++ linker because the
hidden SDK implementation is C++20. Result: **PASS**.

## Clang ASan/UBSan

The complete 23-test headless suite was rebuilt/run with Clang, AddressSanitizer and
UndefinedBehaviorSanitizer. The cross-language shared-library sanitizer configuration disables only
`vptr` instrumentation so the instrumented dylib/so can still be linked by the C-only sanitizer
consumer; ASan and the remaining UBSan checks stay active.

Result: **23/23 PASS**, with no AddressSanitizer or UBSan diagnostics.

The installed external SDK test also passes in the instrumented build by propagating the build's
sanitizer flags to the staged smoke host.

## Fuzz smoke campaigns

Two Clang/libFuzzer + ASan/UBSan targets were exercised:

- `fv1-conformance-fuzzer`: **1000 runs**, production/reference differential observer;
- `fv1-spinasm-fuzzer`: **1000 runs**, arbitrary native SpinASM parser/compiler input.

Both campaigns completed without sanitizer findings or conformance traps. These are smoke campaigns,
not substitutes for the longer Phase-6C release soak.

## Linux runtime SDK dogfooding

The Phase-2 runtime regression, host-rate SRC regression, validation CLI and rendered-demo tests all
pass after replacing `fv1-runtime`'s raw core-engine ownership with an opaque `fv1_sdk_engine`. Thus
the existing Linux audio/testbench path is exercising the candidate SDK processing boundary.

## Runtime dependency audit

The CLI, live host, Qt program-loader path and Paste SpinASM dialog call native
`fv1::spinasm::compile()` directly. Runtime source contains no Python subprocess/temp-file compiler
bridge. Python references in the build are confined to regression/oracle tests.

## Phase 6A disposition

The SDK extraction is ready for Linux workstation/Qt acceptance. Expected Qt-enabled result:
**24/24 tests**, where the additional test is `fv1-lab-smoke`.

Phase 6A still labels the public header as an **ABI candidate**. Phase 6B must review and stabilize the
surface before the project promises SDK ABI v1 compatibility.
