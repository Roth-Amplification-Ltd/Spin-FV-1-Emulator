#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()

paths = [
    *sorted((root / "src/gui").glob("*.cpp")),
    *sorted((root / "include/fv1/gui").glob("*.hpp")),
]

if not paths:
    raise SystemExit("ERROR: Qt frontend source tree is missing")

forbidden = {
    "Win32 API header": re.compile(r"#\s*include\s*[<\"]windows\.h[>\"]", re.I),
    "WASAPI header": re.compile(r"#\s*include\s*[<\"].*(?:audioclient|mmdeviceapi)\.h[>\"]", re.I),
    "Apple UI header": re.compile(r"#\s*include\s*[<\"].*(?:AppKit|Cocoa|CoreAudio).*", re.I),
    "Linux/X11 header": re.compile(r"#\s*include\s*[<\"](?:X11|alsa|pulse)/", re.I),
    "native Windows implementation dependency": re.compile(r"(?:src/windows/|fv1_session\.hpp|wasapi_engine\.hpp)", re.I),
}

errors: list[str] = []

for path in paths:
    text = path.read_text(encoding="utf-8", errors="replace")
    rel = path.relative_to(root)
    for label, pattern in forbidden.items():
        match = pattern.search(text)
        if match:
            line = text.count("\n", 0, match.start()) + 1
            errors.append(f"{rel}:{line}: {label}: {match.group(0)}")

cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
required = [
    "find_package(Qt6 COMPONENTS Widgets QUIET)",
    "src/gui/main_window.cpp",
    "Qt6::Widgets",
]
for fragment in required:
    if fragment not in cmake:
        errors.append(f"CMakeLists.txt: shared Qt frontend requirement missing: {fragment}")

if errors:
    print("Qt desktop frontend boundary FAILED:", file=sys.stderr)
    for error in errors:
        print(f"  {error}", file=sys.stderr)
    raise SystemExit(1)

print(
    f"Qt desktop frontend boundary OK: {len(paths)} source/header files; "
    "shared Linux/Windows Qt layer contains no native OS UI/audio dependencies"
)
