#!/usr/bin/env python3
"""Fail if the native Windows frontend reaches around the public FV1SDK ABI."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WINDOWS_DIR = ROOT / "src" / "windows"

ALLOWED_FV1_HEADERS = {"fv1/sdk.h", "fv1/sdk_debug.h"}
FORBIDDEN_TOKENS = (
    "Qt",
    "miniaudio",
    "fv1/runtime.hpp",
    "fv1/fv1.h",
    "fv1/fv1.hpp",
    "fv1/spinasm.hpp",
    "fv1/debugger.hpp",
    "fv1/validation.hpp",
    "fv1/reference_model.hpp",
    "fv1/conformance.hpp",
)
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)


def main() -> int:
    failures: list[str] = []
    files = sorted(p for p in WINDOWS_DIR.rglob("*") if p.suffix in {".h", ".hpp", ".c", ".cpp"})
    if not files:
        print("FAIL: no native Windows frontend sources found", file=sys.stderr)
        return 1

    for path in files:
        text = path.read_text(encoding="utf-8")
        relative = path.relative_to(ROOT)
        for header in INCLUDE_RE.findall(text):
            if header.startswith("fv1/") and header not in ALLOWED_FV1_HEADERS:
                failures.append(f"{relative}: private FV-1 header include <{header}>")
        for token in FORBIDDEN_TOKENS:
            if token in text:
                failures.append(f"{relative}: forbidden frontend dependency token {token!r}")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print(f"Phase 7A Windows frontend boundary OK: {len(files)} source/header files, public SDK only")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
