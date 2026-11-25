#!/usr/bin/env python3
"""Deterministic generator for the YCbCr test-image matrix.

This script produces 17 YCbCr (``PhotometricInterpretation = 6``) TIFF
fixtures in ``tests/ycbcr_images/`` and one matching ground-truth P6 PPM
per fixture in ``tests/reference_images/``.

The 17-case matrix
------------------
* 9 subsampling fixtures (64x48): 1x1, 2x1, 1x2, 2x2, 4x1, 1x4, 4x2,
  2x4 and 4x4. These exercise real chroma subsampling written by
  ``rgb2ycbcr``; tags and pixel data agree.
* 5 variant fixtures: ``ycbcr-custom-coeffs`` (BT.709 luma coefficients,
  64x48), ``ycbcr-custom-refbw`` (headroom/footroom reference values, 64x48),
  ``ycbcr-planar-1x1`` (61x47, genuine separate planes on a 1x1 base),
  ``ycbcr-planar-2x2`` (63x61, genuine separate planes on a 2x2 base) and
  ``ycbcr-tiled-2x2`` (64x48, genuine chunky tiles on a 2x2 base).
* 3 odd-dimension fixtures: ``ycbcr-odd-2x2`` (61x47),
  ``ycbcr-odd-4x4`` (63x61) and ``ycbcr-odd-mixed`` (62x47, 2x1).
  Tags and pixel data agree.

The script also renders two references for the ``tests/libtiff-pics/dscf0013.tif``
fixture listed in ``TEST_IMAGES.md``: ``dscf0013.ppm`` for the default 640x480
IFD (what ``TiffImage::read`` opens) and ``dscf0013-ifd1.ppm`` for the 160x120
thumbnail. ``tiffsplit`` separates the IFDs and libtiff's ``tiff2rgba`` and
``tifftopnm`` decode them, so both references follow the same libtiff-only
provenance rule as the generated fixtures.

There is no co-sited (``YCbCrPositioning = 2``) fixture. libtiff 4.7.2 stores
the tag but never consults it while decoding: ``tiff2rgba`` output for the
same TIFF is byte-for-byte identical with positioning 1 and 2. Such a fixture
could only test tag reading rather than a real conversion, so it was removed.

Non-default tag fixtures
------------------------
``ycbcr-custom-coeffs`` and ``ycbcr-custom-refbw`` are built by
``encode_ycbcr_codes`` and ``write_stripped_ycbcr_tiff``: the forward
YCbCr transform (the inverse of libtiff's ``TIFFYCbCrtoRGB``) is applied to
the deterministic source pixels using exactly the values written into tags
529 and 532. Coding ranges are 255 for Y and 127 for Cb/Cr, matching
libtiff's ``Code2V``. The writer emits a valid stripped chunky 1x1 TIFF with
PI = 6 and all four YCbCr tags, so no tag is patched on afterwards.
``verify_ground_truth`` checks the libtiff-decoded PPM against the source
pixels (mean absolute per-channel difference well below one code value).

The tiled fixture
-----------------
``ycbcr-tiled-2x2.tif`` is a genuine subsampled tiled conversion fixture.
``tiffcp`` refuses subsampled input, so a pure-Python ``struct``-based TIFF
tile writer (``write_tiled_ycbcr_tiff``) lays the cell-interleaved YCbCr strip
of the stripped 2x2 TIFF out as 16x16 luma tiles in a 4x3 grid (12 tiles of
384 bytes). All four YCbCr tags, PI = 6 and PlanarConfig = 1 are written into
the new TIFF itself, so no tag is patched on afterwards. Its ground truth is
rendered with ``tiff2rgba`` + ``tifftopnm`` from that exact tiled TIFF and is
byte-identical to the ``ycbcr-2x2.ppm`` reference.

The planar arrangements
-----------------------
Both planar fixtures use odd, non-power-of-two dimensions and
RowsPerStrip = 16, so every plane holds multiple strips.

``ycbcr-planar-1x1.tif`` (61x47) is converted with ``tiffcp -p separate -r 16``,
so its Y, Cb and Cr samples really do occupy separate strips: nine strips in
total, three per plane (16, 16 and 15 rows). Because ``tiffcp`` drops the
four YCbCr tags and resets PhotometricInterpretation to RGB, those five tags
are restored afterwards; its ground truth is rendered from the final planar
TIFF with ``tiff2rgba`` + ``tifftopnm``.

``ycbcr-planar-2x2.tif`` (63x61) is rewritten by
``write_planar_subsampled_tiff``: the cell-interleaved strip of the chunky 2x2
TIFF is de-interleaved into a full-width Y plane and two chroma planes whose
rows use libtiff's full-width planar scanline stride. With RowsPerStrip = 16
libtiff counts four strips per plane (the chroma planes are counted with the
luma RowsPerStrip too), giving 12 strips: 4 Y + 4 Cb + 4 Cr, each chroma strip
holding RowsPerStrip/2 = 8 chroma rows. PlanarConfiguration = 2, consistent
strip arrays and all four YCbCr tags are written into the TIFF itself.
libtiff's ``tiff2rgba`` cannot decode planar + subsampled YCbCr ("Sorry, can
not handle image"), so its ground truth comes from libtiff's
``TIFFReadScanline`` decoding of that exact planar TIFF through the in-repo
C helper, never from an equivalent chunky image.

Ground-truth provenance
-----------------------
Every reference PPM is produced by libtiff decoding the SAME TIFF fixture:
14 via ``tiff2rgba`` + ``tifftopnm``, and 3 (``ycbcr-1x4``, ``ycbcr-2x4`` and
``ycbcr-planar-2x2``) via the libtiff ``TIFFReadScanline`` C helper, whose
raw YCbCr bytes are converted to RGB with the TIFF's own
``YCbCrCoefficients`` and ``ReferenceBlackWhite`` values. libtiff's
``TIFFRGBAImage`` path rejects 4x vertical chroma subsampling and planar +
subsampled YCbCr even though ``TIFFReadScanline`` decodes those pixels.

Automatic structural validation
-------------------------------
At the end of every run ``validate_fixture`` checks all 17 emitted TIFFs: it
parses the first IFD with ``struct``, verifies the YCbCr tag set and values,
checks that strip/tile array lengths and per-strip byte counts match the image
geometry (this checks every planar strip and validates the 12-tile layout),
and fails on any ``tiffinfo`` warning or error.

Toolchain
---------
libtiff and netpbm command-line tools are used: ``pnmtotiff``, ``rgb2ycbcr``,
``tiffset``, ``tiffcp``, ``tiff2rgba`` and ``tifftopnm``. Source PPMs are
written as binary P6 with the standard library (``bytes`` plus ``zlib.crc32``
for the deterministic seed); the pure-Python encoder/writer, the planar and
tiled writers and the structural validation use ``struct``, ``fractions``,
``math`` and ``pathlib``. The small C helper ``ycbcr_scanline_dump.c`` is
compiled with gcc at generation time.

Usage (from the repository root)::

    python3 tests/ycbcr_images/generate_ycbcr.py
"""

import math
import shutil
import struct
import subprocess
import zlib
from fractions import Fraction
from pathlib import Path

BASE_W, BASE_H = 64, 48
PLANAR_1X1_W, PLANAR_1X1_H = 61, 47
PLANAR_2X2_W, PLANAR_2X2_H = 63, 61
PLANAR_RPS = 16

IMAGE_DIR = Path(__file__).resolve().parent
REFERENCE_DIR = Path(__file__).resolve().parent.parent / "reference_images"
TMP_DIR = Path("/tmp/opencode/ycbcr-gen")
SCANLINE_DUMP_SRC = IMAGE_DIR / "ycbcr_scanline_dump.c"
SCANLINE_DUMP_BIN = TMP_DIR / "ycbcr_scanline_dump"

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


def write_planar_subsampled_tiff(chunky_tif, out_tif, hs, vs, w, h, rps):
    """Rewrite a chunky subsampled YCbCr TIFF as a genuine three-plane TIFF.

    ``rgb2ycbcr`` stores subsampled YCbCr as ``hs*vs`` luma samples followed
    by one Cb and one Cr sample per chroma cell (cell-interleaved, not
    plane-interleaved). This helper reads the strip bytes using the source
    TIFF's own StripOffsets/StripByteCounts, de-interleaves the cells into a
    full-width Y plane and two chroma planes, and writes a new classic
    little-endian TIFF with PlanarConfiguration = 2 and matching strip arrays.

    libtiff's planar scanline size is the full image width for *every* plane,
    so each chroma row is stored in ``w`` bytes (the first ``ceil(w/hs)`` are
    chroma samples; the tail is edge-replicated padding). libtiff maps a
    scanline to strip ``row / RowsPerStrip`` in every plane, so a chroma strip
    holds ``rps`` full-width scanlines rather than ``rps/vs`` chroma rows;
    trailing chroma scanlines past ``ceil(h/vs)`` are padded. libtiff also
    counts strips per plane from the luma RowsPerStrip, so all three planes
    get ``ceil(h/rps)`` strips. YCbCr tags are copied from the source (BT.601
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
    if rps <= 0 or rps % vs:
        raise ValueError("RowsPerStrip must be a positive multiple of vertical subsampling")

    offsets, counts = src[273][1], src[279][1]
    if not offsets or len(offsets) != len(counts):
        raise ValueError(f"{chunky_tif.name}: inconsistent strip arrays")
    data = chunky_tif.read_bytes()
    pixels = b"".join(data[off:off + count]
                      for off, count in sorted(zip(offsets, counts)))

    chroma_w = (w + hs - 1) // hs
    chroma_h = (h + vs - 1) // vs
    strips_per_plane = (h + rps - 1) // rps
    chroma_scanlines = strips_per_plane * rps
    cell_size = hs * vs + 2
    if len(pixels) != chroma_w * chroma_h * cell_size:
        raise ValueError(
            f"{chunky_tif.name}: strip data is {len(pixels)} bytes, expected "
            f"{chroma_w * chroma_h * cell_size} for {w}x{h} {hs}x{vs}")
    y_plane = bytearray(w * h)
    cb_plane = bytearray(w * chroma_scanlines)
    cr_plane = bytearray(w * chroma_scanlines)
    for cy in range(chroma_h):
        for cx in range(chroma_w):
            cell = pixels[(cy * chroma_w + cx) * cell_size:
                          (cy * chroma_w + cx + 1) * cell_size]
            for j in range(hs * vs):
                yy, xx = cy * vs + j // hs, cx * hs + j % hs
                if yy < h and xx < w:       # odd-sized edge cell
                    y_plane[yy * w + xx] = cell[j]
            cb_plane[cy * w + cx] = cell[hs * vs]
            cr_plane[cy * w + cx] = cell[hs * vs + 1]
    for cy in range(chroma_scanlines):     # pad every chroma scanline to width w
        base = cy * w
        cb_plane[base + chroma_w:(cy + 1) * w] = (
            bytes([cb_plane[base + chroma_w - 1]]) * (w - chroma_w))
        cr_plane[base + chroma_w:(cy + 1) * w] = (
            bytes([cr_plane[base + chroma_w - 1]]) * (w - chroma_w))

    def plane_strips(plane, rows, rows_per_strip):
        chunks = []
        for i in range(strips_per_plane):
            first = i * rows_per_strip
            nrows = max(0, min(rows_per_strip, rows - first))
            chunks.append(bytes(plane[first * w:(first + nrows) * w]))
        return chunks

    strip_blobs = (plane_strips(y_plane, h, rps)
                   + plane_strips(cb_plane, chroma_scanlines, rps)
                   + plane_strips(cr_plane, chroma_scanlines, rps))
    strip_counts = [len(blob) for blob in strip_blobs]
    nstrips = len(strip_blobs)

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
        (278, _TIFF_SHORT, [rps]),
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

    # Placeholder strip arrays only determine the external payload sizes.
    placeholder = [(tag, typ, values if values is not None else [0] * nstrips)
                   for tag, typ, values in specs]
    encoded = [(tag, typ, len(values), _encode_values(values, typ))
               for tag, typ, values in placeholder]
    ext_sizes = {tag: len(payload) + (len(payload) % 2)
                 for tag, _, _, payload in encoded if len(payload) > 4}
    pix_off = ext_off + sum(ext_sizes.values())
    strip_offsets = []
    cursor = pix_off
    for blob in strip_blobs:
        strip_offsets.append(cursor)
        cursor += len(blob)
    specs = [(tag, typ,
              strip_offsets if tag == 273 else
              strip_counts if tag == 279 else values)
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
    for blob in strip_blobs:
        out += blob
    if len(out) != pix_off + sum(strip_counts):
        raise AssertionError(f"{out_tif.name}: unexpected output size {len(out)}")
    out_tif.write_bytes(out)


def write_tiled_ycbcr_tiff(stripped_tif, out_tif, w, h, hs, vs, tile):
    """Write a genuine chunky tiled YCbCr TIFF from a stripped 2x2 TIFF.

    ``tiffcp`` refuses subsampled input, so this pure-Python ``struct`` writer
    lays the cell-interleaved strip bytes out as square luma tiles instead of
    tiling a 1x1 image and patching on a subsampling tag. For a 64x48 image
    with 2x2 subsampling and 16x16 tiles this produces a 4x3 grid of 12 tiles;
    every tile holds 8x8 cells of six bytes (``hs*vs`` Y samples, Cb, Cr),
    i.e. 384 bytes. All four YCbCr tags, PI = 6 and PlanarConfig = 1 are
    copied/written into the new TIFF itself. No hard-coded offsets, no
    external imaging library.
    """
    stripped_tif = Path(stripped_tif)
    out_tif = Path(out_tif)
    src = parse_tiff(stripped_tif)

    def one(tag):
        values = src[tag][1]
        if len(values) != 1:
            raise ValueError(
                f"{stripped_tif.name}: tag {tag} has {len(values)} values")
        return values[0]

    if (one(256), one(257)) != (w, h):
        raise ValueError(f"{stripped_tif.name}: dimensions differ from {w}x{h}")
    if src[530][1] != [hs, vs]:
        raise ValueError(
            f"{stripped_tif.name}: subsampling differs from {hs}x{vs}")
    if one(284) != 1:
        raise ValueError(f"{stripped_tif.name}: source is not chunky")
    if w % tile or h % tile or tile % hs or tile % vs:
        raise ValueError(
            f"{stripped_tif.name}: {w}x{h} and {tile}x{tile} tiles are not "
            f"compatible with {hs}x{vs} subsampling")

    offsets, counts = src[273][1], src[279][1]
    if not offsets or len(offsets) != len(counts):
        raise ValueError(f"{stripped_tif.name}: inconsistent strip arrays")
    data = stripped_tif.read_bytes()
    pixels = b"".join(data[off:off + count]
                      for off, count in sorted(zip(offsets, counts)))

    chroma_w, chroma_h = w // hs, h // vs
    cell_size = hs * vs + 2
    if len(pixels) != chroma_w * chroma_h * cell_size:
        raise ValueError(
            f"{stripped_tif.name}: strip data is {len(pixels)} bytes, expected "
            f"{chroma_w * chroma_h * cell_size} for {w}x{h} {hs}x{vs}")
    cells_x, cells_y = tile // hs, tile // vs
    tiles = []
    for ty in range(h // tile):
        for tx in range(w // tile):
            blob = bytearray()
            for cy in range(cells_y):
                for cx in range(cells_x):
                    gx, gy = tx * cells_x + cx, ty * cells_y + cy
                    if gx >= chroma_w or gy >= chroma_h:
                        continue
                    base = (gy * chroma_w + gx) * cell_size
                    blob += pixels[base:base + cell_size]
            tiles.append(bytes(blob))
    ntiles = len(tiles)
    tile_counts = [len(blob) for blob in tiles]

    coeffs = (src[529][1] if 529 in src else
              [(299, 1000), (587, 1000), (114, 1000)])
    refbw = (src[532][1] if 532 in src else
             [(0, 1), (255, 1), (128, 1), (255, 1), (128, 1), (255, 1)])
    bits = src[258][1] if 258 in src else [8, 8, 8]
    compression = one(259) if 259 in src else 1
    photometric = one(262) if 262 in src else 6
    fill_order = src[266][1][0] if 266 in src else 1
    spp = one(277) if 277 in src else 3
    positioning = one(531) if 531 in src else 1

    specs = [
        (256, _TIFF_SHORT, [w]),
        (257, _TIFF_SHORT, [h]),
        (258, _TIFF_SHORT, bits),
        (259, _TIFF_SHORT, [compression]),
        (262, _TIFF_SHORT, [photometric]),
        (266, _TIFF_SHORT, [fill_order]),
        (277, _TIFF_SHORT, [spp]),
        (284, _TIFF_SHORT, [1]),          # PlanarConfiguration = chunky
        (322, _TIFF_SHORT, [tile]),       # TileWidth
        (323, _TIFF_SHORT, [tile]),       # TileLength
        (324, _TIFF_LONG, None),          # TileOffsets, patched below
        (325, _TIFF_LONG, None),          # TileByteCounts, patched below
        (529, _TIFF_RATIONAL, coeffs),
        (530, _TIFF_SHORT, [hs, vs]),
        (531, _TIFF_SHORT, [positioning]),
        (532, _TIFF_RATIONAL, refbw),
    ]
    specs.sort(key=lambda spec: spec[0])
    n = len(specs)
    ext_off = 8 + 2 + n * 12 + 4

    # Placeholder tile arrays only determine payload sizes (ntiles LONGs each).
    placeholder = [(tag, typ, values if values is not None else [0] * ntiles)
                   for tag, typ, values in specs]
    encoded = [(tag, typ, len(values), _encode_values(values, typ))
               for tag, typ, values in placeholder]
    ext_sizes = {tag: len(payload) + (len(payload) % 2)
                 for tag, _, _, payload in encoded if len(payload) > 4}
    pix_off = ext_off + sum(ext_sizes.values())
    tile_offsets = []
    cursor = pix_off
    for blob in tiles:
        tile_offsets.append(cursor)
        cursor += len(blob)
    specs = [(tag, typ,
              tile_offsets if tag == 324 else
              tile_counts if tag == 325 else values)
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
    for blob in tiles:
        out += blob
    if len(out) != pix_off + sum(tile_counts):
        raise AssertionError(f"{out_tif.name}: unexpected output size {len(out)}")
    out_tif.write_bytes(out)


# --- Pure-Python RGB -> YCbCr encoding for non-default tag values ------------


def as_tiff_rational(value):
    """Return an exact ``(numerator, denominator)`` pair for a tag value."""
    fraction = Fraction(str(value))
    return fraction.numerator, fraction.denominator


def encode_ycbcr_codes(pixels, w, h, hs, vs, coeffs, refbw):
    """Compute cell-interleaved 8-bit YCbCr codes from RGB source pixels.

    The forward transform is the inverse of libtiff's ``TIFFYCbCrtoRGB``
    (``tif_color.c``), parameterized by the very tag values written into the
    TIFF: ``coeffs`` = YCbCrCoefficients (Lr, Lg, Lb) and ``refbw`` =
    ReferenceBlackWhite (Yb, Yw, Cbb, Cbw, Crb, Crw). Full-range codes are
    ``Y = Lr*R + Lg*G + Lb*B``, ``Cb = (B - Y) / (2 - 2*Lb)`` and
    ``Cr = (R - Y) / (2 - 2*Lr)``. Stored codes use CodingRange 255 for Y and
    127 for Cb/Cr, exactly as libtiff's ``Code2V`` does. Chroma is sampled at
    the origin luma pixel of each ``hs x vs`` cell; for the 1x1 fixtures used
    here that is the pixel itself, so Y/Cb/Cr all come from one RGB triple.

    Returns ``(cells, chroma_w, chroma_h)``: ``cells`` is a byte string with
    ``hs*vs`` luma codes followed by one Cb and one Cr code per chroma cell,
    in raster order (the chunky data-unit layout required by TIFF 6.0).
    """
    Lr, Lg, Lb = coeffs
    Yb, Yw, Cbb, Cbw, Crb, Crw = refbw
    if not (Lr > 0 and Lg > 0 and Lb > 0):
        raise ValueError("YCbCrCoefficients must be positive")
    Lrp, Lbp = 2 - 2 * Lr, 2 - 2 * Lb
    if not (Yw > Yb and Cbw > Cbb and Crw > Crb):
        raise ValueError("ReferenceBlackWhite white values must exceed black")

    chroma_w, chroma_h = (w + hs - 1) // hs, (h + vs - 1) // vs

    def code(value, black, white, coding_range):
        stored = black + value * (white - black) / coding_range
        return max(0, min(255, int(math.floor(stored + 0.5))))

    cells = bytearray()
    for cy in range(chroma_h):
        for cx in range(chroma_w):
            # Chroma comes from the luma sample at the cell origin. The extra
            # luma samples in an odd-sized edge cell are edge-replicated.
            r, g, b = pixels[min(cy * vs, h - 1)][min(cx * hs, w - 1)]
            Yc = Lr * r + Lg * g + Lb * b
            Cb_code = code((b - Yc) / Lbp, Cbb, Cbw, 127)
            Cr_code = code((r - Yc) / Lrp, Crb, Crw, 127)
            for j in range(hs * vs):
                yy, xx = cy * vs + j // hs, cx * hs + j % hs
                r, g, b = pixels[min(yy, h - 1)][min(xx, w - 1)]
                cells.append(code(Lr * r + Lg * g + Lb * b, Yb, Yw, 255))
            cells.append(Cb_code)
            cells.append(Cr_code)
    return bytes(cells), chroma_w, chroma_h


def write_stripped_ycbcr_tiff(out_tif, w, h, hs, vs, cells, coeffs, refbw,
                              positioning=1):
    """Write a stripped, chunky YCbCr TIFF from computed YCbCr codes.

    ``cells`` is the cell-interleaved byte string returned by
    ``encode_ycbcr_codes``. Every YCbCr tag (529-532) and PI=6 are written
    into the new TIFF itself: no tiffset patch is applied afterwards, so the
    tag values and the pixel data are guaranteed to describe the same image.
    One strip is used, with RowsPerStrip = ImageLength, as in the other
    stripped fixtures.
    """
    out_tif = Path(out_tif)
    cell_size = hs * vs + 2
    chroma_w, chroma_h = (w + hs - 1) // hs, (h + vs - 1) // vs
    if len(cells) != chroma_w * chroma_h * cell_size:
        raise ValueError(
            f"{out_tif.name}: {len(cells)} code bytes, expected "
            f"{chroma_w * chroma_h * cell_size}")

    specs = [
        (256, _TIFF_SHORT, [w]),
        (257, _TIFF_SHORT, [h]),
        (258, _TIFF_SHORT, [8, 8, 8]),
        (259, _TIFF_SHORT, [1]),
        (262, _TIFF_SHORT, [6]),
        (266, _TIFF_SHORT, [1]),
        (273, _TIFF_LONG, None),          # StripOffsets, patched below
        (277, _TIFF_SHORT, [3]),
        (278, _TIFF_SHORT, [h]),          # one strip per image
        (279, _TIFF_LONG, None),          # StripByteCounts, patched below
        (284, _TIFF_SHORT, [1]),          # PlanarConfiguration = chunky
        (529, _TIFF_RATIONAL, [as_tiff_rational(v) for v in coeffs]),
        (530, _TIFF_SHORT, [hs, vs]),
        (531, _TIFF_SHORT, [positioning]),
        (532, _TIFF_RATIONAL, [as_tiff_rational(v) for v in refbw]),
    ]
    specs.sort(key=lambda spec: spec[0])
    n = len(specs)
    ext_off = 8 + 2 + n * 12 + 4

    # Placeholders determine external payload sizes; the strip arrays are
    # single LONGs and therefore stay inline in the IFD.
    placeholder = [(tag, typ, values if values is not None else [0])
                   for tag, typ, values in specs]
    encoded = [(tag, typ, len(values), _encode_values(values, typ))
               for tag, typ, values in placeholder]
    ext_sizes = {tag: len(payload) + (len(payload) % 2)
                 for tag, _, _, payload in encoded if len(payload) > 4}
    pix_off = ext_off + sum(ext_sizes.values())
    specs = [(tag, typ, [pix_off] if tag == 273 else
              [len(cells)] if tag == 279 else values)
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
    out += cells
    if len(out) != pix_off + len(cells):
        raise AssertionError(f"{out_tif.name}: unexpected output size {len(out)}")
    out_tif.write_bytes(out)


def verify_ground_truth(name, w, h, max_mean_diff):
    """Assert that libtiff's decoded PPM is close to this fixture's RGB source.

    Prints the first source/decoded pixel and the mean absolute per-channel
    difference, then fails if the reference is not the conversion of the
    fixture's own source. Used for the non-default-coefficient/RefBW fixtures,
    where the pixel data is computed in Python and decoded by libtiff.
    """
    seed = zlib.crc32(name.encode()) & 0xFFFFFFFF
    source = source_pixels(w, h, seed)

    def read_ppm(path):
        data = Path(path).read_bytes()
        # P6\n<w> <h>\n255\n<pixels>
        parts = data.split(b"\n", 3)
        if parts[0] != b"P6":
            raise ValueError(f"{Path(path).name}: not a binary P6 PPM")
        dims = parts[1].split()
        if (int(dims[0]), int(dims[1])) != (w, h) or parts[2] != b"255":
            raise ValueError(f"{Path(path).name}: unexpected PPM header")
        body = parts[3]
        if len(body) != w * h * 3:
            raise ValueError(f"{Path(path).name}: unexpected PPM payload size")
        return [[tuple(body[(y * w + x) * 3:(y * w + x + 1) * 3])
                 for x in range(w)] for y in range(h)]

    decoded = read_ppm(REFERENCE_DIR / f"{name}.ppm")
    diffs = [abs(a - b) for y in range(h) for x in range(w)
             for a, b in zip(source[y][x], decoded[y][x])]
    mean = sum(diffs) / len(diffs)
    print(f"  {name}: source first pixel {source[0][0]}, "
          f"libtiff-decoded first pixel {decoded[0][0]}, "
          f"mean abs diff {mean:.3f} (limit {max_mean_diff})")
    if mean > max_mean_diff:
        raise ValueError(
            f"{name}: decoded reference differs from source (mean abs diff "
            f"{mean:.3f} > {max_mean_diff})")


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
        # ycbcr-tiled-2x2: 16x16 luma tiles over 64x48 -> 4x3 = 12 tiles;
        # each tile holds 8x8 six-byte cells = 384 bytes.
        if expected != 12:
            raise ValueError(f"{path.name}: expected 12 tiles, got {expected}")
        bytes_per_tile = (tw // hs) * (th // vs) * (hs * vs + 2)
        if bytes_per_tile != 384 or any(count != bytes_per_tile for count in counts):
            raise ValueError(
                f"{path.name}: tile byte counts {counts}, expected twelve 384-byte tiles")
    else:
        rps = one(278)
        offsets, counts = need(273), need(279)
        strips_per_plane = (h + rps - 1) // rps

        def plane_counts(rows, rows_per_strip):
            return [w * max(0, min(rows_per_strip, rows - i * rows_per_strip))
                    for i in range(strips_per_plane)]

        if planar == 1:
            # libtiff pads luma dimensions up to multiples of the subsampling
            # factors when W/H are not evenly divisible (the odd fixtures).
            pad_w = ((w + hs - 1) // hs) * hs
            pad_h = ((h + vs - 1) // vs) * vs
            expected_bytes = pad_w * pad_h + 2 * (pad_w // hs) * (pad_h // vs)
            expected_counts = [expected_bytes] if strips_per_plane == 1 else None
            expected_strips = strips_per_plane
        else:
            if rps % vs:
                raise ValueError(
                    f"{path.name}: RowsPerStrip {rps} is not a multiple of "
                    f"vertical subsampling {vs}")
            y_counts = plane_counts(h, rps)
            # libtiff uses a full-width scanline stride and maps scanlines to
            # strips by row/RowsPerStrip in every plane, so a subsampled chroma
            # strip holds rps full-width scanlines (the tail is padding).
            chroma_counts = (y_counts if vs == 1 else
                             [w * rps] * strips_per_plane)
            expected_counts = y_counts + chroma_counts + chroma_counts
            # libtiff counts chroma strips with the luma RowsPerStrip too, so
            # every plane contributes ceil(h / RowsPerStrip) strips.
            expected_strips = 3 * strips_per_plane
        if len(offsets) != len(counts) or len(offsets) != expected_strips:
            raise ValueError(
                f"{path.name}: strip arrays have {len(offsets)}/{len(counts)} "
                f"entries, expected {expected_strips}")
        if expected_counts is not None and counts != expected_counts:
            raise ValueError(
                f"{path.name}: strip byte counts {counts}, "
                f"expected {expected_counts}")
        if planar == 1 and strips_per_plane == 1 and sum(counts) != expected_bytes:
            raise ValueError(
                f"{path.name}: strip byte total {sum(counts)}, "
                f"expected {expected_bytes}")
        size = path.stat().st_size
        if len(set(offsets)) != len(offsets):
            raise ValueError(f"{path.name}: duplicate strip offsets")
        if any(off < 8 or off + cnt > size
               for off, cnt in zip(offsets, counts)):
            raise ValueError(f"{path.name}: strip offset/count out of range")

    proc = subprocess.run(["tiffinfo", str(path)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, universal_newlines=True)
    if proc.returncode != 0:
        raise ValueError(f"{path.name}: tiffinfo exited {proc.returncode}")
    if "Warning" in proc.stderr or "Error" in proc.stderr:
        raise ValueError(f"{path.name}: tiffinfo reported: {proc.stderr.strip()}")


def make_ground_truth(name, tif):
    """Produce the P6 PPM ground truth with tiff2rgba + tifftopnm.

    Used for the 14 fixtures libtiff's RGBA helper can decode. The 1x4, 2x4
    and planar-2x2 cases use the libtiff TIFFReadScanline helper instead.
    """
    rgba = TMP_DIR / f"{name}-rgba.tif"
    ppm = REFERENCE_DIR / f"{name}.ppm"
    run("tiff2rgba", "-n", "-c", "none", tif, rgba)
    with open(rgba, "rb") as fin, open(ppm, "wb") as fout:
        run("tifftopnm", stdin=fin, stdout=fout,
            stderr=subprocess.DEVNULL)


def make_dscf0013_ground_truth():
    """Render references for both dscf0013 IFDs with libtiff only.

    ``tests/libtiff-pics/dscf0013.tif`` holds two YCbCr IFDs: the default
    640x480 image (``TiffImage::read`` opens IFD 0) and a 160x120 thumbnail.
    ``tiffsplit`` separates them (``tiffcp`` refuses subsampled images), then
    ``tiff2rgba`` and ``tifftopnm`` produce ``dscf0013.ppm`` and
    ``dscf0013-ifd1.ppm`` respectively.
    """
    source = (IMAGE_DIR.parent / "libtiff-pics" / "dscf0013.tif").resolve()
    if not source.is_file():
        raise RuntimeError(f"missing libtiff-pics fixture: {source}")
    prefix = TMP_DIR / "dscf0013-split-"
    for stale in TMP_DIR.glob("dscf0013-split-*.tif"):
        stale.unlink()
    run("tiffsplit", source, prefix, stderr=subprocess.DEVNULL)
    parts = sorted(TMP_DIR.glob("dscf0013-split-*.tif"))
    if len(parts) != 2:
        raise RuntimeError(f"tiffsplit produced {len(parts)} dscf0013 IFDs, expected 2")
    for index, (part, name, expected) in enumerate(zip(
            parts, ("dscf0013", "dscf0013-ifd1"), (b"P6 640 480", b"P6 160 120"))):
        rgba = TMP_DIR / f"dscf0013-ifd{index}-rgba.tif"
        run("tiff2rgba", "-n", "-c", "none", part, rgba)
        ppm = REFERENCE_DIR / f"{name}.ppm"
        with open(rgba, "rb") as fin, open(ppm, "wb") as fout:
            run("tifftopnm", stdin=fin, stdout=fout, stderr=subprocess.DEVNULL)
        with open(ppm, "rb") as f:
            header = f.readline().strip() + b" " + f.readline().strip()
        if header != expected:
            raise ValueError(f"{name}.ppm unexpected header: {header!r}")


# --- libtiff scanline decoding for paths tiff2rgba rejects ------------------


def build_scanline_dump():
    """Compile the in-repo libtiff helper into TMP_DIR."""
    if shutil.which("gcc") is None:
        raise RuntimeError(
            "gcc is required to build tests/ycbcr_images/ycbcr_scanline_dump.c; "
            "install a C compiler and libtiff development headers")
    if not SCANLINE_DUMP_SRC.is_file():
        raise RuntimeError(f"missing libtiff helper source: {SCANLINE_DUMP_SRC}")
    try:
        run("gcc", "-I/usr/local/include", "-L/usr/local/lib",
            SCANLINE_DUMP_SRC, "-ltiff", "-o", SCANLINE_DUMP_BIN)
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(
            "could not compile tests/ycbcr_images/ycbcr_scanline_dump.c; "
            "install libtiff headers and library in /usr/local/include and "
            f"/usr/local/lib (gcc exit {exc.returncode})") from exc
    return SCANLINE_DUMP_BIN


def decode_scanlines_to_ppm(tif_path, ppm_path, w, h, hs, vs, helper):
    """Write a P6 PPM from libtiff's decoded YCbCr scanlines.

    ``helper`` is the compiled ``ycbcr_scanline_dump.c`` program, which uses
    ``TIFFReadScanline`` (unlike ``tiff2rgba``, it handles 4x vertical
    subsampling) to emit the raw decoded YCbCr samples. This function converts
    those samples to RGB with the TIFF's own ``YCbCrCoefficients`` and
    ``ReferenceBlackWhite`` values, parsed with ``parse_tiff``. Chroma uses
    nearest-cell replication: a cell's Cb/Cr codes apply to all its luma pixels.

    ReferenceBlackWhite is scaled with CodingRange 255 for Y and 127 for Cb/Cr,
    as required by TIFF 6.0 Section 21.
    """
    tif_path = Path(tif_path)
    tags = parse_tiff(tif_path)

    def one(tag):
        values = tags[tag][1]
        if len(values) != 1:
            raise ValueError(
                f"{tif_path.name}: tag {tag} has {len(values)} values")
        return values[0]

    def rationals(tag):
        values = tags[tag][1]
        if any(den <= 0 for _, den in values):
            raise ValueError(f"{tif_path.name}: tag {tag} has zero denominator")
        return [num / den for num, den in values]

    if (one(256), one(257)) != (w, h):
        raise ValueError(f"{tif_path.name}: dimensions differ from {w}x{h}")
    if one(262) != 6 or one(284) != 1:
        raise ValueError(f"{tif_path.name}: expected chunky YCbCr data")
    if tags[258][1] != [8, 8, 8]:
        raise ValueError(f"{tif_path.name}: BitsPerSample is not [8, 8, 8]")
    if tags[530][1] != [hs, vs]:
        raise ValueError(
            f"{tif_path.name}: subsampling differs from {hs}x{vs}")

    proc = subprocess.run([str(helper), str(tif_path)],
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if proc.returncode != 0:
        diag = proc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"{Path(helper).name} failed on {tif_path.name} "
            f"(exit {proc.returncode}): {diag}")

    chroma_w = (w + hs - 1) // hs
    chroma_h = (h + vs - 1) // vs
    cell_size = hs * vs + 2
    expected = chroma_w * chroma_h * cell_size
    samples = proc.stdout
    if len(samples) != expected:
        raise ValueError(
            f"{tif_path.name}: libtiff returned {len(samples)} scanline bytes, "
            f"expected {expected} for {w}x{h} {hs}x{vs}")

    Lr, Lg, Lb = rationals(529)
    if Lg <= 0:
        raise ValueError(f"{tif_path.name}: invalid YCbCrCoefficients")
    Yb, Yw, Cbb, Cbw, Crb, Crw = rationals(532)
    Lrp, Lbp = 2 - 2 * Lr, 2 - 2 * Lb

    def y_full(code):
        return (code - Yb) * 255 / (Yw - Yb) if Yw != Yb else float(code)

    def chroma_full(code, black, white):
        if white == black:
            return float(code - 128)
        return (code - black) * 127 / (white - black)

    out = bytearray(f"P6\n{w} {h}\n255\n".encode("ascii"))
    # Emit in PPM raster order (row by row), not cell by cell: a cell's luma
    # samples are stored row-major within the cell (j = row*hs + col).
    for yy in range(h):
        for xx in range(w):
            cy, cx = yy // vs, xx // hs
            base = (cy * chroma_w + cx) * cell_size
            j = (yy % vs) * hs + (xx % hs)
            Ycode = samples[base + j]
            C = chroma_full(samples[base + hs * vs], Cbb, Cbw)
            R = chroma_full(samples[base + hs * vs + 1], Crb, Crw)
            Y = y_full(Ycode)
            rgb = (Y + Lrp * R,
                   Y - (Lb * Lbp / Lg) * C - (Lr * Lrp / Lg) * R,
                   Y + Lbp * C)
            out.extend(bytes(max(0, min(255, round(v))) for v in rgb))
    Path(ppm_path).write_bytes(out)


def decode_planar_scanlines_to_ppm(tif_path, ppm_path, w, h, hs, vs, helper):
    """Write a P6 PPM from libtiff's decoded PLANAR YCbCr scanlines.

    ``ycbcr_scanline_dump.c`` emits a PlanarConfiguration = 2 image's three
    planes back-to-back: all Y scanlines (h rows of w bytes), then all Cb
    scanlines, then all Cr scanlines (up to h rows of w bytes each; TIFF's
    padded chroma strips are read only for valid image rows). For a
    subsampled plane only the first ceil(w/hs) bytes of each real chroma row
    are chroma samples; the tail is edge-replicated padding. libtiff's
    planar scanline size is the full image width for every plane, including
    the odd-sized 63x61 fixture. This function converts those libtiff-decoded
    bytes to RGB with the TIFF's own
    YCbCrCoefficients and ReferenceBlackWhite values, using the same
    nearest-cell chroma replication and CodingRange 255/127 scaling as
    ``decode_scanlines_to_ppm``. It is used for ``ycbcr-planar-2x2.tif``,
    whose planar + subsampled pixels libtiff's RGBA helper cannot decode.
    """
    tif_path = Path(tif_path)
    tags = parse_tiff(tif_path)

    def one(tag):
        values = tags[tag][1]
        if len(values) != 1:
            raise ValueError(
                f"{tif_path.name}: tag {tag} has {len(values)} values")
        return values[0]

    def rationals(tag):
        values = tags[tag][1]
        if any(den <= 0 for _, den in values):
            raise ValueError(f"{tif_path.name}: tag {tag} has zero denominator")
        return [num / den for num, den in values]

    if (one(256), one(257)) != (w, h):
        raise ValueError(f"{tif_path.name}: dimensions differ from {w}x{h}")
    if one(262) != 6 or one(284) != 2:
        raise ValueError(f"{tif_path.name}: expected planar YCbCr data")
    if tags[258][1] != [8, 8, 8]:
        raise ValueError(f"{tif_path.name}: BitsPerSample is not [8, 8, 8]")
    if tags[530][1] != [hs, vs]:
        raise ValueError(
            f"{tif_path.name}: subsampling differs from {hs}x{vs}")
    rps = one(278)
    if rps % vs:
        raise ValueError(f"{tif_path.name}: RowsPerStrip is not a multiple of {vs}")

    proc = subprocess.run([str(helper), str(tif_path)],
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if proc.returncode != 0:
        diag = proc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"{Path(helper).name} failed on {tif_path.name} "
            f"(exit {proc.returncode}): {diag}")

    chroma_w, chroma_h = (w + hs - 1) // hs, (h + vs - 1) // vs
    # The helper reads at most h scanlines per plane (TIFFReadScanline
    # rejects row >= ImageLength), even when chroma strips are padded.
    y_size = c_size = w * h
    expected = y_size + 2 * c_size
    samples = proc.stdout
    if len(samples) != expected:
        raise ValueError(
            f"{tif_path.name}: libtiff returned {len(samples)} scanline bytes, "
            f"expected {expected} for planar {w}x{h} {hs}x{vs}")
    Y = samples[:y_size]
    Cb = samples[y_size:y_size + c_size]
    Cr = samples[y_size + c_size:]

    Lr, Lg, Lb = rationals(529)
    if Lg <= 0:
        raise ValueError(f"{tif_path.name}: invalid YCbCrCoefficients")
    Yb, Yw, Cbb, Cbw, Crb, Crw = rationals(532)
    Lrp, Lbp = 2 - 2 * Lr, 2 - 2 * Lb

    def y_full(code):
        return (code - Yb) * 255 / (Yw - Yb) if Yw != Yb else float(code)

    def chroma_full(code, black, white):
        if white == black:
            return float(code - 128)
        return (code - black) * 127 / (white - black)

    out = bytearray(f"P6\n{w} {h}\n255\n".encode("ascii"))
    # Same raster order as decode_scanlines_to_ppm. Each plane row has a
    # full-width stride; only the first chroma_w samples of a chroma row are
    # real Cb/Cr values (the tail is edge-replicated padding).
    for yy in range(h):
        for xx in range(w):
            cy, cx = yy // vs, xx // hs
            Ycode = Y[yy * w + xx]
            C = chroma_full(Cb[cy * w + cx], Cbb, Cbw)
            R = chroma_full(Cr[cy * w + cx], Crb, Crw)
            Yval = y_full(Ycode)
            rgb = (Yval + Lrp * R,
                   Yval - (Lb * Lbp / Lg) * C - (Lr * Lrp / Lg) * R,
                   Yval + Lbp * C)
            out.extend(bytes(max(0, min(255, round(v))) for v in rgb))
    Path(ppm_path).write_bytes(out)


# libtiff's TIFFRGBAImage / tiff2rgba path rejects 4x vertical chroma
# subsampling, but TIFFReadScanline decodes those pixels. Ground truth for the
# 1x4 and 2x4 TIFFs therefore comes from the libtiff scanline helper.
NEEDS_LIBTIFF_SCANLINE = {(1, 4), (2, 4)}


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
    scanline_helper = build_scanline_dump()

    # 9 full-resolution subsampling fixtures: conversion-verified.
    for name, hs, vs in SUBSAMPLING_CASES:
        rgb = make_rgb_tiff(name, BASE_W, BASE_H)
        out = IMAGE_DIR / f"{name}.tif"
        make_ycbcr_tiff(name, rgb, BASE_W, BASE_H, hs, vs, out)
        if (hs, vs) in NEEDS_LIBTIFF_SCANLINE:
            decode_scanlines_to_ppm(
                out, REFERENCE_DIR / f"{name}.ppm", BASE_W, BASE_H, hs, vs,
                scanline_helper)
        else:
            make_ground_truth(name, out)

    # 3 odd-dimension fixtures: conversion-verified.
    for name, w, h, hs, vs in ODD_CASES:
        rgb = make_rgb_tiff(name, w, h)
        out = IMAGE_DIR / f"{name}.tif"
        make_ycbcr_tiff(name, rgb, w, h, hs, vs, out)
        make_ground_truth(name, out)

    # 5 variants: two non-default-tag conversion tests, two genuine
    # separate-plane conversion tests and one genuine tiled conversion test.
    # There is no co-sited fixture: libtiff 4.7.2 stores YCbCrPositioning but
    # never consults it while decoding (tiff2rgba output is byte-identical for
    # positioning 1 and 2), so such an image could only test tag reading.

    # BT.709 luma coefficients are computed into the Y/Cb/Cr codes and written
    # into tag 529 by the same writer; no tag is patched on afterwards.
    name = "ycbcr-custom-coeffs"
    pixels = source_pixels(BASE_W, BASE_H,
                           zlib.crc32(name.encode()) & 0xFFFFFFFF)
    coeffs = (0.2126, 0.7152, 0.0722)
    refbw = (0, 255, 128, 255, 128, 255)
    cells, _, _ = encode_ycbcr_codes(pixels, BASE_W, BASE_H, 1, 1,
                                     coeffs, refbw)
    out = IMAGE_DIR / f"{name}.tif"
    write_stripped_ycbcr_tiff(out, BASE_W, BASE_H, 1, 1, cells, coeffs, refbw)
    make_ground_truth(name, out)
    verify_ground_truth(name, BASE_W, BASE_H, 3.0)

    # Headroom/footroom ReferenceBlackWhite values are applied in the encoder
    # and written into tag 532 by the same writer.
    name = "ycbcr-custom-refbw"
    pixels = source_pixels(BASE_W, BASE_H,
                           zlib.crc32(name.encode()) & 0xFFFFFFFF)
    coeffs = (0.299, 0.587, 0.114)
    refbw = (15, 235, 128, 240, 128, 240)
    cells, _, _ = encode_ycbcr_codes(pixels, BASE_W, BASE_H, 1, 1,
                                     coeffs, refbw)
    out = IMAGE_DIR / f"{name}.tif"
    write_stripped_ycbcr_tiff(out, BASE_W, BASE_H, 1, 1, cells, coeffs, refbw)
    make_ground_truth(name, out)
    verify_ground_truth(name, BASE_W, BASE_H, 3.0)

    # genuine separate planes on a 1x1 base, 61x47 with 16 rows per strip:
    # tiffcp -p separate reorganizes the pixels into nine Y/Cb/Cr strips
    # (three per plane). tiffcp drops the YCbCr tags and resets
    # PhotometricInterpretation to RGB, so all five tags are restored after.
    # The ground truth is rendered from the FINAL planar TIFF.
    name = "ycbcr-planar-1x1"
    rgb = make_rgb_tiff(name, PLANAR_1X1_W, PLANAR_1X1_H)
    stripped = TMP_DIR / f"{name}-1x1.tif"
    out = IMAGE_DIR / f"{name}.tif"
    make_ycbcr_tiff(name, rgb, PLANAR_1X1_W, PLANAR_1X1_H, 1, 1, stripped)
    run("tiffcp", "-p", "separate", "-c", "none", "-r", PLANAR_RPS,
        stripped, out, stderr=subprocess.DEVNULL)
    set_tag(out, "PhotometricInterpretation", 6)
    set_tag(out, "YCbCrCoefficients", 0.299, 0.587, 0.114)
    set_tag(out, "YCbCrPositioning", 1)
    set_tag(out, "ReferenceBlackWhite", 0, 255, 128, 255, 128, 255)
    set_tag(out, "YCbCrSubsampling", 1, 1)
    make_ground_truth(name, out)

    # genuine separate planes on a 2x2 base, 63x61 with 16 luma rows per strip:
    # rgb2ycbcr writes cell-interleaved Y/Cb/Cr samples into one chunky strip;
    # the pure-Python writer de-interleaves them into four strips per plane
    # (12 total). libtiff counts chroma strips with the luma RowsPerStrip too.
    # tiff2rgba cannot decode planar + subsampled YCbCr, so the ground truth
    # comes from libtiff TIFFReadScanline decoding of the FINAL planar TIFF,
    # never from the equivalent chunky TIFF.
    name = "ycbcr-planar-2x2"
    rgb = make_rgb_tiff(name, PLANAR_2X2_W, PLANAR_2X2_H)
    chunky = TMP_DIR / f"{name}-chunky.tif"
    out = IMAGE_DIR / f"{name}.tif"
    make_ycbcr_tiff(name, rgb, PLANAR_2X2_W, PLANAR_2X2_H, 2, 2, chunky)
    write_planar_subsampled_tiff(chunky, out, 2, 2,
                                 PLANAR_2X2_W, PLANAR_2X2_H, PLANAR_RPS)
    decode_planar_scanlines_to_ppm(
        out, REFERENCE_DIR / f"{name}.ppm", PLANAR_2X2_W, PLANAR_2X2_H, 2, 2,
        scanline_helper)

    # genuine chunky tiles on a 2x2 base -- the pure-Python tile writer lays
    # the stripped 2x2 cell data out as 16x16 tiles (4x3 grid, 12 tiles), with
    # PI=6 and all four YCbCr tags written into the tiled TIFF itself. The
    # ground truth is rendered from that same tiled TIFF with tiff2rgba, so it
    # matches the stripped 2x2 reference byte-for-byte.
    name = "ycbcr-tiled-2x2"
    stripped_2x2 = IMAGE_DIR / "ycbcr-2x2.tif"
    out = IMAGE_DIR / f"{name}.tif"
    write_tiled_ycbcr_tiff(stripped_2x2, out, BASE_W, BASE_H, 2, 2, 16)
    make_ground_truth(name, out)

    # Validate every emitted TIFF. This catches inconsistent strip/tile arrays,
    # such as a planar tag patched onto chunky pixel data.
    if len(list(IMAGE_DIR.glob("ycbcr-*.tif"))) != len(ALL_NAMES):
        raise ValueError(f"expected exactly {len(ALL_NAMES)} YCbCr TIFFs")
    for name in ALL_NAMES:
        validate_fixture(IMAGE_DIR / f"{name}.tif")
    make_dscf0013_ground_truth()
    print(f"Generated and validated {len(ALL_NAMES)} YCbCr TIFF fixtures.")


if __name__ == "__main__":
    main()
