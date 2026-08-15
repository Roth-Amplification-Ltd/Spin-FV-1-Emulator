# Phase 6C Headless Test Report

Status: **local RC hardening pass complete; Rosie and post-commit remote ratification still pending**.

## Local build environment

The package-development container used GCC 14.2, Clang 17, CMake 3.31.6, Ninja 1.12.1, Python 3.13.5
and Swift 6.2.1. Qt/SpeexDSP/miniaudio device acceptance is intentionally left to Rosie/the existing
Linux product CI; the local release-hardening builds used GUI/audio-device-independent modes.

## Results

| Gate | Result |
|---|---|
| GCC RelWithDebInfo headless | **32/32 PASS** |
| Clang RelWithDebInfo headless | **32/32 PASS** |
| SDK-only shared | **9/9 PASS** |
| SDK-only static | **7/7 PASS** |
| Clang ASan + UBSan, static SDK | **30/30 PASS** |
| `fv1-conformance-fuzzer` | **5,000 runs PASS** (seeded legal-program corpus) |
| `fv1-spinasm-fuzzer` | **5,000 runs PASS** |
| `fv1-sdk-fuzzer` | **5,000 runs PASS** |
| Phase-6B 0.9.0-header/current-library compatibility | **PASS** |
| malformed CLI inputs | **PASS** |
| differential release stress | **72 deterministic runs PASS** |
| staged product/install smoke | **PASS** |
| exact downloadable overlay rehearsal from clean Phase 6B | **32/32 PASS** |

The differential release stress is eight programs × (eight distinct seeds + one repeated determinism
run) = 72 program executions. It is additive to the existing instruction-level randomized and demo-bank
conformance suites.

## Sanitizer note

The sanitizer gate uses a static SDK while instrumenting the complete implementation. This avoids a
C-linker/runtime mismatch caused by loading a C++ UBSan-instrumented shared object into C-only test
executables. Shared-library ABI behavior remains independently covered by normal SDK shared builds,
installed consumers, exact export tests and cross-platform portability CI.

## Remote Phase-6B observation that triggered a 6C correction

The Phase-6B Linux CI passed. The first SDK Portability matrix passed five of six combinations. The
only failure was Windows shared: the DLL built and C/C++/ABI tests passed, but Python `ctypes` could
not load the MinGW-built DLL because of runtime dependency search behavior. Phase 6C switches the
Windows SDK workflow to native Visual Studio/MSVC and propagates multi-config install/build settings.

## Remaining acceptance

This report does **not** freeze ABI v1. The exact overlay must still pass Rosie's Qt-enabled suite and
normal desktop use. Because the splash-photo integration and branded About presentation add two Qt-only render probes, the revised RC is
expected to report **36/36** on Rosie. The committed RC revision must then pass Linux CI, Release
Hardening CI and all six SDK Portability jobs. Only after those results are green should ABI-v1 status be ratified as frozen.

## Splash asset packaging gate

The Phase-6C product-install test now requires the exact splash source asset
`share/spin-fv1-emulator/splash/FV1LabSplashImagebase.png` in the staged product. Qt-enabled builds also
add `fv1-lab-splash-background-smoke`, which constructs and paints the startup splash offscreen and fails if the
default background cannot be located/decoded. `fv1-lab-about-smoke` separately renders the on-demand
About presentation, while the normal GUI smoke test verifies that **Help → About FV-1 Lab…** exists.
Headless test counts remain unchanged because those probes exist only when the Qt application target is available.
