#!/usr/bin/env python3
"""Deterministic generator for the two retained YCbCr
(PhotometricInterpretation = 6) test fixtures.

Generates:

* ``tests/ycbcr_images/`` - a full-resolution stripped YCbCr TIFF and a tiled
  YCbCr TIFF (both 64x48, 1x1 subsampling, all other YCbCr tags at defaults)
* ``tests/reference_images/`` - ground-truth ASCII Netpbm PPM (P3) RGB reference
  images for each generated fixture

The TIFF images are created with ImageMagick (``magick``) from a deterministic
source image; the ground-truth reference images are also created with
ImageMagick from that source image and written as Netpbm ASCII PPM (P3).

Only attributes that ImageMagick can actually write into the TIFF are emitted.
A tag-level audit showed that ImageMagick cannot write YCbCrSubSampling,
YCbCrPositioning, YCbCrCoefficients, ReferenceBlackWhite, or PlanarConfiguration
= 2; the variant cases that claimed those attributes were removed rather than
kept with a false claim. Tiling is the one variant attribute ImageMagick can
write, so the tiled fixture is retained.

Usage:

    python3 tests/ycbcr_images/generate_ycbcr.py
"""

import subprocess
import zlib
from pathlib import Path

BASE_W, BASE_H = 64, 48
IMAGE_DIR = Path(__file__).resolve().parent
REFERENCE_DIR = Path(__file__).resolve().parent.parent / "reference_images"
TMP_DIR = Path("/tmp/opencode")

# (name, tiled). Only these two combinations are truthfully writable by
# ImageMagick: full-resolution stripped YCbCr, and the same image with real
# TIFF tile tags. The variant cases (subsampling, positioning, coefficients,
# RefBW, planar=2) were removed after a tag-level audit because ImageMagick
# cannot write those tags.
CASES = [
    ("ycbcr-1x1", False),
    ("ycbcr-tiled", True),
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
    for name, tiled in CASES:
        w, h = BASE_W, BASE_H
        pixels = source_pixels(w, h, zlib.crc32(name.encode()) & 0xFFFFFFFF)
        src = TMP_DIR / f"{name}-src.ppm"
        out_tif = IMAGE_DIR / f"{name}.tif"
        out_ppm = REFERENCE_DIR / f"{name}.ppm"

        write_binary_ppm(src, w, h, pixels)

        args = [str(src), "-compress", "none", "-colorspace", "YCbCr"]
        if tiled:
            args += ["-define", "tiff:tile-geometry=64x48"]
        args += [str(out_tif)]
        run_magick(args)

        ref_bin = TMP_DIR / f"{name}-ref.ppm"
        run_magick([str(src), str(ref_bin)])
        write_ascii_ppm(out_ppm, w, h, pixels)


if __name__ == "__main__":
    main()
