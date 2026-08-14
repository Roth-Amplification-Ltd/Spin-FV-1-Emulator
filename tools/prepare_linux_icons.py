#!/usr/bin/env python3
"""Prepare the approved FV-1 badge PNGs for freedesktop desktops.

This script does not redraw or recolor the icon artwork. It only makes the
already-black canvas *outside* the existing rounded metallic badge transparent,
then derives standard hicolor sizes from the corrected Silver master.

Requires Pillow for regeneration; the repository already stores generated PNGs
so end users do not need Pillow to build/run FV-1 Lab.
"""
from __future__ import annotations

from pathlib import Path
import argparse
import numpy as np
from PIL import Image

SIZES = (16, 24, 32, 48, 64, 128, 256, 512)


def mask_existing_badge(path: Path) -> None:
    image = Image.open(path).convert("RGBA")
    rgba = np.array(image)
    brightness = rgba[:, :, :3].max(axis=2)
    height, width = brightness.shape
    alpha = np.full((height, width), 255, dtype=np.uint8)
    band = min(96, height // 3, width // 3)

    # The approved artwork already contains the rounded metallic outer frame.
    # Find that frame only in the corner bands, then clear pixels outside it.
    # RGB pixels inside the badge are left completely untouched.
    threshold = 8
    rows = list(range(band)) + list(range(height - band, height))
    for y in rows:
        left = np.where(brightness[y, :band] > threshold)[0]
        if len(left):
            boundary = int(left.min())
            alpha[y, : max(0, boundary - 1)] = 0
            if boundary - 1 >= 0:
                alpha[y, boundary - 1] = 64
            alpha[y, boundary] = 192

        right = np.where(brightness[y, width - band :] > threshold)[0]
        if len(right):
            boundary = width - band + int(right.max())
            if boundary + 2 < width:
                alpha[y, boundary + 2 :] = 0
            if boundary + 1 < width:
                alpha[y, boundary + 1] = 64
            alpha[y, boundary] = 192

    rgba[:, :, 3] = alpha
    Image.fromarray(rgba, "RGBA").save(path, optimize=True)


def generate_hicolor(icon_dir: Path) -> None:
    silver = Image.open(icon_dir / "fv1-emulator-silver.png").convert("RGBA")
    for size in SIZES:
        target_dir = icon_dir / "hicolor" / f"{size}x{size}" / "apps"
        target_dir.mkdir(parents=True, exist_ok=True)
        resized = silver.copy() if size == 512 else silver.resize((size, size), Image.Resampling.LANCZOS)
        # Downsampling can leave a nearly-transparent black corner pixel. Clear
        # those sub-25%-coverage pixels so small dock/menu sizes stay clean.
        rgba = np.array(resized)
        rgba[:, :, 3] = np.where(rgba[:, :, 3] < 64, 0, rgba[:, :, 3]).astype(np.uint8)
        Image.fromarray(rgba, "RGBA").save(target_dir / "roth-fv1-emulator.png", optimize=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("icon_dir", nargs="?", type=Path,
                        default=Path(__file__).resolve().parents[1] / "assets" / "icons")
    args = parser.parse_args()
    for path in sorted(args.icon_dir.glob("fv1-emulator-*.png")):
        mask_existing_badge(path)
        print(f"alpha-masked existing badge: {path}")
    generate_hicolor(args.icon_dir)
    print("generated freedesktop hicolor sizes from fv1-emulator-silver.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
