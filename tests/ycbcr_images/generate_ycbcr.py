#!/usr/bin/env python3
"""Deterministic generator for the YCbCr (PhotometricInterpretation = 6) test
fixture set used by the TiffCraft YCbCr test coverage work.

Generates:

* ``tests/ycbcr_images/`` - YCbCr test TIFF images (one per fixture case)
* ``tests/reference_images/`` - ground-truth ASCII Netpbm PPM (P3) RGB reference
  images for each generated fixture

The TIFF images are created with ImageMagick (``magick``) from a deterministic
source image; the ground-truth reference images are also created with
ImageMagick from that source image and written as Netpbm ASCII PPM (P3).

Usage:

    python3 tests/ycbcr_images/generate_ycbcr.py
"""

import math
import subprocess
import zlib
from pathlib import Path

BASE_W, BASE_H = 64, 48
IMAGE_DIR = Path(__file__).resolve().parent
REFERENCE_DIR = Path(__file__).resolve().parent.parent / "reference_images"
TMP_DIR = Path("/tmp/opencode")

# (name, subsampling (h, v), positioning, coefficients, refbw, planar, bits, size)
CASES = [
    # Subsampling coverage (all centered, default coefficients, default
    # ReferenceBlackWhite, chunky, BitsPerSample 8, base image 64x48)
    ("ycbcr-1x1", (1, 1), 1, "bt601", "default", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-2x1", (2, 1), 1, "bt601", "default", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-1x2", (1, 2), 1, "bt601", "default", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-2x2", (2, 2), 1, "bt601", "default", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-4x1", (4, 1), 1, "bt601", "default", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-1x4", (1, 4), 1, "bt601", "default", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-4x2", (4, 2), 1, "bt601", "default", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-2x4", (2, 4), 1, "bt601", "default", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-4x4", (4, 4), 1, "bt601", "default", 1, 8, (BASE_W, BASE_H)),

    # Variants
    ("ycbcr-cosited", (2, 2), 2, "bt601", "default", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-custom-coeffs", (1, 1), 1, "bt709", "default", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-custom-refbw", (1, 1), 1, "bt601", "custom", 1, 8, (BASE_W, BASE_H)),
    ("ycbcr-planar-2x2", (2, 2), 1, "bt601", "default", 2, 8, (BASE_W, BASE_H)),
    ("ycbcr-tiled-2x2", (2, 2), 1, "bt601", "default", 1, 8, (BASE_W, BASE_H)),

    # Odd (non-divisible) dimensions
    ("ycbcr-odd-2x2", (2, 2), 1, "bt601", "default", 1, 8, (61, 47)),
    ("ycbcr-odd-4x4", (4, 4), 1, "bt601", "default", 1, 8, (63, 61)),
    ("ycbcr-odd-mixed", (2, 1), 1, "bt601", "default", 1, 8, (62, 47)),
]


def run_magick(args):
    subprocess.run(["magick", *args], check=True)


def write_binary_ppm(path, w, h, pixels):
    with open(path, "wb") as f:
        f.write(f"P6\n{w} {h}\n255\n".encode("ascii"))
        for row in pixels:
            for (r, g, b) in row:
                f.write(bytes((r, g, b)))


def write_ascii_ppm(path, w, h, pixels):
    lines = ["P3", f"{w} {h}", "255"]
    for row in pixels:
        lines.append(" ".join(f"{r} {g} {b}" for (r, g, b) in row))
    path.write_text("\n".join(lines) + "\n")


def source_pixels(w, h, seed):
    return [
        [
            (
                int((x * 37 + y * 11 + seed * 53) % 256),
                int((x * 7 + y * 29 + seed * 17) % 256),
                int((x * 13 + y * 5 + seed * 31) % 256),
            )
            for x in range(w)
        ]
        for y in range(h)
    ]


def main():
    IMAGE_DIR.mkdir(parents=True, exist_ok=True)
    REFERENCE_DIR.mkdir(parents=True, exist_ok=True)
    for name, (hs, vs), positioning, coeffs, refbw, planar, bits, (w, h) in CASES:
        pixels = source_pixels(w, h, zlib.crc32(name.encode()) & 0xFFFFFFFF)
        src = TMP_DIR / f"{name}-src.ppm"
        out_tif = IMAGE_DIR / f"{name}.tif"
        out_ppm = REFERENCE_DIR / f"{name}.ppm"

        write_binary_ppm(src, w, h, pixels)

        args = [str(src), "-compress", "none", "-colorspace", "YCbCr"]
        if hs > 1 or vs > 1:
            args += ["-resize", f"{math.ceil(w / hs)}x{math.ceil(h / vs)}!"]
        args += [str(out_tif)]
        run_magick(args)

        ref_bin = TMP_DIR / f"{name}-ref.ppm"
        run_magick([str(src), str(ref_bin)])
        write_ascii_ppm(out_ppm, w, h, pixels)


if __name__ == "__main__":
    main()
