#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
scan_roots = [ROOT / "apple", ROOT / "src" / "apple"]
forbidden = [
    "fv1/fv1.h", "fv1/fv1.hpp", "fv1/runtime.hpp", "fv1/spinasm.hpp",
    "fv1/debugger.hpp", "fv1/validation.hpp", "fv1/reference_model.hpp",
    "fv1/conformance.hpp", "#include <Qt", "#include \"Qt", "import UIKit",
    "miniaudio", "src/core/", "src/spinasm/", "src/runtime/"
]
violations = []
files = []
for base in scan_roots:
    if not base.exists():
        continue
    for path in base.rglob("*"):
        if path.suffix.lower() not in {".swift", ".h", ".c", ".m", ".mm", ".cpp", ".pbxproj", ".xcscheme"}:
            continue
        files.append(path)
        text = path.read_text(errors="replace")
        if path.suffix.lower() not in {".pbxproj", ".xcscheme"}:
            for needle in forbidden:
                if needle in text:
                    violations.append(f"{path.relative_to(ROOT)}: forbidden dependency/reference {needle!r}")

project = ROOT / "apple" / "FV1Lab.xcodeproj" / "project.pbxproj"
if project.exists():
    text = project.read_text(errors="replace")
    if "TARGETED_DEVICE_FAMILY = 1" in text or 'TARGETED_DEVICE_FAMILY = "1' in text:
        violations.append("Xcode project enables iPhone device family; Phase 8 is macOS + iPadOS only")

required = [
    ROOT / "apple" / "FV1Lab.xcodeproj" / "project.pbxproj",
    ROOT / "apple" / "FV1Lab.xcodeproj" / "xcshareddata" / "xcschemes" / "FV1 Lab macOS.xcscheme",
    ROOT / "apple" / "FV1Lab.xcodeproj" / "xcshareddata" / "xcschemes" / "FV1 Lab iPadOS.xcscheme",
]
for path in required:
    if not path.exists():
        violations.append(f"missing required Apple project artifact: {path.relative_to(ROOT)}")

if violations:
    print("Phase 8 Apple frontend boundary FAILED:")
    for item in violations:
        print(f"  - {item}")
    sys.exit(1)
print(f"Phase 8 Apple frontend boundary OK: {len(files)} Apple source/project files; public SDK only; no iPhone target")
