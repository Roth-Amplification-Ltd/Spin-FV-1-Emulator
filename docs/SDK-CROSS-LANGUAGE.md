# FV-1 SDK Cross-Language Consumption

## Rule

The supported binary contract is C. C++ convenience wrappers and language bindings sit *above* that
contract; no language binding is allowed to require a private FV-1 Emulator header or C++ class.

## Installed development surface

A normal SDK install intentionally exposes only:

```text
include/fv1/sdk.h
include/fv1/sdk_debug.h
include/fv1/sdk.hpp
include/fv1/module.modulemap
lib/.../FV1SDKConfig.cmake
lib/.../FV1SDKTargets.cmake
libfv1-sdk / fv1-sdk.dll / libfv1-sdk.dylib
```

Internal headers such as `fv1.h`, `runtime.hpp`, `validation.hpp`, `spinasm.hpp`, Qt headers, and
reference/conformance classes are not installed as SDK headers.

## Language proving hosts

`examples/sdk-hosts/` contains deliberately small consumers:

- `cpp/` — installed CMake package + header-only RAII wrapper;
- `python/` — direct `ctypes` load of the shared C ABI;
- `swift/` — imports the installed Clang module `FV1SDK` and processes audio;
- `rust/` — hand-written `extern "C"` FFI with no C++ shim;
- `objective-c/` — Objective-C translation-unit import probe against the same C header.

The older `examples/sdk-host/` remains the minimal pure-C CMake package consumer.

## Why a Clang module map is installed

`module.modulemap` makes the C headers importable as a named Clang module. That is the natural bridge
for Swift/Objective-C clients and avoids requiring a frontend to include C++ implementation headers.
The module is only a declaration/import aid; ownership and runtime semantics remain those documented
by the C ABI.

## C++ convenience API

`<fv1/sdk.hpp>` is header-only and provides RAII ownership plus a small `ProgramImage` and native
SpinASM helper. The C++ wrapper is a source-level convenience layer; the binary compatibility promise
remains the C ABI in `sdk.h` / `sdk_debug.h`.

## Python and Rust

The shared library can be consumed directly from Python `ctypes` because all public ownership is
opaque-handle/caller-buffer based. Rust likewise requires only ordinary `extern "C"` declarations;
there is no C++ bridge object, exception boundary, or cross-runtime allocator ownership. Public result
and control scalar types are fixed-width, and Windows explicitly uses the C `cdecl` calling convention.

The proving hosts do more than load the library: they compile SpinASM, create/load an engine, exercise
an intentional invalid-control error, process audio, and inspect post-process state/capabilities. This
keeps the tests focused on actual host ergonomics rather than header syntax alone.

A production Rust crate or Python package may be added later, but such packaging is not required to
freeze the underlying C ABI.
