# Phase 7A headless test plan

1. Configure a clean non-Qt build.
2. Build the full headless target set with GCC or Clang.
3. Run the complete existing CTest suite plus `fv1-native-frontend-session-tests`.
4. Confirm the new session wrapper compiles/loads the four-instruction passthrough through the public SDK.
5. Confirm 512 deterministic probe samples are finite and advance the public snapshot sample counter.
6. Confirm resource analysis reports four used instructions.
7. Re-run the exact Phase 6C symbol manifest and ABI-layout tests unchanged.
8. On GitHub Windows/MSVC, build `fv1-lab-win32` against both shared and static `FV1::sdk`.

A Phase 7A pass is a frontend-boundary pass, not a physical FV-1 silicon claim and not a realtime WASAPI acceptance claim.
