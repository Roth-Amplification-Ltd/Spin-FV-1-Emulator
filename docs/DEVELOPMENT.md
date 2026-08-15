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

## Phase 6A SDK build/install

The default build produces a shared SDK candidate. Install it into a disposable prefix and build the
external C host exactly as a third-party application would:

```bash
cmake -S . -B build-sdk -G Ninja \
  -DFV1_BUILD_GUI=OFF -DFV1_BUILD_TESTS=ON
cmake --build build-sdk
ctest --test-dir build-sdk -R 'fv1-sdk|sdk-export|native-spinasm|installed-sdk' --output-on-failure

cmake --install build-sdk --prefix /tmp/fv1-sdk
cmake -S examples/sdk-host -B /tmp/fv1-sdk-host -G Ninja \
  -DCMAKE_PREFIX_PATH=/tmp/fv1-sdk
cmake --build /tmp/fv1-sdk-host
/tmp/fv1-sdk-host/fv1-sdk-host
```

The shared consumer above is a C-only project. To exercise the supported static form:

```bash
cmake -S . -B build-sdk-static -G Ninja \
  -DFV1_BUILD_GUI=OFF -DFV1_SDK_BUILD_SHARED=OFF
cmake --build build-sdk-static
ctest --test-dir build-sdk-static -R 'fv1-sdk|native-spinasm|installed-sdk' --output-on-failure
```

Only `FV1SDK::sdk` / `<fv1/sdk.h>` are Phase-6A public ABI candidates. Underscore-prefixed imported
targets in the package are private static-link implementation dependencies and must not be consumed
directly. See `SDK-ARCHITECTURE.md` and `SDK-ABI-POLICY.md`.

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

See `PHASE5C-MODEL-HARDENING.md` for differential-model testing and `PHASE6A-SDK-EXTRACTION.md` for the SDK/parser surface.
