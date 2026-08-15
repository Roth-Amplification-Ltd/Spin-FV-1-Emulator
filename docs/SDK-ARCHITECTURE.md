# FV-1 SDK Architecture

## Purpose

The FV-1 Emulator desktop application is a **client of the FV-1 SDK**. It is not the SDK.

Phase 6A extracted the smallest useful platform-neutral boundary so the same virtual chip can be
embedded in other applications and can sit underneath independently written Linux, Windows and
macOS frontends. The SDK boundary is intentionally C-shaped even though the implementation remains
modern C++20.

Phase 6B is actively reviewing this **candidate**, not yet the frozen v1 ABI. Cross-language
consumption and future-host completeness are the primary acceptance criteria.

## Dependency boundary

```text
 Qt/Linux app        native Windows app        native macOS app        other host
     |                       |                       |                    |
     | host audio/UI         | host audio/UI         | host audio/UI      |
     +-----------------------+-----------------------+--------------------+
                                     |
                              public C ABI
                              <fv1/sdk.h>
                                     |
                                FV1SDK::sdk
                                     |
                   +-----------------+-----------------+
                   |                                   |
             fv1-core internals                 native SpinASM
             production engine                  compiler internals
                   |                                   |
                   +-----------------+-----------------+
                                     |
                          private C++ implementation
```

No Qt, miniaudio, SpeexDSP, filesystem path type, C++ standard-library type, exception, or
platform-window/audio type crosses the SDK ABI.

## Phase-6B public candidate

The supported embedding surface is:

- installed core header: `fv1/sdk.h`;
- optional debugger header: `fv1/sdk_debug.h`;
- header-only C++ convenience wrapper: `fv1/sdk.hpp`;
- installed Clang module map for Swift/Objective-C;
- CMake target: `FV1SDK::sdk`;
- opaque `fv1_sdk_engine` lifetime;
- versioned configuration/snapshot/resource/compiler-report structures;
- 512-byte FV-1 program loading;
- POT updates;
- planar and stereo-interleaved `float` processing;
- snapshot, delay-word inspection and program resource analysis;
- native in-process SpinASM compilation into a caller-owned 512-byte image;
- ABI/implementation version and capability introspection;
- program readback, single-POT and single-sample helpers;
- optional instruction stepping/trace/state-digest primitives.

The exported shared-library symbol surface is restricted to `fv1_sdk_*`. Internal C++ symbols are
hidden on ELF and are constrained by an explicit export list on Mach-O; Windows uses explicit
`__declspec(dllexport/dllimport)` declarations.

### What is deliberately *not* frozen

The following remain implementation/development APIs during Phase 6B:

- `fv1-core` C++ and legacy low-level C implementation details;
- `fv1-spinasm` C++ classes;
- `fv1-runtime` host-rate/SRC adapters;
- `fv1-audio` and miniaudio device ownership;
- `fv1-analysis`, `fv1-debugger`, `fv1-validation`, `fv1-reference` and `fv1-conformance` C++ APIs;
- Qt classes and application/session objects.

The consumer-needs audit decides which additional capabilities deserve a stable C surface. Adding
everything merely because it exists internally would make the freeze larger and more fragile. See
`SDK-CONSUMER-REQUIREMENTS.md`.

## Native SpinASM boundary

The application no longer shells out to Python to compile `.spn` files. `fv1-spinasm` is a native
C++20 compiler used internally by the CLI, live host and Qt application. External applications call
that same compiler through `fv1_sdk_compile_spinasm_v1()`.

The historical Python assembler remains in `tools/` as a development/reference oracle. The test
suite compiles all eight bundled programs through both compilers and requires byte-for-byte identity.
Python is therefore a test dependency when tests are enabled, **not an installed runtime dependency**
of an SDK/application consumer.

## Audio ownership

The SDK models the virtual chip, not an operating-system audio stack. A host owns device I/O,
buffering and any host-rate adaptation it chooses to place around the chip boundary.

```text
CoreAudio / WASAPI / PipeWire / file / caller buffers
                         |
                  host-side adapter
                         |
              fv1_sdk_engine_process_*
                         |
                  virtual FV-1
                         |
                  host-side adapter
                         |
                 host audio output
```

The existing Linux application continues using `fv1-runtime`, SpeexDSP and miniaudio for its host
adapter, but Phase 6A routes `fv1-runtime`'s virtual-chip processing through `FV1SDK::sdk`. That means
FV-1 Lab dogfoods the same engine create/load/POT/process boundary used by an external host. The
runtime still owns Linux-specific clock-domain adaptation and audio plumbing outside that boundary.

Those host libraries remain useful implementation details, not requirements imposed on an SDK host.

## Realtime contract

After engine creation and program loading, the `fv1_sdk_engine_process_*()` calls promise not to:

- allocate/free heap memory;
- acquire locks;
- touch the filesystem;
- log;
- invoke GUI facilities.

Compilation, reporting, validation and other setup/inspection operations are not assumed realtime-safe
unless a future API explicitly says otherwise.

## Ownership and error model

- `fv1_sdk_engine_create_v1()` creates an opaque handle.
- `fv1_sdk_engine_destroy()` destroys it; the creating SDK owns the implementation allocation.
- caller-provided audio/program/report/diagnostic buffers remain caller-owned;
- no `new`/`delete`, `std::string`, exception object or allocator ownership crosses the ABI;
- public calls return `fv1_sdk_result`; C++ exceptions are caught internally before returning through
  the C boundary.

This avoids CRT/allocator mismatches on Windows and avoids compiler-specific C++ ABI coupling on all
platforms.

## Versioned structures

Public v1 structures begin with:

```c
uint32_t struct_size;
uint32_t abi_version;
```

Callers should initialize structures with the matching `*_init()` helper rather than manually
zeroing or guessing defaults. Reserved fields are kept zero. An incompatible major ABI is rejected
rather than silently reinterpreted.

## SDK-only build boundary

`FV1_SDK_ONLY=ON` configures a portable SDK build without discovering Qt, miniaudio, SpeexDSP, the
Linux runtime, analysis, validation, or application targets. `fv1-sdk` contains its private machine and
compiler implementation directly, so neither shared nor static consumers receive private core/compiler
link targets. The SDK install exposes only the explicit public headers/package metadata.

## Shared and static forms

`FV1_SDK_BUILD_SHARED=ON` is the default and produces the cleanest cross-language boundary. The
installed example is a **C-only** CMake project in this configuration.

A static SDK is also supported. Its public source/header interface remains C, but the final host
linker must include a C++ runtime because the implementation is compiled as C++. The installed SDK
smoke test exercises both forms.

## External-consumer acceptance test

`examples/sdk-host/` is intentionally outside the internal library graph. During CTest,
`installed-sdk-consumer`:

1. installs the current build into a clean staging prefix;
2. configures the example with `find_package(FV1SDK CONFIG REQUIRED)`;
3. includes only `<fv1/sdk.h>`;
4. compiles SpinASM through the SDK;
5. creates/loads/processes/inspects a virtual FV-1;
6. exits successfully.

That test is one proof that the project has an installable module rather than merely another in-tree
library target. `sdk-cross-language-consumers` extends the same staged package test to C++, Python,
Swift, Rust and Objective-C toolchains when available.
