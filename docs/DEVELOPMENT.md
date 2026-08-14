# Development environment convention

Every software project maintained in this project family should include an executable
`bootstrap-dev.sh` at the repository root.

The purpose is to make a fresh development checkout self-describing and immediately
buildable without relying on undocumented machine setup.

For Linux-first projects, the bootstrap should:

1. Detect whether the required compiler, build system, language runtimes, and project
   utilities are present.
2. Install missing prerequisites on the project's supported Linux distribution(s).
3. Print the resolved toolchain and versions.
4. Configure the project from a clean checkout.
5. Build it.
6. Run its automated tests by default.
7. Be safe to rerun (idempotent).
8. Provide `--check` so CI/users can audit the environment without changing it.
9. Provide `--clean` when stale build state must be discarded.
10. Fail early with an actionable message on unsupported hosts.

For Spin-FV-1-Emulator Phase 2, the supported automatic-bootstrap hosts are Pop!_OS,
Ubuntu, Debian, and apt-compatible derivatives. The Phase-2 bootstrap also checks/installs
miniaudio development headers and SpeexDSP so realtime audio and production SRC are not
silently omitted on a newly cloned machine. Windows and macOS bootstrap scripts
will be added when those ports begin; the emulator core itself remains platform-neutral.

## First build

```bash
./bootstrap-dev.sh
```

## Verify without changing the machine

```bash
./bootstrap-dev.sh --check
```

## Fresh Clang build

```bash
./bootstrap-dev.sh --compiler clang --clean
```
