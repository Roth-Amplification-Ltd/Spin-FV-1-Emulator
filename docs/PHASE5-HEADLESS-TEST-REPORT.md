# Phase 5A Headless Test Report

Phase 5A was exercised from a clean source snapshot with the GUI disabled in the packaging
container. Qt GUI compilation remains a Cortana acceptance gate because Qt development headers are
not installed in this container.

## Toolchains

Primary regression:

- GCC 14.2.0
- CMake 3.31.6
- Ninja 1.12.1
- Python 3.13.5

Sanitizer regression:

- Clang 17
- AddressSanitizer
- UndefinedBehaviorSanitizer
- leak detection enabled

The container does not provide SpeexDSP/miniaudio development packages, so these runs use the
existing deterministic SRC fallback and no-device audio stub. Production SpeexDSP/miniaudio + Qt
are validated by `bootstrap-dev.sh` on Cortana.

## Result

Headless GCC: **12/12 tests passed**.

Headless Clang ASan/UBSan: **12/12 tests passed**, with no sanitizer diagnostics.

Phase-5 additions covered by the suite:

- `fv1-validation-tests` recovers a planted 37-frame capture delay;
- raw -1.938 dB gain error from a 0.8x synthetic capture is measured while optional gain matching
  reduces the residual to the numerical floor;
- deliberately corrupted synthetic capture fails configured limits;
- validation WAV write/read round-trip;
- JSON/Markdown/frequency-CSV/residual-WAV report bundle generation;
- `phase5-validation-cli` generates a deterministic stimulus and validates an identical capture;
- identical validation produces zero delay, unity correlation and residual at the numerical floor.

The install-stage smoke check contains:

- `include/fv1/validation.hpp`;
- `lib/libfv1-validation.a`;
- four selectable 512x512 icon assets;
- the default hicolor Linux application icon;
- `share/applications/roth-fv1-emulator.desktop`.

## Remaining acceptance gate

On Cortana, the production build should add `fv1-lab-smoke` for **13 total tests** and compile the
new Qt `VALIDATION` panel against Qt 6.4.2, SpeexDSP 1.2.1 and miniaudio 0.11.21.
