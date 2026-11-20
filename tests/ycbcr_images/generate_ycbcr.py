#!/usr/bin/env python3
"""Deterministic generator for the three retained YCbCr
(PhotometricInterpretation = 6) test fixtures.

Generates three 64x48, 1x1-subsampled fixtures (all other YCbCr tags at their
TIFF defaults):

* ``tests/ycbcr_images/`` - one full-resolution stripped YCbCr TIFF, one tiled
  YCbCr TIFF with a single 64x48 tile, and one tiled YCbCr TIFF with a 4x3 grid
  of 16x16 tiles (12 tiles)
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
write, so two tiled fixtures (single-tile and multi-tile) are retained.

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

# (name, tile_geometry). None means stripped (no tile define); a string is
# passed as -define tiff:tile-geometry=<value>. Only these three combinations
# are truthfully writable by ImageMagick: full-resolution stripped YCbCr, the
# same image with a single 64x48 tile, and the same image with a 4x3 grid of
# 16x16 tiles (12 tiles). The variant cases (subsampling, positioning,
# coefficients, RefBW, planar=2) were removed after a tag-level audit because
# ImageMagick cannot write those tags.
CASES = [
    ("ycbcr-1x1", None),
    ("ycbcr-tiled-single", "64x48"),
    ("ycbcr-tiled-multi", "16x16"),
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
    for name, tile_geometry in CASES:
        w, h = BASE_W, BASE_H
        pixels = source_pixels(w, h, zlib.crc32(name.encode()) & 0xFFFFFFFF)
        src = TMP_DIR / f"{name}-src.ppm"
        out_tif = IMAGE_DIR / f"{name}.tif"
        out_ppm = REFERENCE_DIR / f"{name}.ppm"

        write_binary_ppm(src, w, h, pixels)

        args = [str(src), "-compress", "none", "-colorspace", "YCbCr"]
        if tile_geometry is not None:
            args += ["-define", f"tiff:tile-geometry={tile_geometry}"]
        args += [str(out_tif)]
        run_magick(args)

        ref_bin = TMP_DIR / f"{name}-ref.ppm"
        run_magick([str(src), str(ref_bin)])
        write_ascii_ppm(out_ppm, w, h, pixels)


if __name__ == "__main__":
    main()
