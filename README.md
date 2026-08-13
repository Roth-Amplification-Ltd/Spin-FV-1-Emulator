# Spin FV-1 Emulator

Linux-first, open-source development tools for emulating and debugging the Spin Semiconductor FV-1 DSP.

**Current milestone: Phase 1 — headless emulator foundation.**

The project is intentionally layered so the virtual FV-1 is not tied to a GUI toolkit, audio-device framework, or operating system. Linux is the only supported/tested host during the initial implementation. Windows and macOS front ends come later, after the core/runtime APIs are stable.

## Phase 1 status

Implemented now:

- `libfv1-core` C++20 static library with a stable C API
- 128-word / 512-byte external FV-1 program loading
- predecoded instruction representation for the realtime execution path
- signed 24-bit saturating datapath
- ACC, PACC, LR and the FV-1 register bank
- 9-bit POT input quantization
- 32K circular delay memory
- sine/cosine and ramp LFO state
- RDA, RMPA, WRA, WRAP, RDAX, RDFX/LDAX, WRAX, WRHX, WRLX, MAXX/ABSA, MULX, LOG, EXP, SOF, AND/CLR, OR, XOR/NOT, SKP/NOP, WLDS, WLDR, JAM and CHO execution paths
- instruction-level sample stepping and state snapshots
- static program resource analysis
- SpinASM-compatible assembler integration
- `.spn`, 512-byte `.bin`, 4 KiB bank and Intel HEX program loading through `fv1-cli`
- deterministic offline WAV rendering at the configured virtual FV-1 rate
- Linux CMake/CTest build and regression suite

The repository also carries the eight **Steal This DSP** factory programs as non-authoritative regression/examples so the emulator is continuously exercised with real effects rather than only synthetic opcodes.

## Build on Linux

Requirements:

- CMake 3.20+
- Ninja
- GCC or Clang with C++20 support
- Python 3 (Phase-1 SpinASM assembler bridge)

```bash
sudo apt install build-essential cmake ninja-build python3

git clone https://github.com/Roth-Amplification-Ltd/Spin-FV-1-Emulator.git
cd Spin-FV-1-Emulator
make test
```

Or directly:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

## `fv1-cli`

Compile a SpinASM program:

```bash
./build/fv1-cli assemble effect.spn effect.bin
```

Inspect virtual DSP resource usage:

```bash
./build/fv1-cli inspect effect.spn
```

Step through one virtual sample instruction by instruction:

```bash
./build/fv1-cli step effect.spn \
  --in-l 0.25 --in-r -0.10 \
  --pot0 0.50 --pot1 0.70 --pot2 0.30
```

Render a WAV file offline:

```bash
./build/fv1-cli render effect.spn input-32768.wav output.wav \
  --pot0 0.50 --pot1 0.50 --pot2 0.50
```

Phase 1 deliberately requires the WAV sample rate to match the selected virtual FV-1 rate. Host↔FV-1 sample-rate conversion belongs to Phase 2 so the Phase-1 renderer cannot silently give the program the wrong timing.

## Resource analyzer

`fv1-cli inspect` currently reports:

- program words used / 128
- worst-case forward-SKP instruction path
- static and dynamic delay reads
- static delay writes
- highest statically referenced delay address
- general register usage
- POT usage
- SIN/COS LFO usage
- RAMP LFO usage
- SKP count
- opcode histogram

These values are intended to feed the **Virtual DSP Resource Usage** panel in the future GUI.

## Debugger API

The core exposes two execution styles:

1. normal block/sample processing;
2. instruction-step mode for the future debugger.

The debugger can begin a virtual audio sample, execute one FV-1 instruction at a time, inspect ACC/PACC/LR/register/LFO state, observe SKP branches, and then retrieve the completed stereo DAC sample.

## Fidelity policy

The project aims for hardware-oriented emulation, but Phase 1 does **not** claim bit-exact FV-1 equivalence yet.

Known fidelity items reserved for hardware/reference validation include:

- exact proprietary delay-RAM floating-point representation;
- final CHO LFO address/fraction bit partition and corner cases;
- exhaustive LOG/EXP edge behavior;
- analog ADC/DAC filtering and converter latency;
- POT hysteresis around code boundaries.

The default `FV1_DELAY_REFERENCE_16` mode intentionally reduces delay-memory precision. A `FV1_DELAY_FULL_24` diagnostic mode is provided to separate algorithm behavior from that approximation.

That distinction is deliberate: unknown hardware details stay labeled as unknown instead of being presented as fake precision.

## Future audio sources are already an architectural requirement

Phase 2 will make the same virtual-FV-1 processing path accept three interchangeable sources:

- **live audio interface input**;
- **imported audio-file playback with seamless looping and loop-region controls**;
- deterministic **test generators** such as impulse, sine, sweep and noise.

The future GUI will therefore be able to repeatedly loop a guitar/drum/etc. recording while an effect is edited and debugged, or process a live instrument through the audio interface.

See [`docs/ROADMAP.md`](docs/ROADMAP.md) and [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## License

Mozilla Public License 2.0. See [`LICENSE`](LICENSE).


## One-command development setup

On a fresh Pop!_OS/Ubuntu/Debian development machine, use the repository bootstrap first:

```bash
./bootstrap-dev.sh
```

It verifies/installs the Linux build environment, configures the project, builds it, and runs the test suite. Use `./bootstrap-dev.sh --check` to audit the environment without changing it. See `docs/DEVELOPMENT.md` for the project-wide bootstrap convention.

