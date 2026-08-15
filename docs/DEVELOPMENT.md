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
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

## libFuzzer differential target

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
```

See `PHASE5C-MODEL-HARDENING.md` for the model/test architecture.
