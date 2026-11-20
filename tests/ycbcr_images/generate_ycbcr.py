#!/usr/bin/env python3
"""Deterministic generator for the YCbCr test-image matrix.

All ground-truth pixel values come from libtiff: ``tiff2rgba -n -c none``
followed by ``tifftopnm``. No Python/Pillow/ImageMagick/TiffCraft code
converts YCbCr samples to RGB. Where libtiff's RGBA converter cannot decode a
fixture directly (planar + subsampled YCbCr), a planar 1x1 equivalent TIFF is
built from the SAME Y/Cb/Cr codes and that equivalent is decoded with the same
libtiff chain. The 16 equivalent TIFFs are committed in ``tests/reference_images/``.

The 29 conversion fixtures use the TIFF 6.0 Section 21 (TN2) conventions: odd
declared ImageWidth/ImageLength, stored luma padded up to multiples of the
subsampling factors, RowsPerStrip = 16 (stripped) or 16x16 tiles (tiled),
YCbCrPositioning = 1, BT.601 coefficients, default ReferenceBlackWhite, and
planar chroma rows stored either packed (ceil(w/hs) samples) or padded to the
declared image width. The two color-reference fixtures
(``ycbcr-color-coeffs-bt709`` and ``ycbcr-color-refbw-limited-range``) are
generated as well: 64x48 chunky 1x1 images (even dimensions, exempt from the
odd-dimension convention) that declare their own non-default tags — BT.709
coefficients and limited-range ReferenceBlackWhite respectively. All 31
fixtures, including the color references, are encoded honestly with the same
cell-interleaved encoder using the tag values they declare; no tag is patched
after encoding.

Each of the 31 generated fixtures has a libtiff-rendered PPM: 15 are decoded
directly from the fixture itself (13 conversion + 2 color-reference), and 16
conversion fixtures are decoded from planar 1x1 equivalents built from the same
Y/Cb/Cr codes.

The dscf0013 references are produced by ``tests/ycbcr_images/dscf0013_ppm.c``,
which walks all IFDs with TIFFReadDirectory and decodes each one with
TIFFReadRGBAImageOriented; regenerating them therefore also needs a C compiler
and libtiff development headers (compile with -I/usr/local/include,
-L/usr/local/lib and -ltiff).

Usage (from the repository root)::

    python3 tests/ycbcr_images/generate_ycbcr.py
"""

import math
import re
import struct
import subprocess
import zlib
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path

IMAGE_DIR = Path(__file__).resolve().parent
REFERENCE_DIR = Path(__file__).resolve().parent.parent / "reference_images"
TMP_DIR = Path("/tmp/opencode/ycbcr-gen")

TILE = 16
RPS = 16
LEGAL_FACTORS = ((1, 1), (2, 1), (2, 2), (4, 1), (4, 2), (4, 4))

# Exact TIFF tag values: BT.601 luma coefficients and default reference
# black/white points (8-bit full range for Y, 128-centered for Cb/Cr).
COEFFS = (Fraction(299, 1000), Fraction(587, 1000), Fraction(114, 1000))
REFBW = (Fraction(0, 1), Fraction(255, 1), Fraction(128, 1),
         Fraction(255, 1), Fraction(128, 1), Fraction(255, 1))

# Exact non-default tag values for the two generated color-reference fixtures.
# BT.709 luma coefficients: 2126 + 7152 + 722 = 10000, so the sum is exactly 1.
BT709_COEFFS = (Fraction(2126, 10000), Fraction(7152, 10000),
                Fraction(722, 10000))
# Limited-range (headroom/footroom) ReferenceBlackWhite: Y 15/235, Cb/Cr 128/240.
LIMITED_REFBW = (Fraction(15, 1), Fraction(235, 1), Fraction(128, 1),
                 Fraction(240, 1), Fraction(128, 1), Fraction(240, 1))

# factor -> (declared W, declared H, stored luma W, stored luma H,
#            chroma W, chroma H)
GEOMETRIES = {
    (1, 1): (63, 47, 63, 47, 63, 47),
    (2, 1): (63, 47, 64, 47, 32, 47),
    (2, 2): (61, 45, 62, 46, 31, 23),
    (4, 1): (61, 47, 64, 47, 16, 47),
    (4, 2): (57, 43, 60, 44, 15, 22),
    (4, 4): (63, 61, 64, 64, 16, 16),
}

_TIFF_TYPE_SIZE = {
    1: 1, 2: 1, 3: 2, 4: 4, 5: 8, 6: 1, 7: 1,
    8: 2, 9: 4, 10: 8, 11: 4, 12: 8,
}
_TIFF_SHORT, _TIFF_LONG, _TIFF_RATIONAL = 3, 4, 5


@dataclass(frozen=True)
class Fixture:
    """One rebuilt fixture: name, subsampling and storage organization."""

    name: str
    factor: tuple
    organization: str            # "chunky" or "planar"
    tiled: bool = False
    chroma_stride: str = "padded"  # planar stripped: "packed" or "padded"
    size: tuple | None = None       # explicit geometry (color refs: 64x48)
    coeffs: tuple | None = None     # explicit YCbCrCoefficients (tag 529)
    refbw: tuple | None = None      # explicit ReferenceBlackWhite (tag 532)
    color_reference: bool = False   # even dimensions / non-default tags allowed

    @property
    def hs(self):
        return self.factor[0]

    @property
    def vs(self):
        return self.factor[1]

    @property
    def geometry(self):
        return self.size if self.size is not None else GEOMETRIES[self.factor]

    @property
    def coeffs_values(self):
        return self.coeffs if self.coeffs is not None else COEFFS

    @property
    def refbw_values(self):
        return self.refbw if self.refbw is not None else REFBW

    @property
    def w(self):
        return self.geometry[0]

    @property
    def h(self):
        return self.geometry[1]

    @property
    def stored_w(self):
        return self.geometry[2]

    @property
    def stored_h(self):
        return self.geometry[3]

    @property
    def chroma_w(self):
        return self.geometry[4]

    @property
    def chroma_h(self):
        return self.geometry[5]

    @property
    def nstrips(self):
        return (self.stored_h + RPS - 1) // RPS

    @property
    def cell_size(self):
        return self.hs * self.vs + 2

    @property
    def chroma_row_stride(self):
        return self.chroma_w if self.chroma_stride == "packed" else self.w

    @property
    def tiff(self):
        return IMAGE_DIR / f"{self.name}.tif"

    @property
    def ppm(self):
        return REFERENCE_DIR / f"{self.name}.ppm"

    @property
    def equivalent(self):
        return REFERENCE_DIR / f"{self.name}-equiv.tif"


def _factor_name(factor):
    return f"{factor[0]}x{factor[1]}"


def _build_manifest():
    """The exact 31-fixture manifest (29 conversion + 2 color), in order."""
    fixtures = []
    for factor in LEGAL_FACTORS:
        tag = _factor_name(factor)
        fixtures.append(Fixture(f"ycbcr-chunky-{tag}", factor, "chunky"))
        fixtures.append(Fixture(f"ycbcr-chunky-tiled-{tag}", factor,
                                "chunky", tiled=True))
        fixtures.append(Fixture(f"ycbcr-planar-tiled-{tag}", factor,
                                "planar", tiled=True, chroma_stride="packed"))
        strides = ("packed", "padded") if factor != (1, 1) else ("packed",)
        for stride in strides:
            fixtures.append(Fixture(f"ycbcr-planar-{stride}-{tag}",
                                    factor, "planar", chroma_stride=stride))
    fixtures.append(Fixture(
        "ycbcr-color-coeffs-bt709", (1, 1), "chunky",
        size=(64, 48, 64, 48, 64, 48), coeffs=BT709_COEFFS,
        color_reference=True))
    fixtures.append(Fixture(
        "ycbcr-color-refbw-limited-range", (1, 1), "chunky",
        size=(64, 48, 64, 48, 64, 48), refbw=LIMITED_REFBW,
        color_reference=True))
    return tuple(fixtures)


FIXTURES = _build_manifest()
DSCF_PPMS = ("dscf0013.ppm", "dscf0013-ifd1.ppm")


def run(*args, stdin=None, stdout=None, stderr=None):
    """Run an external tool, aborting on a non-zero exit status."""
    subprocess.run([str(a) for a in args], check=True,
                   stdin=stdin, stdout=stdout, stderr=stderr)


def source_pixels(w, h, seed):
    """The canonical deterministic six-stripe RGB source pattern.

    Stripe i covers columns [floor(W*i/6), floor(W*(i+1)/6)) and has a
    constant RGB color for every row:
    R_i = (seed*(i+1)+11) % 256, G_i = (seed*(i+3)+29) % 256,
    B_i = (seed*(i+5)+47) % 256.
    """
    colors = [None] * w
    for i in range(6):
        r = (seed * (i + 1) + 11) % 256
        g = (seed * (i + 3) + 29) % 256
        b = (seed * (i + 5) + 47) % 256
        for x in range((w * i) // 6, (w * (i + 1)) // 6):
            colors[x] = (r, g, b)
    if any(c is None for c in colors):
        raise AssertionError("stripe pattern does not cover all columns")
    row = [tuple(c) for c in colors]
    return [list(row) for _ in range(h)]


def as_tiff_rational(value):
    """Return an exact ``(numerator, denominator)`` pair for a tag value."""
    fraction = Fraction(value)
    return fraction.numerator, fraction.denominator


def encode_ycbcr_codes(pixels, w, h, hs, vs, coeffs, refbw):
    """Compute cell-interleaved 8-bit YCbCr codes from RGB source pixels.

    The forward transform is the exact inverse of libtiff's
    ``TIFFYCbCrtoRGB`` (``tif_color.c``), parameterized by the tag values the
    writer emits: ``coeffs`` = YCbCrCoefficients (Lr, Lg, Lb) and ``refbw`` =
    ReferenceBlackWhite (Yb, Yw, Cbb, Cbw, Crb, Crw). Full-range values are
    ``Y = Lr*R + Lg*G + Lb*B``, ``Cb = (B-Y)/(2-2*Lb)`` and
    ``Cr = (R-Y)/(2-2*Lr)``. Stored codes use CodingRange 255 for Y and 127
    for Cb/Cr, as libtiff's ``Code2V`` does, with round-half-up quantization.
    Chroma is sampled at the cell-origin luma pixel; extra luma samples in an
    odd-sized edge cell are edge-replicated. Returns
    ``(cells, chroma_w, chroma_h)`` where ``cells`` holds ``hs*vs`` luma codes
    followed by one Cb and one Cr code per chroma cell, in raster order.
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
        return max(0, min(255, math.floor(stored + Fraction(1, 2))))

    cells = bytearray()
    for cy in range(chroma_h):
        for cx in range(chroma_w):
            r, g, b = pixels[min(cy * vs, h - 1)][min(cx * hs, w - 1)]
            Yc = Lr * r + Lg * g + Lb * b
            cb_code = code((b - Yc) / Lbp, Cbb, Cbw, 127)
            cr_code = code((r - Yc) / Lrp, Crb, Crw, 127)
            for j in range(hs * vs):
                yy, xx = cy * vs + j // hs, cx * hs + j % hs
                r, g, b = pixels[min(yy, h - 1)][min(xx, w - 1)]
                cells.append(code(Lr * r + Lg * g + Lb * b, Yb, Yw, 255))
            cells.append(cb_code)
            cells.append(cr_code)
    return bytes(cells), chroma_w, chroma_h


# --- Pure-Python ClassicTIFF writer ------------------------------------------


def _encode_values(values, typ):
    """Encode tag values as a little-endian TIFF payload."""
    if typ == _TIFF_SHORT:
        return struct.pack(f"<{len(values)}H", *values)
    if typ == _TIFF_LONG:
        return struct.pack(f"<{len(values)}I", *values)
    if typ == _TIFF_RATIONAL:
        return b"".join(struct.pack("<II", num, den) for num, den in values)
    raise ValueError(f"unsupported TIFF type {typ}")


def write_tiff(path, specs, blobs, offset_tag):
    """Write one little-endian ClassicTIFF IFD plus concatenated image blobs.

    ``specs`` maps tag -> (type, values). The array for ``offset_tag`` is
    computed from the layout of ``blobs`` (which are stored contiguously after
    the IFD and external tag payloads); its placeholder value is ignored.
    """
    specs = dict(specs)
    # Give the offset array its final count before computing payload sizes;
    # a multi-strip/multi-tile LONG array lives outside the IFD.
    specs[offset_tag] = (_TIFF_LONG, [0] * len(blobs))
    ext_off = 8 + 2 + 12 * len(specs) + 4
    ext_sizes = {}
    for tag, (typ, values) in specs.items():
        payload = _encode_values(values, typ)
        if len(payload) > 4:
            ext_sizes[tag] = len(payload) + (len(payload) % 2)
    pix_off = ext_off + sum(ext_sizes.values())

    offsets = []
    cursor = pix_off
    for blob in blobs:
        offsets.append(cursor)
        cursor += len(blob)
    specs[offset_tag] = (_TIFF_LONG, offsets)

    encoded = [(tag, typ, len(values), _encode_values(values, typ))
               for tag, (typ, values) in sorted(specs.items())]
    ext_offsets = {}
    cursor = ext_off
    for tag, _, _, payload in encoded:
        if len(payload) > 4:
            ext_offsets[tag] = cursor
            cursor += ext_sizes[tag]
    if cursor != pix_off:
        raise AssertionError(f"{Path(path).name}: external payload layout mismatch")

    out = bytearray(b"II*\x00" + struct.pack("<I", 8)
                    + struct.pack("<H", len(specs)))
    for tag, typ, count, payload in encoded:
        out += struct.pack("<HHI", tag, typ, count)
        if len(payload) <= 4:
            out += payload.ljust(4, b"\0")
        else:
            out += struct.pack("<I", ext_offsets[tag])
    out += struct.pack("<I", 0)          # no next IFD
    for tag, _, _, payload in encoded:
        if tag in ext_offsets:
            out += payload + (b"\0" if len(payload) % 2 else b"")
    for blob in blobs:
        out += blob
    if len(out) != pix_off + sum(len(blob) for blob in blobs):
        raise AssertionError(f"{Path(path).name}: unexpected output size {len(out)}")
    Path(path).write_bytes(out)


def base_tags(w, h, hs, vs, planar, blobs, tiled, coeffs=COEFFS, refbw=REFBW):
    """Common YCbCr tag set; all required YCbCr tags are written inline.

    ``coeffs`` and ``refbw`` are the fixture's own tag values: the conversion
    fixtures use the module defaults, while the color references declare their
    non-default BT.709 coefficients / limited-range reference levels.
    """
    specs = {
        256: (_TIFF_SHORT, [w]),
        257: (_TIFF_SHORT, [h]),
        258: (_TIFF_SHORT, [8, 8, 8]),
        259: (_TIFF_SHORT, [1]),
        262: (_TIFF_SHORT, [6]),
        266: (_TIFF_SHORT, [1]),
        277: (_TIFF_SHORT, [3]),
        284: (_TIFF_SHORT, [2 if planar else 1]),
        529: (_TIFF_RATIONAL, [as_tiff_rational(v) for v in coeffs]),
        530: (_TIFF_SHORT, [hs, vs]),
        531: (_TIFF_SHORT, [1]),
        532: (_TIFF_RATIONAL, [as_tiff_rational(v) for v in refbw]),
    }
    if tiled:
        specs[322] = (_TIFF_SHORT, [TILE])
        specs[323] = (_TIFF_SHORT, [TILE])
        specs[324] = (_TIFF_LONG, [])
        specs[325] = (_TIFF_LONG, [len(blob) for blob in blobs])
        return specs, 324
    specs[273] = (_TIFF_LONG, [])
    specs[278] = (_TIFF_SHORT, [RPS])
    specs[279] = (_TIFF_LONG, [len(blob) for blob in blobs])
    return specs, 273


# --- Cell de-interleaving and layout builders --------------------------------


def cells_to_luma_plane(cells, cw, ch, hs, vs):
    """De-interleave cell luma samples into a stored_w x stored_h plane."""
    plane = bytearray()
    cell_size = hs * vs + 2
    for cy in range(ch):
        for j in range(vs):
            row = bytearray()
            for cx in range(cw):
                base = (cy * cw + cx) * cell_size
                row += cells[base + j * hs:base + (j + 1) * hs]
            plane += row
    return bytes(plane)


def cells_to_chroma_planes(cells, cw, ch, hs, vs, stride):
    """De-interleave chroma cells into two planes of ``ch`` rows.

    Each row has ``stride`` bytes: ``cw`` real samples followed, when
    ``stride > cw``, by edge-replicated padding up to the declared width.
    """
    cb, cr = bytearray(), bytearray()
    cell_size = hs * vs + 2
    for cy in range(ch):
        cb_row, cr_row = bytearray(), bytearray()
        for cx in range(cw):
            base = (cy * cw + cx) * cell_size
            cb_row.append(cells[base + hs * vs])
            cr_row.append(cells[base + hs * vs + 1])
        if stride > cw:
            cb_row += bytes([cb_row[-1]]) * (stride - cw)
            cr_row += bytes([cr_row[-1]]) * (stride - cw)
        cb += cb_row
        cr += cr_row
    return bytes(cb), bytes(cr)


def _strip_ranges(f):
    """Y-strip row ranges and chroma-strip cell-row ranges for RPS=16."""
    crows = RPS // f.vs
    if RPS % f.vs:
        raise ValueError(f"{f.name}: RowsPerStrip is not a multiple of vs")
    y_ranges = [(i * RPS, min((i + 1) * RPS, f.stored_h))
                for i in range(f.nstrips)]
    c_ranges = [(i * crows, min((i + 1) * crows, f.chroma_h))
                for i in range(f.nstrips)]
    return y_ranges, c_ranges, crows


def chunky_strip_blobs(f, cells):
    """Chunky strips, cell rows grouped by RowsPerStrip luma rows."""
    y_ranges, _, crows = _strip_ranges(f)
    per_row = f.chroma_w * f.cell_size
    blobs = []
    for index in range(f.nstrips):
        a = index * crows
        b = min((index + 1) * crows, f.chroma_h)
        blobs.append(cells[a * per_row:b * per_row])
    if sum(len(blob) for blob in blobs) != len(cells):
        raise AssertionError(f"{f.name}: chunky strip blobs do not cover cells")
    return blobs


def planar_strip_blobs(f, cells):
    """Three-plane strips: Y at stored width, chroma at the requested stride."""
    y_ranges, c_ranges, _ = _strip_ranges(f)
    y = cells_to_luma_plane(cells, f.chroma_w, f.chroma_h, f.hs, f.vs)
    cb, cr = cells_to_chroma_planes(cells, f.chroma_w, f.chroma_h, f.hs, f.vs,
                                   f.chroma_row_stride)
    blobs = [y[a * f.stored_w:b * f.stored_w] for a, b in y_ranges]
    for plane in (cb, cr):
        blobs += [plane[a * f.chroma_row_stride:b * f.chroma_row_stride]
                  for a, b in c_ranges]
    return blobs


def _tile_grid(w, h):
    return (w + TILE - 1) // TILE, (h + TILE - 1) // TILE


def chunky_tile_blobs(f, cells):
    """Chunky 16x16 tiles; chroma interleaved inside the luma tiles."""
    cells_x, cells_y = TILE // f.hs, TILE // f.vs
    nx, ny = _tile_grid(f.stored_w, f.stored_h)
    blobs = []
    for ty in range(ny):
        for tx in range(nx):
            blob = bytearray()
            for cy in range(cells_y):
                for cx in range(cells_x):
                    gx = min(tx * cells_x + cx, f.chroma_w - 1)
                    gy = min(ty * cells_y + cy, f.chroma_h - 1)
                    base = (gy * f.chroma_w + gx) * f.cell_size
                    blob += cells[base:base + f.cell_size]
            blobs.append(bytes(blob))
    return blobs


def planar_tile_blobs(f, cells):
    """Planar 16x16 luma tiles and (16/hs)x(16/vs) chroma tiles."""
    y = cells_to_luma_plane(cells, f.chroma_w, f.chroma_h, f.hs, f.vs)
    cb, cr = cells_to_chroma_planes(cells, f.chroma_w, f.chroma_h, f.hs, f.vs,
                                   f.chroma_w)
    nx, ny = _tile_grid(f.stored_w, f.stored_h)
    y_tiles = []
    for ty in range(ny):
        for tx in range(nx):
            blob = bytearray()
            for yoff in range(TILE):
                gy = min(ty * TILE + yoff, f.stored_h - 1)
                for xoff in range(TILE):
                    gx = min(tx * TILE + xoff, f.stored_w - 1)
                    blob.append(y[gy * f.stored_w + gx])
            y_tiles.append(bytes(blob))
    cw_tile, ch_tile = TILE // f.hs, TILE // f.vs
    c_tiles = []
    for plane in (cb, cr):
        for ty in range(ny):
            for tx in range(nx):
                blob = bytearray()
                for yoff in range(ch_tile):
                    gy = min(ty * ch_tile + yoff, f.chroma_h - 1)
                    for xoff in range(cw_tile):
                        gx = min(tx * cw_tile + xoff, f.chroma_w - 1)
                        blob.append(plane[gy * f.chroma_w + gx])
                c_tiles.append(bytes(blob))
    return y_tiles + c_tiles


def write_fixture(f, cells):
    """Write one rebuilt fixture TIFF into tests/ycbcr_images/."""
    if f.tiled:
        blobs = (chunky_tile_blobs(f, cells) if f.organization == "chunky"
                 else planar_tile_blobs(f, cells))
    else:
        blobs = (chunky_strip_blobs(f, cells) if f.organization == "chunky"
                 else planar_strip_blobs(f, cells))
    specs, offset_tag = base_tags(f.w, f.h, f.hs, f.vs,
                                  f.organization == "planar", blobs, f.tiled,
                                   coeffs=f.coeffs_values, refbw=f.refbw_values)
    write_tiff(f.tiff, specs, blobs, offset_tag)


def equivalent_planes(f, cells):
    """Full-resolution Y/Cb/Cr planes from the fixture's own Y/Cb/Cr codes.

    Chroma is replicated by nearest cell (edge-replicated at the stored
    boundary), exactly as libtiff's own subsampled decode does. The planes
    have the source's declared (odd, unpadded) dimensions.
    """
    y = bytearray(f.w * f.h)
    cb = bytearray(f.w * f.h)
    cr = bytearray(f.w * f.h)
    for yy in range(f.h):
        for xx in range(f.w):
            cx, cy = xx // f.hs, yy // f.vs
            base = (cy * f.chroma_w + cx) * f.cell_size
            j = (yy % f.vs) * f.hs + (xx % f.hs)
            pos = yy * f.w + xx
            y[pos] = cells[base + j]
            cb[pos] = cells[base + f.hs * f.vs]
            cr[pos] = cells[base + f.hs * f.vs + 1]
    return bytes(y), bytes(cb), bytes(cr)


def _plane_strips(plane, w, h):
    return [plane[a * w:b * w]
            for a, b in ((i * RPS, min((i + 1) * RPS, h))
                         for i in range((h + RPS - 1) // RPS))]


def _plane_tiles(plane, w, h):
    nx, ny = _tile_grid(w, h)
    tiles = []
    for ty in range(ny):
        for tx in range(nx):
            blob = bytearray()
            for yoff in range(TILE):
                gy = min(ty * TILE + yoff, h - 1)
                for xoff in range(TILE):
                    gx = min(tx * TILE + xoff, w - 1)
                    blob.append(plane[gy * w + gx])
            tiles.append(bytes(blob))
    return tiles


def write_equivalent(f, cells):
    """Write the committed planar-1x1 equivalent source TIFF for ``f``."""
    y, cb, cr = equivalent_planes(f, cells)
    if f.tiled:
        blobs = _plane_tiles(y, f.w, f.h) + _plane_tiles(cb, f.w, f.h) \
            + _plane_tiles(cr, f.w, f.h)
    else:
        blobs = (_plane_strips(y, f.w, f.h) + _plane_strips(cb, f.w, f.h)
                 + _plane_strips(cr, f.w, f.h))
    specs, offset_tag = base_tags(f.w, f.h, 1, 1, True, blobs, f.tiled)
    write_tiff(f.equivalent, specs, blobs, offset_tag)


# --- Structural validation ---------------------------------------------------


def parse_tiff(path):
    """Parse the first IFD of a classic TIFF into ``{tag: (type, values)}``."""
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


def check_tiffinfo_clean(path):
    """Require tiffinfo exit 0 and no warning/error text on stderr."""
    proc = subprocess.run(["tiffinfo", str(path)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, universal_newlines=True)
    if proc.returncode != 0:
        raise ValueError(f"{Path(path).name}: tiffinfo exited {proc.returncode}")
    if re.search(r"warning|error", proc.stderr, re.IGNORECASE):
        raise ValueError(f"{Path(path).name}: tiffinfo reported: {proc.stderr.strip()}")


def validate_fixture(f):
    """Structurally validate one rebuilt fixture against its manifest entry.

    Checks tag types/counts/values, the TN2 odd declared and padded stored
    geometry (except the even 64x48 color references), both planar chroma
    strides, strip/tile array lengths and byte counts, each fixture's own
    coefficients/reference levels, and tiffinfo cleanliness.
    """
    path = f.tiff
    tags = parse_tiff(path)

    def need(tag, typ=None):
        if tag not in tags:
            raise ValueError(f"{f.name}: missing required tag {tag}")
        if typ is not None and tags[tag][0] != typ:
            raise ValueError(f"{f.name}: tag {tag} has wrong type {tags[tag][0]}")
        return tags[tag][1]

    def one(tag, typ=None):
        values = need(tag, typ)
        if len(values) != 1:
            raise ValueError(
                f"{f.name}: tag {tag} must have 1 value, got {len(values)}")
        return values[0]

    expected_tags = [256, 257, 258, 259, 262, 266, 273, 277, 278, 279,
                     284, 529, 530, 531, 532]
    if f.tiled:
        expected_tags = [t for t in expected_tags if t not in (273, 278, 279)]
        expected_tags += [322, 323, 324, 325]
        for tag in (273, 278, 279):
            if tag in tags:
                raise ValueError(f"{f.name}: tiled image has strip tag {tag}")
    else:
        for tag in (322, 323, 324, 325):
            if tag in tags:
                raise ValueError(f"{f.name}: stripped image has tile tag {tag}")
    for tag in expected_tags:
        need(tag)

    if (one(256, _TIFF_SHORT), one(257, _TIFF_SHORT)) != (f.w, f.h):
        raise ValueError(f"{f.name}: declared dimensions are not {f.w}x{f.h}")
    if f.color_reference:
        if (f.w, f.h) != (64, 48):
            raise ValueError(f"{f.name}: color reference must be 64x48")
    elif f.w % 2 == 0 or f.h % 2 == 0:
        raise ValueError(f"{f.name}: declared dimensions must be odd")
    if one(262, _TIFF_SHORT) != 6:
        raise ValueError(f"{f.name}: PhotometricInterpretation is not 6")
    if need(258, _TIFF_SHORT) != [8, 8, 8]:
        raise ValueError(f"{f.name}: BitsPerSample is not [8, 8, 8]")
    if one(259, _TIFF_SHORT) != 1:
        raise ValueError(f"{f.name}: Compression is not 1")
    if need(266, _TIFF_SHORT) != [1]:
        raise ValueError(f"{f.name}: FillOrder is not 1")
    if one(277, _TIFF_SHORT) != 3:
        raise ValueError(f"{f.name}: SamplesPerPixel is not 3")
    expected_planar = 2 if f.organization == "planar" else 1
    if one(284, _TIFF_SHORT) != expected_planar:
        raise ValueError(f"{f.name}: wrong PlanarConfiguration")
    if need(529, _TIFF_RATIONAL) != [
            as_tiff_rational(v) for v in f.coeffs_values]:
        raise ValueError(f"{f.name}: YCbCrCoefficients do not match manifest")
    if need(530, _TIFF_SHORT) != [f.hs, f.vs]:
        raise ValueError(f"{f.name}: wrong YCbCrSubsampling")
    if (f.hs, f.vs) not in LEGAL_FACTORS or f.vs > f.hs:
        raise ValueError(f"{f.name}: illegal subsampling {f.hs}x{f.vs}")
    if one(531, _TIFF_SHORT) != 1:
        raise ValueError(f"{f.name}: YCbCrPositioning is not centered")
    if need(532, _TIFF_RATIONAL) != [
            as_tiff_rational(v) for v in f.refbw_values]:
        raise ValueError(
            f"{f.name}: ReferenceBlackWhite does not match manifest")

    size = path.stat().st_size
    if f.tiled:
        if one(322, _TIFF_SHORT) != TILE or one(323, _TIFF_SHORT) != TILE:
            raise ValueError(f"{f.name}: tile size is not 16x16")
        nx, ny = _tile_grid(f.stored_w, f.stored_h)
        grid = nx * ny
        offsets = need(324, _TIFF_LONG)
        counts = need(325, _TIFF_LONG)
        expected_len = grid if f.organization == "chunky" else 3 * grid
        if f.organization == "chunky":
            tile_bytes = (TILE // f.hs) * (TILE // f.vs) * f.cell_size
            expected_counts = [tile_bytes] * grid
        else:
            chroma_bytes = (TILE // f.hs) * (TILE // f.vs)
            expected_counts = ([TILE * TILE] * grid + [chroma_bytes] * grid
                               + [chroma_bytes] * grid)
    else:
        rps = one(278, _TIFF_SHORT)
        if rps != RPS:
            raise ValueError(f"{f.name}: RowsPerStrip is not 16")
        if RPS % f.vs:
            raise ValueError(f"{f.name}: RowsPerStrip is not a multiple of vs")
        offsets = need(273, _TIFF_LONG)
        counts = need(279, _TIFF_LONG)
        y_ranges, c_ranges, _ = _strip_ranges(f)
        if f.organization == "chunky":
            per_crow = f.chroma_w * f.cell_size
            expected_counts = [(b - a) * per_crow for a, b in c_ranges]
            expected_len = f.nstrips
        else:
            stride = f.chroma_row_stride
            y_counts = [(b - a) * f.stored_w for a, b in y_ranges]
            c_counts = [(b - a) * stride for a, b in c_ranges]
            expected_counts = y_counts + c_counts + c_counts
            expected_len = 3 * f.nstrips
    if len(offsets) != len(counts) or len(offsets) != expected_len:
        raise ValueError(
            f"{f.name}: array length {len(offsets)}/{len(counts)}, "
            f"expected {expected_len}")
    if counts != expected_counts:
        raise ValueError(
            f"{f.name}: byte counts {counts[:8]}..., expected "
            f"{expected_counts[:8]}...")
    if len(set(offsets)) != len(offsets):
        raise ValueError(f"{f.name}: duplicate image-data offsets")
    if any(off < 8 or off + cnt > size for off, cnt in zip(offsets, counts)):
        raise ValueError(f"{f.name}: image-data offset/count out of range")
    check_tiffinfo_clean(path)


# --- Ground truth: libtiff only ----------------------------------------------


def make_ground_truth(name, source_tif):
    """Render a P6 PPM with tiff2rgba -n -c none followed by tifftopnm."""
    rgba = TMP_DIR / f"{name}-rgba.tif"
    ppm = REFERENCE_DIR / f"{name}.ppm"
    run("tiff2rgba", "-n", "-c", "none", source_tif, rgba)
    with open(rgba, "rb") as fin, open(ppm, "wb") as fout:
        run("tifftopnm", stdin=fin, stdout=fout, stderr=subprocess.DEVNULL)


def make_dscf0013_ground_truth():
    """Render both dscf0013 IFD references via the libtiff C helper.

    The helper walks the original TIFF's IFDs with TIFFReadDirectory and
    decodes each one with TIFFReadRGBAImageOriented(..., ORIENTATION_TOPLEFT,
    0), so tags 530/531 stay intact. It needs a C compiler and libtiff
    development headers at generation time.
    """
    source = (IMAGE_DIR.parent / "libtiff-pics" / "dscf0013.tif").resolve()
    if not source.is_file():
        raise RuntimeError(f"missing libtiff-pics fixture: {source}")
    helper_source = IMAGE_DIR / "dscf0013_ppm.c"
    if not helper_source.is_file():
        raise RuntimeError(f"missing C helper: {helper_source}")
    helper_bin = TMP_DIR / "dscf0013_ppm"
    if (not helper_bin.is_file()
            or helper_bin.stat().st_mtime < helper_source.stat().st_mtime):
        run("cc", "-I/usr/local/include", "-L/usr/local/lib",
            helper_source, "-ltiff", "-o", helper_bin)

    out_dir = TMP_DIR / "dscf0013-ppms"
    if out_dir.exists():
        for stale in out_dir.glob("ifd*.ppm"):
            stale.unlink()
    else:
        out_dir.mkdir(parents=True)
    run(helper_bin, source, out_dir)

    parts = sorted(out_dir.glob("ifd*.ppm"))
    if len(parts) != 2:
        raise RuntimeError(
            f"helper produced {len(parts)} dscf0013 PPMs, expected 2")
    for part, name, expected in zip(
            parts, ("dscf0013", "dscf0013-ifd1"),
            (b"P6 640 480", b"P6 160 120")):
        data = part.read_bytes()
        header = data.splitlines()[0] + b" " + data.splitlines()[1]
        if header != expected:
            raise ValueError(f"{name}.ppm unexpected header: {header!r}")
        (REFERENCE_DIR / f"{name}.ppm").write_bytes(data)


def remove_stale_fixtures():
    """Delete stale YCbCr TIFFs/PPMs/equivalents; the manifest is exhaustive."""
    expected_tifs = {f"{f.name}.tif" for f in FIXTURES}
    expected_ppms = {f.ppm.name for f in FIXTURES} | set(DSCF_PPMS)
    expected_equivs = {f.equivalent.name for f in FIXTURES
                       if not (f.organization == "chunky"
                               or (not f.tiled and f.factor == (1, 1)))}
    for path in IMAGE_DIR.glob("ycbcr-*.tif"):
        if path.name not in expected_tifs:
            path.unlink()
    for path in REFERENCE_DIR.glob("ycbcr-*.ppm"):
        if path.name not in expected_ppms:
            path.unlink()
    for path in REFERENCE_DIR.glob("ycbcr-*-equiv.tif"):
        if path.name not in expected_equivs:
            path.unlink()


def main():
    TMP_DIR.mkdir(parents=True, exist_ok=True)
    IMAGE_DIR.mkdir(parents=True, exist_ok=True)
    REFERENCE_DIR.mkdir(parents=True, exist_ok=True)

    fixtures = {f.name: f for f in FIXTURES}
    if len(fixtures) != 31:
        raise AssertionError(f"expected 31 fixtures, manifest has {len(fixtures)}")
    direct_names = {f.name for f in FIXTURES
                    if f.organization == "chunky"
                    or (not f.tiled and f.factor == (1, 1))}
    if len(direct_names) != 15:
        raise AssertionError(f"expected 15 direct fixtures, got {len(direct_names)}")
    remove_stale_fixtures()

    for f in FIXTURES:
        seed = zlib.crc32(f.name.encode()) & 0xFFFFFFFF
        pixels = source_pixels(f.w, f.h, seed)
        cells, cw, ch = encode_ycbcr_codes(pixels, f.w, f.h, f.hs, f.vs,
                                          f.coeffs_values, f.refbw_values)
        if (cw, ch) != (f.chroma_w, f.chroma_h):
            raise AssertionError(f"{f.name}: chroma geometry mismatch")
        if len(cells) != f.chroma_w * f.chroma_h * f.cell_size:
            raise AssertionError(f"{f.name}: cell byte count mismatch")
        write_fixture(f, cells)

        if f.name in direct_names:
            make_ground_truth(f.name, f.tiff)
        else:
            # libtiff's RGBA converter rejects planar + subsampled YCbCr.
            # The equivalent stores the SAME Y/Cb/Cr codes at 1x1, with the
            # source's odd declared dimensions and no stored-luma padding.
            write_equivalent(f, cells)
            make_ground_truth(f.name, f.equivalent)

    for f in FIXTURES:
        validate_fixture(f)
    print(f"Validated {len(FIXTURES)} fixtures.")

    equivalents = [f.equivalent for f in FIXTURES
                   if not (f.organization == "chunky"
                           or (not f.tiled and f.factor == (1, 1)))]
    if len(equivalents) != 16:
        raise AssertionError(f"expected 16 equivalents, got {len(equivalents)}")
    for path in equivalents:
        if not path.is_file():
            raise ValueError(f"missing equivalent source: {path}")
        check_tiffinfo_clean(path)
    print("tiffinfo clean for 16 equivalent sources.")

    make_dscf0013_ground_truth()

    actual_tifs = set(p.name for p in IMAGE_DIR.glob("ycbcr-*.tif"))
    if actual_tifs != {f.tiff.name for f in FIXTURES}:
        raise AssertionError("unexpected files in tests/ycbcr_images/")
    actual_ppms = set(p.name for p in REFERENCE_DIR.glob("ycbcr-*.ppm"))
    if actual_ppms != {f.ppm.name for f in FIXTURES}:
        raise AssertionError("unexpected YCbCr PPMs in tests/reference_images/")
    print(f"Generated {len(FIXTURES)} fixtures, {len(FIXTURES)} PPMs "
          f"and {len(equivalents)} equivalent sources.")
    print("Fixture breakdown: 29 conversion + 2 color-reference; "
          f"{len(direct_names)} direct and {len(equivalents)} equivalent-source "
          "PPMs.")


if __name__ == "__main__":
    main()
