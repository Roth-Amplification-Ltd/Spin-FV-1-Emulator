# Phase 6A — FV-1 SDK Extraction

## Goal

Make the virtual FV-1 usable as an installed module by applications that know nothing about the
FV-1 Emulator GUI or private C++ implementation.

Phase 6A deliberately precedes ABI freeze. Its job is to expose, exercise and criticize the
candidate boundary while changes are still cheap.

## Implemented

### Versioned C ABI candidate

`include/fv1/sdk.h` exposes opaque engine handles, setup/configuration, 512-byte program loading,
POT controls, planar/interleaved float processing, state/resource inspection and native SpinASM
compilation. No C++ standard-library or GUI/audio-device type appears in the header.

### Linux runtime dogfoods the SDK

`fv1-runtime` no longer owns a raw `fv1_engine*` or exposes it through its public C++ interface. Its
virtual chip is an opaque `fv1_sdk_engine*`; prepare/reset/load/POT/process operations go through the
Phase-6A SDK candidate. The host-rate SRC and audio queues remain outside the SDK, exactly as a
Windows/CoreAudio/WASAPI host adapter would.

This is intentional architectural pressure: the Linux application now exercises the same processing
boundary intended for future native frontends instead of leaving the SDK as an unused sidecar.

### Native SpinASM compiler

`fv1-spinasm` ports the repository's assembler semantics to C++20. The CLI, live host and Qt app
compile SpinASM in-process. The Python compiler remains a regression oracle and the test suite
requires byte-identical output for all eight bundled demo programs.

### Installed CMake package

A normal install produces:

```text
include/fv1/sdk.h
lib/libfv1-sdk.*
lib/cmake/FV1SDK/FV1SDKConfig.cmake
lib/cmake/FV1SDK/FV1SDKTargets.cmake
...
```

An external project consumes it with:

```cmake
find_package(FV1SDK CONFIG REQUIRED)
target_link_libraries(my_host PRIVATE FV1SDK::sdk)
```

### Public-symbol control

The shared SDK hides implementation symbols. ELF uses a linker version script, Mach-O has an
explicit exported-symbol list, and Windows uses the SDK import/export macro. The Linux regression
suite checks the dynamic export surface.

### External host acceptance

`examples/sdk-host/main.c` is a pure C consumer of the installed package. In the default shared SDK
configuration its CMake project enables only the C language. Static SDK mode is separately exercised
and uses a C++ linker only because the hidden implementation runtime is C++.

### SDK parser fuzz surface

The newly native SpinASM parser is fuzzable with Clang/libFuzzer + ASan/UBSan in addition to the
Phase-5C production/reference conformance fuzzer.

## Candidate usage

```c
#include <fv1/sdk.h>

fv1_sdk_engine_config_v1 cfg;
fv1_sdk_engine_config_v1_init(&cfg);

fv1_sdk_engine *engine = NULL;
if (fv1_sdk_engine_create_v1(&cfg, &engine) != FV1_SDK_OK) {
    /* handle error */
}

/* compile or provide exactly 512 bytes of FV-1 program image */
fv1_sdk_engine_load_program(engine, program, FV1_SDK_PROGRAM_BYTES);
fv1_sdk_engine_set_pots(engine, 0.25f, 0.5f, 0.75f);
fv1_sdk_engine_process_interleaved_f32(engine, input, output, frame_count);
fv1_sdk_engine_destroy(engine);
```

See `examples/sdk-host/main.c` for a complete compile/load/process example.

## Design decisions

- The GUI is a client, never the definition of the SDK.
- Audio-device ownership is not part of the core ABI.
- The production implementation remains C++20.
- Cross-language consumers see a C ABI.
- Caller-owned buffers avoid allocator/CRT ownership crossing the boundary.
- Shared SDK is the preferred cross-language distribution form; static remains supported.
- `fv1-reference`/`fv1-conformance` stay verification internals for now rather than inflating ABI v1.
- Phase 6B, not Phase 6A, is the freeze gate.

## Exit criteria

Phase 6A is ready for acceptance when:

- normal headless CTest passes;
- native/Python SpinASM equivalence passes for all eight demos;
- C ABI unit test passes;
- shared-library export surface contains only the SDK contract;
- clean-prefix installed external C consumer builds and runs;
- static SDK installed-consumer path builds and runs;
- GCC normal build is warning-clean for project code;
- Clang ASan/UBSan build passes the SDK/compiler tests;
- short conformance and SpinASM fuzz smoke campaigns complete without sanitizer findings.

The next milestone is **Phase 6B — ABI/API review and stabilization**. No ABI is declared frozen by
this document.
