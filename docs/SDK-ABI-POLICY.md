# FV-1 SDK ABI Policy

## Status

**Phase 6A: ABI candidate — NOT FROZEN.**

The names and shapes in `fv1/sdk.h` are being exercised as the candidate cross-platform embedding
contract. Phase 6B exists specifically to review this surface before declaring SDK ABI v1 frozen.
No downstream port should treat Phase-6A source compatibility as a permanent guarantee yet.

## Freeze principle

Freeze the **observer boundary**, not the implementation.

A frozen SDK must allow the production engine, decoder, arithmetic implementation, reference model,
runtime adapters and GUI code to change without requiring an application that embeds `FV1SDK::sdk`
to be rebuilt for unrelated internal refactors.

## Rules for ABI v1

1. **C linkage only.** No exported C++ classes, templates, STL containers, exceptions or RTTI.
2. **Opaque state.** Stateful implementation objects cross the boundary only as opaque handles.
3. **Fixed-width fields.** Public structure fields use `<stdint.h>` types where binary width matters.
4. **Versioned structures.** Public extensible records carry `struct_size` and `abi_version`.
5. **Caller-owned bulk memory.** Audio, program images and diagnostics use caller buffers unless a
   future function documents a paired SDK allocator/free function.
6. **No exceptions across ABI.** Every exported function catches implementation exceptions and maps
   failures to `fv1_sdk_result`.
7. **No platform UI/audio types.** Qt, Win32 window classes, Objective-C/Swift objects, CoreAudio,
   WASAPI, PipeWire and miniaudio types stay outside the ABI.
8. **Narrow symbol export.** Shared libraries export only documented `fv1_sdk_*` symbols.
9. **Realtime functions are explicit.** Only APIs explicitly documented realtime-safe receive that
   guarantee.
10. **Reserved space stays zero.** Callers initialize structures with SDK helpers and do not assign
    meaning to reserved fields.

## Version interpretation

`FV1_SDK_ABI_VERSION` is encoded as major/minor. The ABI major is the compatibility boundary.

- compatible additions may increase the minor version without changing existing semantics;
- breaking field/function/ownership/semantic changes require a new ABI major or a new explicitly
  versioned entry point;
- the implementation/release version returned by `fv1_sdk_get_version_string()` is separate from
  the ABI version.

During Phase 6A the major is already encoded as `1` to exercise realistic version handling, but this
is **candidate ABI 1**, not the declaration that ABI 1 has been frozen.

## Function evolution

Prefer additive APIs such as:

```c
fv1_sdk_engine_create_v1(...);
fv1_sdk_engine_create_v2(...); /* only if a genuinely incompatible contract is needed */
```

over changing the meaning or calling convention of an existing exported symbol.

Do not reuse enum/result numeric values for new meanings. Do not change existing structure field
offsets after freeze.

## Realtime contract stability

For a function documented realtime-safe, removing that guarantee is an ABI/API compatibility break
even if the C signature is unchanged. The behavioral contract matters as much as symbol layout.

For Phase 6A, `fv1_sdk_engine_process_planar_f32()` and
`fv1_sdk_engine_process_interleaved_f32()` are the realtime processing boundary after setup.

## Phase-6B freeze checklist

Before declaring v1 frozen:

- review every public function for necessity and naming;
- review ownership/null/aliasing rules;
- review configuration/version-extension strategy;
- decide whether debugger stepping, host-rate runtime, analysis and validation need separate stable C
  modules now or should remain future additive SDK surfaces;
- compile a consumer with GCC and Clang against the same shared library where practical;
- exercise Windows DLL export/import and MSVC consumption;
- exercise macOS dylib symbol visibility and Swift/Objective-C C import;
- verify shared/static install packages;
- record `sizeof`/`offsetof` ABI fixtures for supported 64-bit targets;
- record exported symbols as a release artifact;
- document the exact v1 compatibility promise.

Only after that checklist passes should documentation say **FV-1 SDK ABI v1 FROZEN**.
