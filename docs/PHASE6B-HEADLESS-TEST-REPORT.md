# Phase 6B Headless Test Report

Status: **implementation test pass; Rosie GUI acceptance pending corrective window-control retest**.

## Local build environment used for package verification

- GCC 14.2.0 normal build;
- Clang/Swift toolchain available for Objective-C/Swift consumer probes;
- Python 3.13 direct FFI test;
- Rust compiler not installed in this build container, so Rust execution is delegated to the Linux
  SDK portability CI job; the Rust proving-host source is included in the repository.

## Normal headless suite

The Phase-6B normal non-Qt configuration passes **27/27** tests. New SDK-specific coverage includes:

- expanded C ABI behavior/version/capability/program-readback test;
- debug stepping/state digest test;
- C++ wrapper test;
- fixed-width/calling-convention and 64-bit ABI-layout regression fixture;
- shared export-surface test;
- installed pure-C CMake consumer;
- cross-language installed-package test.

## SDK-only shared build

`FV1_SDK_ONLY=ON` builds only `fv1-sdk` plus requested SDK tests. The shared SDK-only suite passes
**7/7** tests and does not configure Qt, miniaudio, SpeexDSP, application runtime, analysis, or
validation libraries.

The cross-language test in this environment successfully executed:

- C installed consumer;
- C++ installed consumer;
- Python `ctypes` consumer;
- Swift Clang-module consumer;
- Objective-C header/import syntax probe.

Rust was explicitly reported `SKIP` because `rustc` is absent rather than being counted as a pass.
The portability CI executes the Rust proving host when `rustc` is available.

## SDK-only static build

The self-contained static SDK configuration passes **5/5** tests, including the clean installed C
consumer. Private `fv1-core`/`fv1-spinasm` archives no longer have to be exported to make the static
SDK linkable.

## Sanitizer and fuzz gate

A Clang SDK-only shared build with AddressSanitizer + UndefinedBehaviorSanitizer passes **6/6** applicable
SDK tests. The foreign-runtime aggregate test is deliberately omitted only in this configuration because
loading an ASan-instrumented shared object into an unsanitized Python/Swift runtime is a sanitizer-runtime
ordering problem rather than an SDK ABI test; the installed C consumer remains sanitizer-linked and passes.

The existing differential engine and native SpinASM fuzz targets each completed a 1,000-run smoke campaign
without a sanitizer finding. Longer soak campaigns remain Phase 6C work.

## ABI-candidate hardening added during review

- public result/control scalar typedefs are fixed-width rather than C enum storage;
- Windows calling convention is explicitly `cdecl`;
- reset flags use fixed-width values;
- compiler reports expose required/written diagnostic byte counts while preserving the 64-byte record size;
- stable FV-1 register indices and debugger register-name metadata are public;
- a 64-bit `sizeof`/`offsetof` fixture protects the candidate layout and is compiled with `-fshort-enums` on GCC/Clang;
- C++/Python/Swift/Rust proving hosts exercise an intentional error path and inspect post-process state.

## Rosie GUI corrective regression

The first Rosie Phase-6B application run exposed a desktop-window regression: the main FV-1 Lab
window did not advertise a maximize button even though the application remained otherwise resizable
and operational. An initial corrective attempt changed the window hints after `QMainWindow`
construction. Rosie then showed that this was the wrong layer: maximize was still absent and minimize
no longer integrated correctly with the desktop dock/task list.

The corrected implementation now supplies the complete ordinary top-level window/decorations contract
(`Qt::Window`, title, system menu, minimize, maximize, and close) directly to the `QMainWindow` base
constructor. It does not call `setWindowFlag()` on the main window after construction. This preserves a
single native top-level/window-manager identity from creation onward. `fv1-lab --smoke` now verifies
that the main object is an unparented `Qt::Window` and that minimize, maximize, and close hints are all
present. The approved dashboard layout and SDK surface are unchanged.

The smoke test validates Qt's client-side contract; actual server-side title-bar rendering and dock/task
list behavior remain desktop-window-manager integration tests and are accepted on Rosie.

## Remaining gates

Before ABI v1 freeze:

- execute the portability workflow on actual GitHub Linux/macOS/Windows runners;
- execute the Rust proving host in CI;
- complete Phase 6C sanitizer/fuzzer/release/package hardening;
- perform the final Phase-6C review of the recorded ABI layout/symbol candidate before declaring it frozen.
