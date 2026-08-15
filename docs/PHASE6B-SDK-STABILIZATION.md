# Phase 6B — Cross-Language SDK Stabilization

## Goal

Prove that the Phase-6A SDK extraction is usable as a real module outside FV-1 Lab, then fill only the
capability gaps that are clearly required by native Windows/macOS applications, a future IDE, or
third-party hosts.

**Phase 6B is still pre-freeze.** Breaking candidate changes are allowed here. The freeze gate remains
after Phase 6C release hardening.

## Implemented

- project SDK candidate advanced to 0.9.0 while ABI major remains candidate `1`;
- `FV1_SDK_ONLY=ON` configure mode avoids Qt, miniaudio, SpeexDSP, runtime, analysis, validation, and
  product-target discovery;
- `fv1-sdk` is now self-contained for both shared and static packaging rather than exporting private
  core/compiler libraries as link dependencies;
- installed SDK development headers are narrowed to the explicit public surface;
- capability and structured version discovery;
- program readback;
- single-POT and single-sample convenience calls;
- optional `sdk_debug.h` instruction-step/state-digest module;
- installed Clang module map for Swift/Objective-C import;
- header-only C++ RAII convenience wrapper;
- independent C, C++, Python, Swift, Rust, and Objective-C proving-host sources;
- SDK portability CI matrix for Linux/macOS/Windows SDK-only builds;
- consumer-requirements audit defining what is and is not part of ABI v1.

## Compatibility rules reinforced

- C ABI is the binary contract;
- one engine is mutable and not concurrently callable from multiple threads;
- bulk data stays caller-owned;
- no C++ exceptions cross the ABI;
- process functions are the only realtime-safe public operations currently promised;
- host audio/UI/SRC remains outside the virtual-chip contract;
- private engine-state serialization is not frozen;
- future source maps, richer compiler diagnostics, validation, and analysis may be additive modules.

## Acceptance

Phase 6B software acceptance requires:

1. complete normal regression suite;
2. SDK-only shared build/test with no application dependencies;
3. SDK-only static installed-consumer test;
4. narrow shared-library export surface;
5. installed C and C++ external hosts;
6. Python direct-FFI host;
7. Swift Clang-module host when Swift is present;
8. Rust FFI host when Rust is present;
9. Objective-C header/import probe when Clang is present;
10. clean staging tree with no internal development headers leaked into the SDK package.

Rosie remains the Qt/Linux application acceptance machine; native Windows/macOS GUI implementation is
still deferred until the API candidate survives Phase 6C hardening.

## ABI details hardened during the review

The candidate now avoids implementation-defined enum width across the binary boundary, explicitly
uses `cdecl` on Windows, regression-tests 64-bit structure layout, publishes compiler diagnostic buffer
requirements, and exposes the FV-1 architectural register map to debugger clients. These were treated
as pre-freeze corrections rather than deferred compatibility work.

See also [`SDK-ABI-CANDIDATE.md`](SDK-ABI-CANDIDATE.md).
