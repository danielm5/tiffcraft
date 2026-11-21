#!/usr/bin/env python3
"""Deterministic generator for the YCbCr test-image matrix.

This script produces 17 YCbCr (``PhotometricInterpretation = 6``) TIFF
fixtures in ``tests/ycbcr_images/`` and one matching ground-truth P6 PPM
per fixture in ``tests/reference_images/``.

The 17-case matrix
------------------
* 9 subsampling fixtures (64x48): 1x1, 2x1, 1x2, 2x2, 4x1, 1x4, 4x2,
  2x4 and 4x4. These exercise real chroma subsampling written by
  ``rgb2ycbcr``; tags and pixel data agree, so they are conversion-verified.
* 5 variant fixtures (64x48): ``ycbcr-cosited`` (positioning = 2),
  ``ycbcr-custom-coeffs`` (BT.709 coefficients), ``ycbcr-custom-refbw``
  (headroom/footroom reference values), ``ycbcr-planar-2x2``
  (PlanarConfiguration = 2) and ``ycbcr-tiled-2x2`` (tiled organization).
  These are tag-reading-only fixtures: ``tiffset`` changes tag values
  without reorganizing the pixel data, so their pixels do not necessarily
  match the patched tags. They must not be used for conversion checks.
* 3 odd-dimension fixtures: ``ycbcr-odd-2x2`` (61x47),
  ``ycbcr-odd-4x4`` (63x61) and ``ycbcr-odd-mixed`` (62x47, 2x1).
  Tags and pixel data agree, so these are conversion-verified.

The tiled deviation
-------------------
``tiffcp`` refuses subsampled input, so ``ycbcr-tiled-2x2.tif`` is tiled
while still 1x1-subsampled; the ``YCbCrSubsampling`` tag is set to ``2 2``
only afterwards. ``tiffcp`` also drops the four YCbCr tags and resets
PhotometricInterpretation to RGB, so the TIFF tags are restored after
tiling. Its pixel data is therefore *not* subsampled: this fixture is a
tag-reading test only and is not usable for conversion verification.

Toolchain
---------
Only libtiff and netpbm command-line tools are used: ``pnmtotiff``,
``rgb2ycbcr``, ``tiffset``, ``tiffcp``, ``tiff2rgba`` and ``tifftopnm``.
Source PPMs are written as binary P6 with the standard library (``bytes``
plus ``zlib.crc32`` for the deterministic seed). There is no external
imaging-library dependency and no ImageMagick dependency. Ground-truth PPMs
come exclusively from ``tiff2rgba`` followed by ``tifftopnm``. libtiff's YCbCr
RGBA reader cannot decode 1x4 or 2x4 subsampling, so those two references are
rendered from the matching RGB source TIFF through that same pipeline.

Usage (from the repository root)::

    python3 tests/ycbcr_images/generate_ycbcr.py
"""

import subprocess
import zlib
from pathlib import Path

BASE_W, BASE_H = 64, 48

IMAGE_DIR = Path(__file__).resolve().parent
REFERENCE_DIR = Path(__file__).resolve().parent.parent / "reference_images"
TMP_DIR = Path("/tmp/opencode/ycbcr-gen")

# (name, horizontal subsampling, vertical subsampling) -- all 64x48.
SUBSAMPLING_CASES = [
    ("ycbcr-1x1", 1, 1),
    ("ycbcr-2x1", 2, 1),
    ("ycbcr-1x2", 1, 2),
    ("ycbcr-2x2", 2, 2),
    ("ycbcr-4x1", 4, 1),
    ("ycbcr-1x4", 1, 4),
    ("ycbcr-4x2", 4, 2),
    ("ycbcr-2x4", 2, 4),
    ("ycbcr-4x4", 4, 4),
]

# (name, width, height, horizontal subsampling, vertical subsampling).
ODD_CASES = [
    ("ycbcr-odd-2x2", 61, 47, 2, 2),
    ("ycbcr-odd-4x4", 63, 61, 4, 4),
    ("ycbcr-odd-mixed", 62, 47, 2, 1),
]

VARIANT_NAMES = [
    "ycbcr-cosited",
    "ycbcr-custom-coeffs",
    "ycbcr-custom-refbw",
    "ycbcr-planar-2x2",
    "ycbcr-tiled-2x2",
]

ALL_NAMES = (
    [name for name, _, _ in SUBSAMPLING_CASES]
    + [name for name, _, _, _, _ in ODD_CASES]
    + VARIANT_NAMES
)


def run(*args, stdin=None, stdout=None, stderr=None):
    """Run an external tool, aborting on a non-zero exit status."""
    subprocess.run([str(a) for a in args], check=True,
                   stdin=stdin, stdout=stdout, stderr=stderr)


def write_binary_ppm(path, w, h, pixels):
    """Write a binary (P6) Netpbm image with the standard library only."""
    with open(path, "wb") as f:
        f.write(f"P6\n{w} {h}\n255\n".encode("ascii"))
        for row in pixels:
            for (r, g, b) in row:
                f.write(bytes((r, g, b)))


def source_pixels(w, h, seed):
    """Deterministic RGB source pattern seeded by ``zlib.crc32``."""
    return [
        [
            (
                (x * 37 + y * 11 + seed * 53) % 256,
                (x * 7 + y * 29 + seed * 17) % 256,
                (x * 13 + y * 5 + seed * 31) % 256,
            )
            for x in range(w)
        ]
        for y in range(h)
    ]


def make_rgb_tiff(name, w, h):
    """Write a binary P6 source and convert it to an RGB TIFF."""
    ppm = TMP_DIR / f"{name}-src.ppm"
    tif = TMP_DIR / f"{name}-src.tif"
    seed = zlib.crc32(name.encode()) & 0xFFFFFFFF
    write_binary_ppm(ppm, w, h, source_pixels(w, h, seed))
    with open(ppm, "rb") as fin, open(tif, "wb") as fout:
        run("pnmtotiff", stdin=fin, stdout=fout,
            stderr=subprocess.DEVNULL)
    return tif


def make_ycbcr_tiff(name, rgb_tif, w, h, hs, vs, out_tif):
    """Encode an RGB TIFF as YCbCr with all four YCbCr tags written."""
    run("rgb2ycbcr", "-c", "none", "-h", hs, "-v", vs, "-r", h,
        rgb_tif, out_tif, stderr=subprocess.DEVNULL)
    return out_tif


def set_tag(path, tag, *values):
    """Set a TIFF tag with tiffset.

    RATIONAL values are passed with no explicit count; supplying one
    corrupts the first value.
    """
    run("tiffset", "-s", tag, *values, path)


def make_ground_truth(name, tif):
    """Produce the P6 PPM ground truth with tiff2rgba + tifftopnm."""
    rgba = TMP_DIR / f"{name}-rgba.tif"
    ppm = REFERENCE_DIR / f"{name}.ppm"
    run("tiff2rgba", "-n", "-c", "none", tif, rgba)
    with open(rgba, "rb") as fin, open(ppm, "wb") as fout:
        run("tifftopnm", stdin=fin, stdout=fout,
            stderr=subprocess.DEVNULL)


# libtiff's YCbCr RGBA reader has no 1x4 or 2x4 strip/tile routine, so
# tiff2rgba rejects those two YCbCr encodings. Their ground truth is
# rendered from the equivalent RGB source TIFF through the very same
# tiff2rgba + tifftopnm pipeline; that source is the exact image fed to
# rgb2ycbcr, so it is the correct expected RGB result.
UNSUPPORTED_FOR_TIFF2RGBA = {(1, 4), (2, 4)}


def remove_stale_fixtures():
    """Remove fixtures that are not part of the current 17-case matrix."""
    expected_tifs = {f"{name}.tif" for name in ALL_NAMES}
    expected_ppms = {f"{name}.ppm" for name in ALL_NAMES}
    for path in IMAGE_DIR.glob("ycbcr-*.tif"):
        if path.name not in expected_tifs:
            path.unlink()
    for path in REFERENCE_DIR.glob("ycbcr-*.ppm"):
        if path.name not in expected_ppms:
            path.unlink()


def main():
    TMP_DIR.mkdir(parents=True, exist_ok=True)
    IMAGE_DIR.mkdir(parents=True, exist_ok=True)
    REFERENCE_DIR.mkdir(parents=True, exist_ok=True)
    remove_stale_fixtures()

    # 9 full-resolution subsampling fixtures: conversion-verified.
    for name, hs, vs in SUBSAMPLING_CASES:
        rgb = make_rgb_tiff(name, BASE_W, BASE_H)
        out = IMAGE_DIR / f"{name}.tif"
        make_ycbcr_tiff(name, rgb, BASE_W, BASE_H, hs, vs, out)
        source = rgb if (hs, vs) in UNSUPPORTED_FOR_TIFF2RGBA else out
        make_ground_truth(name, source)

    # 3 odd-dimension fixtures: conversion-verified.
    for name, w, h, hs, vs in ODD_CASES:
        rgb = make_rgb_tiff(name, w, h)
        out = IMAGE_DIR / f"{name}.tif"
        make_ycbcr_tiff(name, rgb, w, h, hs, vs, out)
        make_ground_truth(name, out)

    # 5 variants: tag-reading only (see module docstring).
    # cosited -- patch positioning on a 2x2 base.
    name = "ycbcr-cosited"
    rgb = make_rgb_tiff(name, BASE_W, BASE_H)
    out = IMAGE_DIR / f"{name}.tif"
    make_ycbcr_tiff(name, rgb, BASE_W, BASE_H, 2, 2, out)
    set_tag(out, "YCbCrPositioning", 2)
    make_ground_truth(name, out)

    # custom coefficients -- patch tag 529 on a 1x1 base.
    name = "ycbcr-custom-coeffs"
    rgb = make_rgb_tiff(name, BASE_W, BASE_H)
    out = IMAGE_DIR / f"{name}.tif"
    make_ycbcr_tiff(name, rgb, BASE_W, BASE_H, 1, 1, out)
    set_tag(out, "YCbCrCoefficients", 0.2126, 0.7152, 0.0722)
    make_ground_truth(name, out)

    # custom reference black/white -- patch tag 532 on a 1x1 base.
    name = "ycbcr-custom-refbw"
    rgb = make_rgb_tiff(name, BASE_W, BASE_H)
    out = IMAGE_DIR / f"{name}.tif"
    make_ycbcr_tiff(name, rgb, BASE_W, BASE_H, 1, 1, out)
    set_tag(out, "ReferenceBlackWhite", 15, 235, 128, 240, 128, 240)
    make_ground_truth(name, out)

    # separate planar configuration -- patch tag 284 without moving pixels.
    # The ground truth is rendered from the chunky TIFF before the patch.
    name = "ycbcr-planar-2x2"
    rgb = make_rgb_tiff(name, BASE_W, BASE_H)
    out = IMAGE_DIR / f"{name}.tif"
    make_ycbcr_tiff(name, rgb, BASE_W, BASE_H, 2, 2, out)
    make_ground_truth(name, out)
    set_tag(out, "PlanarConfiguration", 2)

    # tiled -- tiffcp refuses subsampled input and drops the YCbCr tags.
    # Tile a 1x1 image, restore PI=6 and all four tags, then patch 530.
    # The ground truth is rendered from the 1x1 stripped TIFF before tiling.
    name = "ycbcr-tiled-2x2"
    rgb = make_rgb_tiff(name, BASE_W, BASE_H)
    stripped = TMP_DIR / f"{name}-1x1.tif"
    out = IMAGE_DIR / f"{name}.tif"
    make_ycbcr_tiff(name, rgb, BASE_W, BASE_H, 1, 1, stripped)
    make_ground_truth(name, stripped)
    run("tiffcp", "-t", "-w", "16", "-l", "16", "-c", "none",
        stripped, out, stderr=subprocess.DEVNULL)
    set_tag(out, "PhotometricInterpretation", 6)
    set_tag(out, "YCbCrCoefficients", 0.299, 0.587, 0.114)
    set_tag(out, "YCbCrPositioning", 1)
    set_tag(out, "ReferenceBlackWhite", 0, 255, 128, 255, 128, 255)
    # libtiff's field name is "YCbCrSubsampling" (lowercase 's').
    set_tag(out, "YCbCrSubsampling", 2, 2)


if __name__ == "__main__":
    main()
