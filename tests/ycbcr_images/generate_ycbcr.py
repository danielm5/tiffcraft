#!/usr/bin/env python3
"""Deterministic generator for the YCbCr test-image matrix.

This script produces 18 YCbCr (``PhotometricInterpretation = 6``) TIFF
fixtures in ``tests/ycbcr_images/`` and one matching ground-truth P6 PPM
per fixture in ``tests/reference_images/``.

The 18-case matrix
------------------
* 9 subsampling fixtures (64x48): 1x1, 2x1, 1x2, 2x2, 4x1, 1x4, 4x2,
  2x4 and 4x4. These exercise real chroma subsampling written by
  ``rgb2ycbcr``; tags and pixel data agree, so they are conversion-verified.
* 6 variant fixtures (64x48): ``ycbcr-cosited`` (positioning = 2),
  ``ycbcr-custom-coeffs`` (BT.709 coefficients), ``ycbcr-custom-refbw``
  (headroom/footroom reference values), ``ycbcr-planar-1x1`` (genuine
  separate planes on a 1x1 base), ``ycbcr-planar-2x2`` (genuine separate
  planes on a 2x2 base) and ``ycbcr-tiled-2x2`` (tiled organization).
  ``ycbcr-cosited``, ``ycbcr-custom-coeffs``, ``ycbcr-custom-refbw`` and
  ``ycbcr-tiled-2x2`` are tag-reading-only fixtures: ``tiffset`` changes tag
  values without reorganizing the pixel data. The two planar fixtures hold
  genuine separate planes (Y, Cb and Cr each in their own strips) and are
  conversion-verified.
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

The planar arrangements
-----------------------
``ycbcr-planar-1x1.tif`` is converted with ``tiffcp -p separate``, so its
Y, Cb and Cr samples really do occupy separate strips (six strips in total,
two per plane). Because ``tiffcp`` drops the four YCbCr tags and resets
PhotometricInterpretation to RGB, those five tags are restored afterwards;
its ground truth is rendered from the final planar TIFF.

``ycbcr-planar-2x2.tif`` is rewritten from the equivalent chunky 2x2 TIFF
by ``write_planar_subsampled_tiff``: the three plane segments of the single
chunky strip become three separate strips (one per plane), with
PlanarConfiguration = 2 and internally consistent strip arrays. Both planar
fixtures are therefore genuine separate-plane conversion tests. libtiff's
``tiff2rgba`` cannot decode planar + subsampled YCbCr ("Sorry, can not
handle image"), so the ground truth for the 2x2 planar fixture is rendered
from the equivalent chunky 2x2 YCbCr TIFF through the same ``tiff2rgba`` +
``tifftopnm`` pipeline used for every other fixture.

Automatic structural validation
-------------------------------
At the end of every run ``validate_fixture`` checks all 18 emitted TIFFs: it
parses the first IFD with ``struct``, verifies the YCbCr tag set and values,
checks that strip/tile array lengths and total strip byte counts match the
image geometry (this catches inconsistent planar strip arrays), and fails on
any ``tiffinfo`` warning or error.

Toolchain
---------
Only libtiff and netpbm command-line tools are used: ``pnmtotiff``,
``rgb2ycbcr``, ``tiffset``, ``tiffcp``, ``tiff2rgba`` and ``tifftopnm``.
Source PPMs are written as binary P6 with the standard library (``bytes``
plus ``zlib.crc32`` for the deterministic seed), and the planar rewrite and
structural validation use only ``struct`` and ``pathlib``. Ground-truth PPMs
come exclusively from ``tiff2rgba`` followed by ``tifftopnm``. libtiff's YCbCr
RGBA reader cannot decode 1x4 or 2x4 subsampling, so those two references are
rendered from the matching RGB source TIFF through that same pipeline.

Usage (from the repository root)::

    python3 tests/ycbcr_images/generate_ycbcr.py
"""

import struct
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
    "ycbcr-planar-1x1",
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


# --- Pure-Python IFD parsing, planar rewrite and structural validation -------

_TIFF_TYPE_SIZE = {
    1: 1, 2: 1, 3: 2, 4: 4, 5: 8, 6: 1, 7: 1,
    8: 2, 9: 4, 10: 8, 11: 4, 12: 8,
}
_TIFF_SHORT, _TIFF_LONG, _TIFF_RATIONAL = 3, 4, 5


def parse_tiff(path):
    """Parse the first IFD of a classic TIFF into ``{tag: (type, values)}``.

    SHORT and LONG values become ints; RATIONAL values become ``(num, den)``
    pairs. Only the standard library (``struct``, ``pathlib``) is used.
    """
    data = Path(path).read_bytes()
    if data[:4] == b"II*\x00":
        endian = "<"
    elif data[:4] == b"MM\x00*":
        endian = ">"
    else:
        raise ValueError(f"{Path(path).name}: not a classic TIFF")
    ifd_off = struct.unpack_from(endian + "I", data, 4)[0]
    n = struct.unpack_from(endian + "H", data, ifd_off)[0]
    tags = {}
    for i in range(n):
        pos = ifd_off + 2 + 12 * i
        tag, typ, count = struct.unpack_from(endian + "HHI", data, pos)
        size = _TIFF_TYPE_SIZE.get(typ)
        if size is None:
            raise ValueError(f"{Path(path).name}: unsupported TIFF type {typ}")
        nbytes = count * size
        if nbytes <= 4:
            block = data[pos + 8:pos + 8 + nbytes]
        else:
            val_off = struct.unpack_from(endian + "I", data, pos + 8)[0]
            block = data[val_off:val_off + nbytes]
        if len(block) != nbytes:
            raise ValueError(f"{Path(path).name}: truncated value for tag {tag}")
        if typ == _TIFF_SHORT:
            values = list(struct.unpack(endian + f"{count}H", block))
        elif typ == _TIFF_LONG:
            values = list(struct.unpack(endian + f"{count}I", block))
        elif typ == _TIFF_RATIONAL:
            values = [struct.unpack_from(endian + "II", block, 8 * j)
                      for j in range(count)]
        else:
            values = []
        tags[tag] = (typ, values)
    return tags


def _encode_values(values, typ):
    """Encode tag values as a little-endian TIFF payload."""
    if typ == _TIFF_SHORT:
        return struct.pack(f"<{len(values)}H", *values)
    if typ == _TIFF_LONG:
        return struct.pack(f"<{len(values)}I", *values)
    if typ == _TIFF_RATIONAL:
        return b"".join(struct.pack("<II", num, den) for num, den in values)
    raise ValueError(f"unsupported TIFF type {typ}")


def write_planar_subsampled_tiff(chunky_tif, out_tif, hs, vs, w, h):
    """Rewrite a chunky subsampled YCbCr TIFF as a genuine three-plane TIFF.

    ``rgb2ycbcr`` writes the Y, Cb and Cr plane segments back-to-back inside
    one strip. This helper reads those bytes using the source TIFF's own
    StripOffsets/StripByteCounts, gives each plane its own strip, and writes
    a new classic little-endian TIFF with PlanarConfiguration = 2 and
    matching strip arrays. YCbCr tags are copied from the source (BT.601
    defaults are used as a fallback). No hard-coded offsets, no external
    imaging library.
    """
    chunky_tif = Path(chunky_tif)
    out_tif = Path(out_tif)
    src = parse_tiff(chunky_tif)

    def one(tag):
        values = src[tag][1]
        if len(values) != 1:
            raise ValueError(
                f"{chunky_tif.name}: tag {tag} has {len(values)} values")
        return values[0]

    if (one(256), one(257)) != (w, h):
        raise ValueError(f"{chunky_tif.name}: dimensions differ from {w}x{h}")
    src_hs, src_vs = src[530][1]
    if (src_hs, src_vs) != (hs, vs):
        raise ValueError(
            f"{chunky_tif.name}: subsampling differs from {hs}x{vs}")

    offsets, counts = src[273][1], src[279][1]
    if not offsets or len(offsets) != len(counts):
        raise ValueError(f"{chunky_tif.name}: inconsistent strip arrays")
    data = chunky_tif.read_bytes()
    pixels = b"".join(data[off:off + count]
                      for off, count in sorted(zip(offsets, counts)))

    y_size = w * h
    chroma_w = (w + hs - 1) // hs
    chroma_h = (h + vs - 1) // vs
    c_size = chroma_w * chroma_h
    if len(pixels) != y_size + 2 * c_size:
        raise ValueError(
            f"{chunky_tif.name}: strip data is {len(pixels)} bytes, expected "
            f"{y_size + 2 * c_size} for {w}x{h} {hs}x{vs}")
    y_plane = pixels[:y_size]
    cb_plane = pixels[y_size:y_size + c_size]
    cr_plane = pixels[y_size + c_size:]

    coeffs = (src[529][1] if 529 in src else
              [(299, 1000), (587, 1000), (114, 1000)])
    refbw = (src[532][1] if 532 in src else
             [(0, 1), (255, 1), (128, 1), (255, 1), (128, 1), (255, 1)])
    positioning = one(531) if 531 in src else 1
    bits = src[258][1] if 258 in src else [8, 8, 8]
    compression = one(259) if 259 in src else 1
    photometric = one(262) if 262 in src else 6
    fill_order = src[266][1][0] if 266 in src else 1
    spp = one(277) if 277 in src else 3

    specs = [
        (256, _TIFF_SHORT, [w]),
        (257, _TIFF_SHORT, [h]),
        (258, _TIFF_SHORT, bits),
        (259, _TIFF_SHORT, [compression]),
        (262, _TIFF_SHORT, [photometric]),
        (266, _TIFF_SHORT, [fill_order]),
        (273, _TIFF_LONG, None),          # StripOffsets, patched below
        (277, _TIFF_SHORT, [spp]),
        (278, _TIFF_SHORT, [h]),          # one strip per plane
        (279, _TIFF_LONG, None),          # StripByteCounts, patched below
        (284, _TIFF_SHORT, [2]),          # PlanarConfiguration = separate
        (529, _TIFF_RATIONAL, coeffs),
        (530, _TIFF_SHORT, [hs, vs]),
        (531, _TIFF_SHORT, [positioning]),
        (532, _TIFF_RATIONAL, refbw),
    ]
    specs.sort(key=lambda spec: spec[0])
    n = len(specs)
    ext_off = 8 + 2 + n * 12 + 4

    # Placeholder strip arrays only determine payload sizes (3 LONGs each).
    placeholder = [(tag, typ, values if values is not None else [0, 0, 0])
                   for tag, typ, values in specs]
    encoded = [(tag, typ, len(values), _encode_values(values, typ))
               for tag, typ, values in placeholder]
    ext_sizes = {tag: len(payload) + (len(payload) % 2)
                 for tag, _, _, payload in encoded if len(payload) > 4}
    pix_off = ext_off + sum(ext_sizes.values())
    strips = [pix_off, pix_off + y_size, pix_off + y_size + c_size]
    strip_counts = [y_size, c_size, c_size]
    specs = [(tag, typ,
              strips if tag == 273 else strip_counts if tag == 279 else values)
             for tag, typ, values in specs]
    encoded = [(tag, typ, len(values), _encode_values(values, typ))
               for tag, typ, values in specs]

    ext_offsets = {}
    cursor = ext_off
    for tag, _, _, payload in encoded:
        if tag in ext_sizes:
            ext_offsets[tag] = cursor
            cursor += ext_sizes[tag]
    if cursor != pix_off:
        raise AssertionError("external payload layout mismatch")

    out = bytearray(b"II*\x00" + struct.pack("<I", 8) + struct.pack("<H", n))
    for tag, typ, count, payload in encoded:
        out += struct.pack("<HHI", tag, typ, count)
        if len(payload) <= 4:
            out += payload.ljust(4, b"\0")
        else:
            out += struct.pack("<I", ext_offsets[tag])
    out += struct.pack("<I", 0)              # no next IFD
    for tag, _, _, payload in encoded:
        if tag in ext_offsets:
            out += payload + (b"\0" if len(payload) % 2 else b"")
    out += y_plane + cb_plane + cr_plane
    if len(out) != pix_off + y_size + 2 * c_size:
        raise AssertionError(f"{out_tif.name}: unexpected output size {len(out)}")
    out_tif.write_bytes(out)


def validate_fixture(path):
    """Structurally validate one emitted YCbCr TIFF fixture.

    Raises a ``ValueError`` naming the fixture on any defect. The strip-array
    check is what catches a PlanarConfiguration = 2 tag patched onto chunky
    data with too few strip offsets/counts.
    """
    path = Path(path)
    tags = parse_tiff(path)

    def need(tag):
        if tag not in tags:
            raise ValueError(f"{path.name}: missing required tag {tag}")
        return tags[tag][1]

    def one(tag):
        values = need(tag)
        if len(values) != 1:
            raise ValueError(
                f"{path.name}: tag {tag} must have 1 value, got {len(values)}")
        return values[0]

    is_tiled = 322 in tags
    required = [256, 257, 258, 259, 262, 277, 278, 284, 529, 530, 531, 532]
    for tag in required:
        # Tiled TIFFs legitimately omit RowsPerStrip (tag 278).
        if is_tiled and tag == 278:
            continue
        need(tag)

    w, h = one(256), one(257)
    if one(262) != 6:
        raise ValueError(f"{path.name}: PhotometricInterpretation is not 6")
    if one(277) != 3:
        raise ValueError(f"{path.name}: SamplesPerPixel is not 3")
    if need(258) != [8, 8, 8]:
        raise ValueError(f"{path.name}: BitsPerSample is not [8, 8, 8]")
    if one(259) != 1:
        raise ValueError(f"{path.name}: Compression is not 1")
    hs, vs = need(530)
    if hs not in (1, 2, 4) or vs not in (1, 2, 4):
        raise ValueError(f"{path.name}: invalid YCbCrSubsampling {hs}x{vs}")
    positioning = one(531)
    if positioning not in (1, 2):
        raise ValueError(f"{path.name}: invalid YCbCrPositioning {positioning}")
    planar = one(284)
    if planar not in (1, 2):
        raise ValueError(f"{path.name}: invalid PlanarConfiguration {planar}")
    coeffs = need(529)
    if len(coeffs) != 3 or any(den <= 0 for _, den in coeffs):
        raise ValueError(f"{path.name}: invalid YCbCrCoefficients")
    refbw = need(532)
    if len(refbw) != 6 or any(den <= 0 for _, den in refbw):
        raise ValueError(f"{path.name}: invalid ReferenceBlackWhite")

    if is_tiled:
        for tag in (322, 323, 324, 325):
            need(tag)
        for tag in (273, 279):
            if tag in tags:
                raise ValueError(f"{path.name}: tiled image has strip tag {tag}")
        tw, th = one(322), one(323)
        offsets, counts = need(324), need(325)
        expected = ((w + tw - 1) // tw) * ((h + th - 1) // th)
        if len(offsets) != len(counts) or len(offsets) != expected:
            raise ValueError(
                f"{path.name}: tile arrays have {len(offsets)}/{len(counts)} "
                f"entries, expected {expected}")
    else:
        rps = one(278)
        offsets, counts = need(273), need(279)
        if planar == 1:
            expected_strips = (h + rps - 1) // rps
        else:
            chroma_rows = (h + vs - 1) // vs
            expected_strips = ((h + rps - 1) // rps
                               + 2 * ((chroma_rows + rps - 1) // rps))
        if len(offsets) != len(counts) or len(offsets) != expected_strips:
            raise ValueError(
                f"{path.name}: strip arrays have {len(offsets)}/{len(counts)} "
                f"entries, expected {expected_strips}")
        # libtiff pads luma dimensions up to multiples of the subsampling
        # factors when W/H are not evenly divisible (the odd fixtures).
        pad_w = ((w + hs - 1) // hs) * hs
        pad_h = ((h + vs - 1) // vs) * vs
        expected_bytes = pad_w * pad_h + 2 * (pad_w // hs) * (pad_h // vs)
        if sum(counts) != expected_bytes:
            raise ValueError(
                f"{path.name}: strip byte total {sum(counts)}, "
                f"expected {expected_bytes}")

    proc = subprocess.run(["tiffinfo", str(path)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, universal_newlines=True)
    if proc.returncode != 0:
        raise ValueError(f"{path.name}: tiffinfo exited {proc.returncode}")
    if "Warning" in proc.stderr or "Error" in proc.stderr:
        raise ValueError(f"{path.name}: tiffinfo reported: {proc.stderr.strip()}")


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
    """Remove fixtures that are not part of the current 18-case matrix."""
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

    # 6 variants: four tag-reading only, two genuine planar conversion tests.
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

    # genuine separate planes on a 1x1 base -- tiffcp -p separate reorganizes
    # the pixels into Y/Cb/Cr strips; tiffcp drops the YCbCr tags and resets
    # PhotometricInterpretation to RGB, so all five tags are restored after.
    # The ground truth is rendered from the FINAL planar TIFF.
    name = "ycbcr-planar-1x1"
    rgb = make_rgb_tiff(name, BASE_W, BASE_H)
    stripped = TMP_DIR / f"{name}-1x1.tif"
    out = IMAGE_DIR / f"{name}.tif"
    make_ycbcr_tiff(name, rgb, BASE_W, BASE_H, 1, 1, stripped)
    run("tiffcp", "-p", "separate", "-c", "none", stripped, out,
        stderr=subprocess.DEVNULL)
    set_tag(out, "PhotometricInterpretation", 6)
    set_tag(out, "YCbCrCoefficients", 0.299, 0.587, 0.114)
    set_tag(out, "YCbCrPositioning", 1)
    set_tag(out, "ReferenceBlackWhite", 0, 255, 128, 255, 128, 255)
    set_tag(out, "YCbCrSubsampling", 1, 1)
    make_ground_truth(name, out)

    # genuine separate planes on a 2x2 base -- rgb2ycbcr writes the Y, Cb
    # and Cr segments into one chunky strip; the pure-Python writer gives
    # each plane its own strip. Ground truth comes from the chunky TIFF
    # because tiff2rgba cannot decode planar + subsampled YCbCr.
    name = "ycbcr-planar-2x2"
    rgb = make_rgb_tiff(name, BASE_W, BASE_H)
    chunky = TMP_DIR / f"{name}-chunky.tif"
    out = IMAGE_DIR / f"{name}.tif"
    make_ycbcr_tiff(name, rgb, BASE_W, BASE_H, 2, 2, chunky)
    make_ground_truth(name, chunky)
    write_planar_subsampled_tiff(chunky, out, 2, 2, BASE_W, BASE_H)

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

    # Validate every emitted TIFF. This catches inconsistent strip arrays,
    # such as a planar tag patched onto chunky pixel data.
    if len(list(IMAGE_DIR.glob("ycbcr-*.tif"))) != len(ALL_NAMES):
        raise ValueError(f"expected exactly {len(ALL_NAMES)} YCbCr TIFFs")
    for name in ALL_NAMES:
        validate_fixture(IMAGE_DIR / f"{name}.tif")
    print(f"Generated and validated {len(ALL_NAMES)} YCbCr TIFF fixtures.")


if __name__ == "__main__":
    main()
