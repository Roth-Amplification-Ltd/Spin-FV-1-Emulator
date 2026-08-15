# Development

## Normal build/test

The repository convention is clone → bootstrap → working build/test:

```bash
./bootstrap-dev.sh --clean
```

For an already provisioned development machine:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

## Phase 6B SDK-only build/install

Use the SDK-only configure when testing the embeddable module. It intentionally avoids Linux product
dependencies and builds the public package plus SDK-facing tests only:

```bash
cmake -S . -B build-sdk -G Ninja \
  -DFV1_SDK_ONLY=ON \
  -DFV1_BUILD_TESTS=ON \
  -DFV1_SDK_BUILD_SHARED=ON
cmake --build build-sdk
ctest --test-dir build-sdk --output-on-failure
cmake --install build-sdk --prefix /tmp/fv1-sdk
```

The installed package exposes `FV1SDK::sdk`, `<fv1/sdk.h>`, `<fv1/sdk_debug.h>`, the header-only
`<fv1/sdk.hpp>` convenience wrapper, and `module.modulemap`. Internal emulator headers are deliberately
not part of the SDK development install.

The static form is self-contained and no longer exports private core/compiler targets:

```bash
cmake -S . -B build-sdk-static -G Ninja \
  -DFV1_SDK_ONLY=ON \
  -DFV1_BUILD_TESTS=ON \
  -DFV1_SDK_BUILD_SHARED=OFF
cmake --build build-sdk-static
ctest --test-dir build-sdk-static --output-on-failure
```

Cross-language consumers are exercised by `sdk-cross-language-consumers`. The test always executes the
installed C++ and Python hosts; Swift, Rust and Objective-C probes execute when the matching toolchain
is present and otherwise report an explicit SKIP.

See `SDK-CONSUMER-REQUIREMENTS.md`, `SDK-CROSS-LANGUAGE.md`, `SDK-ARCHITECTURE.md`, and
`SDK-ABI-POLICY.md`.

## Phase 5C conformance

Compare the production engine against the independent reference model:

```bash
./build/fv1-cli conformance \
  examples/steal-this-dsp-programs/00_55_gallon_saint.spn \
  --samples 256 --seed 0x4656315c2026
```

The harness compares every executed instruction and full architectural state, plus complete delay-state
digests at sample boundaries. A PASS is differential agreement under the current Hardware Emulation
Contract, not a claim of physical-silicon equivalence.

## Sanitizer build

```bash
cmake -S . -B build-san -G Ninja \
  -DFV1_BUILD_GUI=OFF \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' \
  -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

`vptr` is excluded from this cross-language shared-library sanitizer configuration so an
instrumented SDK can still be linked and exercised by the C-only external-host smoke test using the
C sanitizer runtime. AddressSanitizer and the remaining UndefinedBehaviorSanitizer checks stay active.

## libFuzzer hardening targets

```bash
cmake -S . -B build-fuzz -G Ninja \
  -DFV1_BUILD_GUI=OFF \
  -DFV1_BUILD_FUZZERS=ON \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz

mkdir -p /tmp/fv1-fuzz-corpus
./build/fv1-cli assemble examples/simple_passthrough.spn /tmp/fv1-fuzz-corpus/passthrough.bin
./build-fuzz/fv1-conformance-fuzzer -runs=10000 -max_len=521 /tmp/fv1-fuzz-corpus

mkdir -p /tmp/fv1-spinasm-fuzz-corpus
printf 'RDAX ADCL, 1.0\nWRAX DACL, 0\n' > /tmp/fv1-spinasm-fuzz-corpus/passthrough.spn
./build-fuzz/fv1-spinasm-fuzzer -runs=10000 -max_len=65536 /tmp/fv1-spinasm-fuzz-corpus
```

See `PHASE5C-MODEL-HARDENING.md` for differential-model testing and `PHASE6B-SDK-STABILIZATION.md` for the current SDK boundary.
