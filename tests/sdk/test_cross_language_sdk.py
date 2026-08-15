#!/usr/bin/env python3
"""Exercise the installed FV1SDK from language/toolchain boundaries.

Required on every platform: C++ CMake consumer and Python ctypes consumer.
Optional when toolchains are present: Swift execution, Rust execution, and an
Objective-C syntax/import probe. Missing optional compilers are reported as
SKIP rather than silently treated as coverage.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


def run(cmd, *, env=None, cwd=None):
    print("+", " ".join(map(str, cmd)), flush=True)
    subprocess.run([str(x) for x in cmd], check=True, env=env, cwd=cwd)


def runtime_env(prefix: Path):
    env = os.environ.copy()
    lib = str(prefix / "lib")
    bin_dir = str(prefix / "bin")
    if sys.platform == "darwin":
        env["DYLD_LIBRARY_PATH"] = lib + os.pathsep + env.get("DYLD_LIBRARY_PATH", "")
    elif os.name == "nt":
        env["PATH"] = bin_dir + os.pathsep + lib + os.pathsep + env.get("PATH", "")
    else:
        env["LD_LIBRARY_PATH"] = lib + os.pathsep + env.get("LD_LIBRARY_PATH", "")
    return env


def find_library(prefix: Path) -> Path:
    candidates = []
    if os.name == "nt":
        candidates += list((prefix / "bin").glob("*fv1-sdk*.dll"))
    elif sys.platform == "darwin":
        candidates += list((prefix / "lib").glob("libfv1-sdk*.dylib"))
    else:
        candidates += [p for p in (prefix / "lib").glob("libfv1-sdk.so*") if p.is_file()]
    if not candidates:
        raise RuntimeError(f"could not locate installed FV1SDK shared library under {prefix}")
    return sorted(candidates, key=lambda p: (len(p.name), p.name))[0]


def executable(build: Path, name: str) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    direct = build / (name + suffix)
    if direct.exists():
        return direct
    for cfg in ("Debug", "Release", "RelWithDebInfo", "MinSizeRel"):
        candidate = build / cfg / (name + suffix)
        if candidate.exists():
            return candidate
    raise RuntimeError(f"could not locate {name} under {build}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-binary", required=True)
    ap.add_argument("--source", required=True)
    ap.add_argument("--test-root", required=True)
    args = ap.parse_args()

    project_binary = Path(args.project_binary).resolve()
    source = Path(args.source).resolve()
    root = Path(args.test_root).resolve()
    prefix = root / "prefix"
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)

    run(["cmake", "--install", project_binary, "--prefix", prefix])
    env = runtime_env(prefix)
    library = find_library(prefix)

    # C++: consume only installed <fv1/sdk.hpp> and FV1SDK::sdk.
    cpp_build = root / "cpp-build"
    run(["cmake", "-S", source / "examples/sdk-hosts/cpp", "-B", cpp_build,
         f"-DCMAKE_PREFIX_PATH={prefix}"])
    run(["cmake", "--build", cpp_build])
    run([executable(cpp_build, "fv1-sdk-cpp-host")], env=env)

    # Python: direct dynamic-library FFI with no extension module.
    py_env = env.copy()
    py_env["FV1_SDK_LIBRARY"] = str(library)
    run([sys.executable, source / "examples/sdk-hosts/python/host.py"], env=py_env)

    # Swift: real Clang-module import and execution when Swift is installed.
    swiftc = shutil.which("swiftc")
    if swiftc and os.name != "nt":
        swift_out = root / "fv1-sdk-swift-host"
        module_map = prefix / "include/fv1/module.modulemap"
        cmd = [swiftc, source / "examples/sdk-hosts/swift/main.swift",
               "-I", prefix / "include",
               "-Xcc", f"-fmodule-map-file={module_map}",
               "-L", prefix / "lib", "-lfv1-sdk", "-o", swift_out]
        run(cmd)
        run([swift_out], env=env)
        print("CROSS-LANGUAGE: Swift RUN PASS")
    else:
        print("CROSS-LANGUAGE: Swift SKIP (swiftc unavailable on this runner)")

    # Rust: deliberately hand-written FFI proves the C ABI needs no C++ shim.
    rustc = shutil.which("rustc")
    if rustc:
        rust_out = root / "fv1-sdk-rust-host"
        run([rustc, source / "examples/sdk-hosts/rust/main.rs",
             "-L", f"native={prefix / 'lib'}", "-l", "dylib=fv1-sdk", "-o", rust_out])
        run([rust_out], env=env)
        print("CROSS-LANGUAGE: Rust RUN PASS")
    else:
        print("CROSS-LANGUAGE: Rust SKIP (rustc unavailable on this runner)")

    # Objective-C: Linux often lacks libobjc/Foundation, but clang can still
    # prove that the installed SDK header is valid in an Objective-C TU.
    clang = shutil.which("clang")
    if clang and os.name != "nt":
        run([clang, "-x", "objective-c", "-fsyntax-only", "-Werror",
             "-I", prefix / "include", source / "examples/sdk-hosts/objective-c/main.m"])
        print("CROSS-LANGUAGE: Objective-C HEADER PASS")
    else:
        print("CROSS-LANGUAGE: Objective-C SKIP (clang unavailable on this runner)")

    print(f"CROSS-LANGUAGE: C++ RUN PASS\nCROSS-LANGUAGE: Python ctypes RUN PASS\nSDK LIBRARY: {library}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
