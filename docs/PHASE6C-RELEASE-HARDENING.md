# Phase 6C — Linux 1.0.0-rc1 Release Hardening

## Purpose

Phase 6C is a release-candidate and compatibility-ratification phase, not a feature phase. The Linux
Qt testbench stays visually/behaviorally frozen while the SDK, parsers, installation surface and
emulator execution paths are subjected to broader failure-oriented testing.

The desired exit state is a candidate that can safely become **FV-1 SDK ABI v1 FROZEN** after the
same committed revision passes Rosie and remote Linux/macOS/Windows gates. This source tree itself
still labels ABI v1 **not frozen**.

## Release candidate identity

- product / implementation version: `1.0.0-rc1`;
- public ABI identifier: candidate `1.0`;
- shared-library SOVERSION: `1`;
- SDK-only CMake package requirement used by proving hosts: `FV1SDK 1.0`.

The implementation version can advance within ABI v1 after ratification. The ABI identifier is the
binary compatibility contract and changes only under the rules in `SDK-ABI-POLICY.md`.

## Hardening added in Phase 6C

### Compatibility tripwires

The exact Phase-6B/0.9.0 public headers are preserved under `tests/sdk/abi-baseline/0.9.0/`.
A current library must still compile/link/run a consumer built against that older public surface.
Existing 64-bit `sizeof`/`offsetof` fixtures and exact exported-symbol manifests remain mandatory.

### Public SDK abuse testing

`fv1-sdk-abuse-tests` attacks invalid/undersized/wrong-major records, invalid engine configuration,
program size errors, non-finite controls, zero-frame processing, null buffers, malformed debug state
transitions, delay bounds and inspection structures. The objective is deterministic error results and
no crash/undefined behavior across the public ABI.

Host inputs containing NaN or infinity are handled at the software observer boundary rather than
being allowed to reach undefined C/C++ numeric conversions. Non-finite configuration/POT values are
rejected; non-finite audio samples are converted deterministically by the virtual input quantizer.
This is host-interface hardening, not a new claim about analog FV-1 silicon behavior.

### External-input assault

`phase6c-malformed-inputs` covers undersized/oversized raw programs, invalid Intel HEX checksums,
invalid SpinASM and truncated/garbage WAV input. CLI failures must be normal diagnostic exits rather
than signals, out-of-bounds access or uncontrolled allocation.

### Differential stress

`phase6c-differential-stress` runs every bundled Steal This DSP program across eight deterministic
seeds plus a repeated determinism run per program. Production/reference state and delay digests must
agree for every run. This supplements, rather than replaces, the faster randomized instruction tests.

### Fuzzing

Three Clang/libFuzzer + ASan/UBSan surfaces are release-gated:

1. production/reference conformance input;
2. native SpinASM compiler input;
3. public SDK create/load/control/process/debug operation sequences.

The normal CI smoke uses 1,000 runs per target. `tools/run-release-gate.sh` defaults to 5,000 runs per
target and can be increased with `FV1_FUZZ_RUNS` for longer release soaks.

### Installation and versioning

The staged product test verifies CLI/live binaries, optional GUI when built, Linux desktop metadata,
hicolor icon installation, public SDK headers/CMake package, absence of private headers, and execution
of the installed `fv1-cli --version` through the installed SDK runtime.

The Qt application keeps its accepted window flags, dock/taskbar identity, icons and approved dashboard
layout. Before the RC commit, the previously reserved splash-photo hook is populated with the user-supplied
`assets/splash/FV1LabSplashImagebase.png`. `StartupSplash` locates the same exact asset in both the source
tree and installed product tree, full-bleed crops it, applies the active accent as a restrained software tint,
and darkens it beneath the existing FV-1/waveform/DIP/progress foreground. Missing/corrupt images fall
back to the existing rendered gradient instead of preventing startup.

## Cross-platform SDK ratification

The Phase-6B remote run proved Linux shared/static, macOS shared/static and Windows static SDK jobs.
The Windows shared job built and passed its native SDK tests but its Python `ctypes` probe could not
load a MinGW-built DLL because the workflow accidentally configured Windows through Git Bash/Ninja
and therefore inherited MinGW runtime DLL search requirements.

Phase 6C changes Windows SDK portability jobs to the native Visual Studio/MSVC generator and carries
configuration names through install/external-host tests. This is the intended shipping Windows ABI
proof. Python also explicitly adds the installed DLL directory to its Windows search path.

## Ratification gate

ABI v1 may be called **FROZEN** only after all of these are true for one committed RC revision:

- Rosie normal Qt build passes both standard and HiDPI smoke tests and the user accepts normal app behavior;
- normal Linux regression suite is green;
- SDK-only shared and static suites are green;
- GCC and Clang release builds are warning-clean under project warnings;
- Clang ASan/UBSan suite is green;
- all three fuzz targets complete the release run without findings;
- installed-product smoke is green;
- 0.9.0 public-header backward-compatibility test is green;
- exact ABI layout and exported-symbol fixtures remain unchanged;
- SDK Portability is green for Linux/macOS/Windows × shared/static;
- Release Hardening CI is green.

If a gate reveals a necessary breaking ABI change, the correct response is to revise the candidate
and repeat the gate—not to freeze and immediately create an exception.

## Deferred items

Physical silicon validation remains separate. Likewise, native Windows/macOS frontends begin only
after the shared SDK boundary is ratified; they are independent native clients, not ports of the Qt
private architecture.

### Branded About / credits surface

Before the RC commit, the splash presentation also became the application's canonical About surface.
The startup splash now identifies **Adam Vadala-Roth** as creator/engineer, retains the permanent
`© 2026 Roth Amplification LTD` notice, and identifies the Mozilla Public License 2.0. The main menu
adds **Help → About FV-1 Lab…**, which reopens the same photo-backed FV-1 artwork as a conventional
owned dialog and replaces startup progress with author, company, product-version, license, and
copyright information. This keeps branding/credits in one renderer rather than maintaining a second
visual implementation. The Qt smoke suite verifies both the menu action and the on-demand About
presentation.
