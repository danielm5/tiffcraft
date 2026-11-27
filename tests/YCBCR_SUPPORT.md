# YCbCr (PhotometricInterpretation = 6) Support Reference

YCbCr is the color encoding carried by TIFF images whose
`PhotometricInterpretation` is 6. This document describes the format rules from
TIFF 6.0 Section 21 and Technical Note 2, and the test images this repository uses
to validate YCbCr decoding. A reader looking for a specific image should go to
**Test image matrix**; a reader asking where the expected pixel values come from
should read **Ground-truth references**.

## Scope

- YCbCr image encoding as defined by TIFF 6.0 Section 21.
- Conversion between RGB and YCbCr.
- Chroma subsampling, positioning, and reference black/white handling.
- Minimum requirements for a valid YCbCr TIFF.
- Mapping of the test images in `tests/ycbcr_images/` to the specification
  topics they exercise.

## Tag definitions

The following TIFF tags describe the color information of an image whose
`PhotometricInterpretation = 6` (YCbCr). They are defined in TIFF 6.0
Section 21.

| Tag | Name | TIFF value | Type | Count | Default | Description |
|---|---|---|---|---|---|---|
| 529 | YCbCrCoefficients | 0x0211 | RATIONAL (5) | 3 | ITU-R BT.601 luma coefficients | Coefficients used for the transformation from RGB to YCbCr. |
| 530 | YCbCrSubsampling | 0x0212 | SHORT (3) | 2 | 2, 2 | Horizontal and vertical subsampling factors of the chroma components. |
| 531 | YCbCrPositioning | 0x0213 | SHORT (3) | 1 | 1 | Positioning of the chroma samples relative to the luminance samples. |
| 532 | ReferenceBlackWhite | 0x0214 | RATIONAL (5) | 6 | (0, NV, 0, NV, 0, NV) | Reference black and white data values (headroom/footroom) per component; NV = 2^BitsPerSample - 1. |

Notes on the individual tags:

- **YCbCrCoefficients (529 / 0x0211):** Type RATIONAL (TIFF data type 5), with
  **3 values** (the RGB -> YCbCr transformation coefficients, e.g. the luma
  row). They are proportions and must sum to 1. Defaults to the CCIR Rec. 601-1
  coefficients (299/1000, 587/1000, 114/1000 for LumaRed, LumaGreen and
  LumaBlue).
- **YCbCrSubsampling (530 / 0x0212):** Type SHORT (TIFF data type 3), **2
  values** — the horizontal subsampling factor and the vertical subsampling
  factor. Default is **2, 2** (chroma subsampled by 2 on both axes). Legal
  factors are **1, 2 and 4**
  (1 = no subsampling, 2 = half resolution, 4 = quarter resolution along the
  axis).
- **YCbCrPositioning (531 / 0x0213):** Type SHORT (TIFF data type 3), **1
  value**. Default is **1 = centered**. The alternative value is **2 = co-sited**
  (chroma sample points aligned with luma sample positions, as used e.g. in
  broadcast 4:2:2 workflows).
- **ReferenceBlackWhite (532 / 0x0214):** Type RATIONAL (TIFF data type 5), **6
  values**. The values are grouped in pairs of (reference black, reference
  white) per pixel component, ordered like the components of the
  `PhotometricInterpretation = 6` image (Y, Cb, Cr). The TIFF default is
  `(0, NV, 0, NV, 0, NV)` with `NV = 2^BitsPerSample - 1` for every component;
  this is inappropriate for YCbCr. The no-headroom/footroom YCbCr profile is
  `(0, 255, 128, 255, 128, 255)` for 8-bit samples, and the CCIR Rec. 601-1
  profile is `(15, 235, 128, 240, 128, 240)`.

## Conversion formulas

### Generalized RGB <-> YCbCr

The conversion is parameterized by the luma coefficients `Lr`, `Lg` and `Lb`
from `YCbCrCoefficients` (tag 529), which satisfy `Lr + Lg + Lb = 1`. Two
helper constants give the chroma scale factors:

- `Lr' = 2 * (1 - Lr)`
- `Lb' = 2 * (1 - Lb)`

All `Y`, `Cb` and `Cr` values below are **full-range** values obtained from
the stored codes by the `ReferenceBlackWhite` scaling described later. The
forward relations are:

- `Y = Lr*R + Lg*G + Lb*B`
- `Cb_full = (B - Y) / Lb'`
- `Cr_full = (R - Y) / Lr'`

They produce zero chroma at neutral; the chroma neutral offset (128 for
8-bit samples) is added when the codes are stored.

The inverse follows by substituting the red and blue relations into
`G = (Y - Lr*R - Lb*B) / Lg` and simplifying with `Lg = 1 - Lr - Lb`:

```text
R = Y + Cr_full * Lr'
B = Y + Cb_full * Lb'
G = (Y - Lr*(Y + Cr_full*Lr') - Lb*(Y + Cb_full*Lb')) / Lg
  = Y - (Lb*Lb'/Lg)*Cb_full - (Lr*Lr'/Lg)*Cr_full
```

**Default coefficients (ITU-R BT.601, the default for tag 529):** `Lr =
0.299`, `Lg = 0.587`, `Lb = 0.114`. Then `Lr' = 1.402`, `Lb' = 1.772`,
`Lr*Lr'/Lg ≈ 0.714136` and `Lb*Lb'/Lg ≈ 0.344136`, giving the familiar

- `R = Y + 1.402*Cr_full`
- `B = Y + 1.772*Cb_full`
- `G = Y - 0.344136*Cb_full - 0.714136*Cr_full`

Under the default reference values the neutral chroma code 128 maps to a
full-range value of zero, so `Cb_full = Cb_code - 128` and `Cr_full =
Cr_code - 128`; the traditional `R = Y + 1.402 * (Cr - 128)` is exactly
this default-coefficient case written with stored codes.

### Reference black/white / headroom and footroom

The `ReferenceBlackWhite` tag supplies, per component, a reference black value
and a reference white value. These define the headroom/footroom range of the
image data: the black value corresponds to the black end of the coding range and
the white value to the white end. The values stored in the image are **codes**;
`ReferenceBlackWhite` defines how those codes scale into full-range values when
converting to RGB.

For each component the forward scaling is:

`FullRangeValue = (code - ReferenceBlack) * CodingRange / (ReferenceWhite - ReferenceBlack)`

and the reverse scaling is:

`code = (FullRangeValue * (ReferenceWhite - ReferenceBlack) / CodingRange) + ReferenceBlack`

`CodingRange` is the range of full-range values: `2^BitsPerSample - 1` for Y and
RGB (255 for 8-bit samples), but **127 for Cb and Cr**. That difference matters:
with the default reference values, chroma code 128 is neutral, codes below it
carry negative chroma and codes above it carry positive chroma; the scaling
reduces to `(code - 128) * 127 / (255 - 128) = code - 128`. In other words
`Cb_full = Cb_code - 128` and `Cr_full = Cr_code - 128` under the defaults. The generalized formulas above consume these full-range values, so
the traditional `R = Y + 1.402 * (Cr - 128)` is exactly the default-coefficient
case (`Lr = 0.299`, hence `Lr' = 1.402`): the `128` is the Cb/Cr
`ReferenceBlack` code in that special case, not a magic number.

For YCbCr images with no headroom/footroom, the commonly used reference values
are black = 0 and white = 255 for luma (Y), and black = 128 and white = 255 for
chroma (Cb/Cr). Merely subtracting 128 is correct only for these values. Images
with headroom/footroom carry other reference values: for example, with
`ReferenceBlackWhite = [15, 235, 128, 240, 128, 240]` a luma code of 235 maps to
`255` and a luma code of 15 maps to `0`, using
`(code - 15) * 255 / (235 - 15)`. The chroma white reference is 240 rather than
255, so subtracting 128 alone leaves chroma under-scaled. Readers must apply the
tag's scaling instead of assuming fixed values.

The TIFF 6.0 specification says the generic `ReferenceBlackWhite` default is
inappropriate for Class Y images and that the tag must be supplied explicitly.
In practice many images omit it, so readers commonly fall back to the
no-headroom/footroom YCbCr values above (Y 0/255, Cb/Cr 128/255), as does
libtiff's JPEG subset.

## Subsampling

- The `YCbCrSubsampling` factors describe how the chroma components are
  subsampled relative to the luma component. Both Cb and Cr use the same
  factors, and the vertical factor must not exceed the horizontal factor.
- Legal subsampling factors are **1, 2 and 4**. The spec default is **2, 2** —
  YCbCr is subsampled by default; an image without tag 530 is not implicitly
  1x1. TiffCraft's YCbCr exporter (`TiffExporterYCbCr`, dispatched by
  `TiffExporterAny`) handles the spec-legal combinations `[1,1]`, `[2,1]`,
  `[2,2]`, `[4,1]`, `[4,2]` and `[4,4]`, for both chunky (cell-interleaved)
  and separate-plane images, stripped or tiled. When tag 530 is absent the
  spec default `[2,2]` is used.
- All TIFF dimensions (`ImageWidth`, `ImageLength`, `RowsPerStrip`,
  `TileWidth`, `TileLength`) are measured in luma samples. TIFF 6.0 originally
  required image dimensions to be multiples of the subsampling factors;
  Technical Note 2 relaxes this: the source is padded to the next multiple
  before downsampling, so the number of stored chroma samples is the **ceiling**
  of the luma dimension divided by the factor, and readers ignore the extra
  samples after upsampling. That is the source of the ceiling rule used by the
  odd-dimension test images below.
- `RowsPerStrip` must be an integer multiple of the vertical subsampling factor
  (unless the image is a single strip), and for tiled images `TileWidth` and
  `TileLength` must be multiples of the horizontal and vertical factors.
- With `PlanarConfig = 1`, samples are stored in data units: a block of
  `horizontal x vertical` Y samples followed by one Cb and one Cr sample.

## Positioning

- `YCbCrPositioning = 1` (default): **centered** positioning of the chroma
  samples with respect to the luminance samples.
- `YCbCrPositioning = 2`: **co-sited** positioning (chroma sample points aligned
  with luma sample points, e.g. recommended for 4:2:2 workflows).

libtiff 4.7.2 (the version used for the references in this repository) stores
this tag but never consults it while decoding: rendering the same TIFF with
`tiff2rgba` is byte-for-byte identical for values 1 and 2.
TiffCraft likewise reads `YCbCrPositioning` but does not use it during
conversion, so co-sited images are decoded with centered chroma placement.
The test image set therefore contains no co-sited image, since the tag would have
no effect on libtiff's decoded pixels.

## Minimum requirements for YCbCr images

In addition to the general Baseline TIFF requirements, a YCbCr file must have:

- `SamplesPerPixel = 3` (Y, Cb, Cr).
- `BitsPerSample = 8, 8, 8`.
- `Compression = 1` (none), `5` (LZW) or JPEG: TIFF 6.0 listed `6`, which
  Technical Note 2 replaced with `7` for new-style JPEG.
- `PhotometricInterpretation = 6` (YCbCr).
- `PlanarConfig = 1` (chunky) or `2` (separate).
- `YCbCrCoefficients`: required if the RGB -> YCbCr conversion is not the
  CCIR Rec. 601-1 default.
- `ReferenceBlackWhite`: required by the TIFF 6.0 specification. Readers must
  nevertheless handle its absence by falling back to the no-headroom/footroom
  YCbCr values above (Y 0/255, Cb/Cr 128/255), because many real-world files
  omit tag 532.

### Terms used in the test image sections

- **Declared dimensions** — `ImageWidth` and `ImageLength`: the size of the
  visible image, which may be odd.
- **Stored luma** — the padded Y sample grid permitted by Technical Note 2. For
  stripped images the stored size is the declared size rounded up to a multiple
  of the subsampling factors; for tiled images it is rounded up to the tile grid.
  Readers crop it after upsampling.
- **Chroma cell** — the block of `hs x vs` luma positions that share one Cb/Cr
  pair.
- **Chroma stride** — the number of stored samples in one chroma row. A packed
  row holds `ceil(w/hs)` real samples; a padded (full-width) row allocates `w`
  samples, with real samples in the first `ceil(w/hs)` columns and
  edge-replicated values in the rest. The dimension table below counts **real**
  chroma samples; the stride describes how they are stored.
- **Equivalent TIFF** — a planar 1x1 TIFF carrying the same Y/Cb/Cr codes as a
  test image, used to obtain a libtiff reference when a direct decode is
  unavailable or, for tiled planar images, kept for route uniformity.
- **Headroom/footroom** — the codes reserved beyond nominal black and white;
  `ReferenceBlackWhite` maps those boundaries to full-range values.
- **Class Y** — TIFF's term for images whose components carry luma and chroma
  information, including YCbCr.
- **Genuine conversion test** — a test image whose tags and pixel data describe
  the image they claim to represent, so decoding it is a real conversion rather
  than a tag-reading check.
- **Origin luma pixel** — the top-left luma position of a subsampling cell in
  the stored image.
- **Padded edges** — the stored positions beyond the declared image width or
  height, where the nearest in-image sample is replicated.

## test image set

`tests/ycbcr_images/` contains 31 YCbCr test images, each with a ground-truth PPM
in `tests/reference_images/`:

- 6 chunky, stripped conversion test images.
- 6 planar packed, stripped conversion test images (including the packed 1x1).
- 5 planar padded, stripped conversion test images.
- 6 tiled chunky conversion test images.
- 6 tiled planar conversion test images.
- 2 color-reference test images with non-default coefficient or reference-level
  tags.

All 31 images, including the two color references, are generated by
`tests/ycbcr_images/generate_ycbcr.py`. The 29 conversion images use odd
declared dimensions; the color references are generated as 64x48 chunky 1x1
images (even dimensions, exempt from the odd-dimension convention) with
non-default tags that are written into the same encoded pixel data libtiff
decodes.

Stripped planar test images use `ycbcr-planar-{packed|padded}-{factor}.tif`, with
`packed-1x1` for the only 1x1 planar case; tiled planar test images use
`ycbcr-planar-tiled-{factor}.tif`.

Every conversion test image is genuine: its tags and pixel data describe an
image whose RGB reference comes from libtiff decoding, directly or through an
equivalent planar-1x1 TIFF. The test images cover
the subsampling factors `[1,1]`, `[2,1]`, `[2,2]`, `[4,1]`, `[4,2]` and `[4,4]`;
no other factor combinations are present. All 31 generated images are exercised
by both `TiffExporterYCbCr<uint8_t>` and `TiffExporterAny` in
`tests/tiffExporterTest.cpp`; the same tests also cover
`tests/libtiff-pics/dscf0013.tif`.

## Generating the test images

From the repository root:

```sh
python3 tests/ycbcr_images/generate_ycbcr.py
```

The generator writes test images to `tests/ycbcr_images/*.tif`, P6 references to
`tests/reference_images/ycbcr-*.ppm`, and the equivalent source TIFFs to
`tests/reference_images/ycbcr-*-equiv.tif`.

The generator uses only the Python 3 standard library plus the libtiff and
netpbm command-line tools (`tiff2rgba`, `tifftopnm` and `tiffinfo`). No imaging
library synthesizes or converts pixels. The dscf0013 references are produced by
the in-repo C helper `tests/ycbcr_images/dscf0013_ppm.c`, which the generator
compiles with `cc -I/usr/local/include -L/usr/local/lib ... -ltiff`; a C
compiler and libtiff development headers are therefore required to regenerate
them. Each TIFF is written directly with `struct`-based tag and data layouts
using:

- `BitsPerSample = 8, 8, 8`, `SamplesPerPixel = 3`,
  `PhotometricInterpretation = 6`, `Compression = 1`.
- `PlanarConfiguration = 1` (chunky) or `2` (separate planes).
- Little-endian ClassicTIFF (`II*\0`), `FillOrder = 1` (msb-to-lsb),
  `YCbCrPositioning = 1` (centered).
- BT.601 `YCbCrCoefficients` (0.299 / 0.587 / 0.114) and default
  `ReferenceBlackWhite` values (Y 0/255, Cb/Cr 128/255) for the 29 conversion
  test images. The two generated color references declare their own non-default
  values — BT.709 0.2126 / 0.7152 / 0.0722, and limited-range
  `ReferenceBlackWhite` (15,235), (128,240), (128,240) — and the encoder uses
  those same values for the pixel data.
- `RowsPerStrip = 16` for stripped images; `TileWidth = TileLength = 16` and
  no `RowsPerStrip` for tiled images.

The in-repo C helper `tests/ycbcr_images/ycbcr_scanline_dump.c` is provided for
raw-sample inspection and is not part of reference generation. The companion
helper `tests/ycbcr_images/dscf0013_ppm.c` is part of reference generation: it
walks the TIFF's IFDs and writes one P6 PPM per IFD.

### Source pattern

Every test image is generated from a deterministic pattern. For a test image with
declared size `W x H` and base name `name` (the file name without `.tif`):

- `seed = zlib.crc32(name.encode()) & 0xFFFFFFFF`
- Six vertical stripes `i = 0..5`; stripe `i` covers columns
  `[floor(W*i/6), floor(W*(i+1)/6))`, so stripe widths differ by at most one
  column.
- Stripe `i` has a constant RGB color, identical in every row:
  - `R_i = (seed*(i+1) + 11) % 256`
  - `G_i = (seed*(i+3) + 29) % 256`
  - `B_i = (seed*(i+5) + 47) % 256`

The stored Y/Cb/Cr codes are the exact inverse of libtiff's `TIFFYCbCrtoRGB`
transform applied to those RGB pixels, with a coding range of 255 for Y and
127 for Cb/Cr and round-half-up quantization to 8-bit codes. This inverse is
what makes libtiff reproduce the source RGB when it decodes the test image or
its equivalent.
Chroma is sampled at the origin luma pixel of each subsampling cell; padding
outside the declared area edge-replicates the nearest in-image sample.

### Dimensions, padding and strip/tile layout

Declared `ImageWidth`/`ImageLength` are odd for the 29 conversion test images.
The two generated color references are exempt: they declare 64x48 with 1x1
subsampling, so no padding is needed. Stored luma is padded up to the next
multiple of each subsampling factor; chroma dimensions are
`ceil(declared / factor)`:

| Factor | Declared WxH | Stored luma WxH | Chroma WxH |
|---|---|---|---|
| 1x1 | 63x47 | 63x47 | 63x47 |
| 2x1 | 63x47 | 64x47 | 32x47 |
| 2x2 | 61x45 | 62x46 | 31x23 |
| 4x1 | 61x47 | 64x47 | 16x47 |
| 4x2 | 57x43 | 60x44 | 15x22 |
| 4x4 | 63x61 | 64x64 | 16x16 |

Stripped images use `RowsPerStrip = 16`, which is a multiple of the vertical
factor. Their stored-luma strip row counts are:

| Factor | Strips | Row counts |
|---|---|---|
| 1x1 | 3 | 16, 16, 15 |
| 2x1 | 3 | 16, 16, 15 |
| 2x2 | 3 | 16, 16, 14 |
| 4x1 | 3 | 16, 16, 15 |
| 4x2 | 3 | 16, 16, 12 |
| 4x4 | 4 | 16, 16, 16, 16 |

Tiled images have no `RowsPerStrip`. Only the stored tile area is padded to the
tile grid: factors 1x1, 2x1, 2x2, 4x1 and 4x2 store 64x48 luma as a 4x3 grid of
12 tiles of 16x16 luma samples; 4x4 stores 64x64 as a 4x4 grid of 16 tiles.
Boundary tiles are edge-replicated. Chroma tile sizes apply to planar-tiled
images: 1x1 → 16x16, 2x1 → 8x16, 2x2 → 8x8, 4x1 → 4x16, 4x2 → 4x8,
4x4 → 4x4. Chunky-tiled images interleave chroma inside their luma tiles and
have no separate chroma tile grid.

### Planar chroma layouts

Separate-plane test images store genuine Y, Cb and Cr planes. Chroma rows use one
of two spec-conformant layouts:

- `-packed`: each chroma row holds exactly `ceil(w/hs)` real samples, with no
  padding.
- `-padded`: each chroma row is padded out to the full declared width `w`, with
  real samples in the first `ceil(w/hs)` columns and edge-replicated values in
  the rest.

A *full-width stride* allocates `w` samples per chroma row. The `-padded` test
images use it; the `-packed` test images hold only the real samples, with the
same chroma values; every test image's source pattern is seeded from its own
file name. Stripped planar test images are named
`ycbcr-planar-{stride}-{factor}.tif` (the 1x1 case has only `packed`); tiled
planar test images are named `ycbcr-planar-tiled-{factor}.tif`. A reader uses only
the first `ceil(w/hs)` samples of each chroma row and ignores any padding.

### Structural validation

The generator's `validate_fixture` routine parses each test image's IFD with
`struct`, checks tag types, counts and values, verifies declared dimensions and
strip/tile array lengths and byte counts for both chroma strides, and rejects
any `tiffinfo` warning or error. It applies to all 31 generated test images in
`tests/ycbcr_images/` (29 conversion + 2 color-reference): the color references
are checked against their own coefficients/reference levels and are exempt from
the odd-dimension convention. The subsampling gate remains: only the six legal
factor pairs above are emitted, with the vertical factor never exceeding the
horizontal one.

## Test image matrix

All files below are in `tests/ycbcr_images/`.

### Chunky, stripped (6)

| Test image | Subsampling | Validates |
|---|---|---|
| `ycbcr-chunky-1x1.tif` | 1x1 | Full-resolution chunky YCbCr with `YCbCrSubsampling` explicitly 1x1, the other tags at defaults and 16-row strips. |
| `ycbcr-chunky-2x1.tif` | 2x1 | Horizontal-only chroma subsampling. |
| `ycbcr-chunky-2x2.tif` | 2x2 | Half-resolution chroma on both axes. |
| `ycbcr-chunky-4x1.tif` | 4x1 | Quarter-resolution horizontal chroma. |
| `ycbcr-chunky-4x2.tif` | 4x2 | Mixed 4x2 chroma grid. |
| `ycbcr-chunky-4x4.tif` | 4x4 | Quarter-resolution chroma on both axes. |

### Planar packed, stripped (6)

| Test image | Subsampling | Chroma row layout | Validates |
|---|---|---|---|
| `ycbcr-planar-packed-1x1.tif` | 1x1 | Packed | Genuine separate planes at full resolution with 16-row strips. |
| `ycbcr-planar-packed-2x1.tif` | 2x1 | Packed | `ceil(w/2)` real chroma samples per row. |
| `ycbcr-planar-packed-2x2.tif` | 2x2 | Packed | `ceil(w/2)` real chroma samples per row. |
| `ycbcr-planar-packed-4x1.tif` | 4x1 | Packed | `ceil(w/4)` real chroma samples per row. |
| `ycbcr-planar-packed-4x2.tif` | 4x2 | Packed | `ceil(w/4)` real chroma samples per row. |
| `ycbcr-planar-packed-4x4.tif` | 4x4 | Packed | `ceil(w/4)` real chroma samples per row. |

### Planar padded, stripped (5)

| Test image | Subsampling | Chroma row layout | Validates |
|---|---|---|---|
| `ycbcr-planar-padded-2x1.tif` | 2x1 | Padded | Full-width chroma rows with padding after the real samples. |
| `ycbcr-planar-padded-2x2.tif` | 2x2 | Padded | Full-width chroma rows with padding after the real samples. |
| `ycbcr-planar-padded-4x1.tif` | 4x1 | Padded | Full-width chroma rows with padding after the real samples. |
| `ycbcr-planar-padded-4x2.tif` | 4x2 | Padded | Full-width chroma rows with padding after the real samples. |
| `ycbcr-planar-padded-4x4.tif` | 4x4 | Padded | Full-width chroma rows with padding after the real samples. |

### Tiled chunky (6)

| Test image | Subsampling | Validates |
|---|---|---|
| `ycbcr-chunky-tiled-1x1.tif` | 1x1 | 16x16 luma tiles with interleaved chroma, no `RowsPerStrip`. |
| `ycbcr-chunky-tiled-2x1.tif` | 2x1 | Interleaved chroma in 16x16 tiles with 2x1 subsampling. |
| `ycbcr-chunky-tiled-2x2.tif` | 2x2 | Interleaved chroma in 16x16 tiles with 2x2 subsampling. |
| `ycbcr-chunky-tiled-4x1.tif` | 4x1 | Interleaved chroma in 16x16 tiles with 4x1 subsampling. |
| `ycbcr-chunky-tiled-4x2.tif` | 4x2 | Interleaved chroma in 16x16 tiles with 4x2 subsampling. |
| `ycbcr-chunky-tiled-4x4.tif` | 4x4 | 4x4 tile grid with 4x4 subsampling in interleaved tiles. |

### Tiled planar (6)

| Test image | Subsampling | Validates |
|---|---|---|
| `ycbcr-planar-tiled-1x1.tif` | 1x1 | Separate 16x16 luma and 16x16 chroma tile grids. |
| `ycbcr-planar-tiled-2x1.tif` | 2x1 | Separate planes with 8x16 chroma tiles. |
| `ycbcr-planar-tiled-2x2.tif` | 2x2 | Separate planes with 8x8 chroma tiles. |
| `ycbcr-planar-tiled-4x1.tif` | 4x1 | Separate planes with 4x16 chroma tiles. |
| `ycbcr-planar-tiled-4x2.tif` | 4x2 | Separate planes with 4x8 chroma tiles. |
| `ycbcr-planar-tiled-4x4.tif` | 4x4 | Separate planes with 4x4 chroma tiles on a 4x4 luma grid. |

### Color-reference test images (2)

| Test image | Attribute | Validates |
|---|---|---|
| `ycbcr-color-coeffs-bt709.tif` | 64x48, 1x1, `RowsPerStrip = 16`, `YCbCrCoefficients = 0.2126 / 0.7152 / 0.0722` (BT.709) | Generated chunky 1x1 test image whose BT.709 coefficients (tag 529) are honestly applied to its pixel data; decoded directly by libtiff. |
| `ycbcr-color-refbw-limited-range.tif` | 64x48, 1x1, `RowsPerStrip = 16`, `ReferenceBlackWhite = (15,235), (128,240), (128,240)` | Generated chunky 1x1 test image whose headroom/footroom levels (tag 532) are honestly applied to its pixel data; decoded directly by libtiff. |

## Ground-truth references

libtiff's `tiff2rgba` decodes chunky YCbCr and planar 1x1 YCbCr. Its RGBA path
has no separate-plane decoder for subsampled chroma (`PickSeparateCase()` in
`tif_getimage.c` selects `put.separate` only for 1x1), so all planar subsampled
test images take the equivalent route below. Of those, the `-padded` images use
a libtiff-compatible layout — full-width chroma rows whose first `ceil(w/hs)`
samples are the real chroma and whose remaining bytes are padding a reader can
ignore — while the `-packed` and tiled planar layouts are not libtiff-compatible.
Every reference pixel value in this repository comes from libtiff through one
of two routes. The following rule is mandatory:

- All reference pixel values are generated with libtiff decode functions only.
- `tiff2rgba` + `tifftopnm` for direct conversion.
- No Python, Pillow, ImageMagick, or TiffCraft code may convert YCbCr samples
  to RGB for a reference.
- Where libtiff cannot convert a test image directly, generate an equivalent
  TIFF from the same Y/Cb/Cr codes of the test image that libtiff can convert,
  and take its `tiff2rgba` output as the reference.

`tests/reference_images/ycbcr-*.ppm` holds one binary P6 PPM per generated test
image (31 total). Each P6 header matches the test image's declared dimensions.
The two routes below cover all 31 generated test images: 15 direct decodes (13
conversion + 2 color-reference) and 16 equivalent-source decodes. The 2
`dscf0013` references are produced separately by the libtiff C helper, as
described at the end of this section.

- **Direct route (15 test images: 13 conversion + 2 color-reference):**
  `tiff2rgba -n -c none <tif> <rgba.tif>` followed by
  `tifftopnm <rgba.tif> > <name>.ppm`, applied to the test image itself. This
  covers the 6 chunky stripped test images, `ycbcr-planar-packed-1x1.tif`, the
  6 tiled chunky test images, and the two generated color references at 64x48
  (chunky 1x1), whose PPMs honor their non-default tags.
- **Equivalent route (16 conversion test images):** for the 5 planar packed and
  5 planar padded subsampled stripped test images and the 6 tiled planar test
  images, the generator writes an equivalent planar-1x1 TIFF from the test
  image's own Y/Cb/Cr codes with chroma replicated to full resolution, then
  decodes that file with the same `tiff2rgba` + `tifftopnm` chain. For `-padded`
  sources only the first `ceil(w/hs)` real chroma samples of each row are used;
  padding bytes are ignored.

Each chroma cell `(cx, cy)` supplies its Cb/Cr to luma positions
`[cx*hs, (cx+1)*hs) x [cy*vs, (cy+1)*vs)` within the declared dimensions, with
the outermost cell edge-replicated at padded edges. This nearest-cell
replication matches libtiff's own decode, so the equivalent reproduces
libtiff's upsampling. An equivalent declares the source test image's odd
`ImageWidth`/`ImageLength` and stores its 1x1 luma and chroma at the unpadded
declared sizes; the padded stored-luma sizes in **Dimensions, padding and
strip/tile layout** do not apply because a 1x1 equivalent has no subsampling
padding. Tiled equivalents use the source's
16x16 luma tile grid (4x3 or 4x4). The tiled planar 1x1 test image is also handled
by this route for uniformity: libtiff can decode it directly, but no chroma
replication is needed and the equivalent carries the same 16x16 tile grid and
odd declared dimensions. The equivalents are ground-truth sources in
`tests/reference_images/`, not test images.

The equivalent source TIFFs are:

- Planar packed, stripped (5): `ycbcr-planar-packed-2x1-equiv.tif`,
  `ycbcr-planar-packed-2x2-equiv.tif`, `ycbcr-planar-packed-4x1-equiv.tif`,
  `ycbcr-planar-packed-4x2-equiv.tif`, `ycbcr-planar-packed-4x4-equiv.tif`
- Planar padded, stripped (5): `ycbcr-planar-padded-2x1-equiv.tif`,
  `ycbcr-planar-padded-2x2-equiv.tif`, `ycbcr-planar-padded-4x1-equiv.tif`,
  `ycbcr-planar-padded-4x2-equiv.tif`, `ycbcr-planar-padded-4x4-equiv.tif`
- Tiled planar (6): `ycbcr-planar-tiled-1x1-equiv.tif`,
  `ycbcr-planar-tiled-2x1-equiv.tif`, `ycbcr-planar-tiled-2x2-equiv.tif`,
  `ycbcr-planar-tiled-4x1-equiv.tif`, `ycbcr-planar-tiled-4x2-equiv.tif`,
  `ycbcr-planar-tiled-4x4-equiv.tif`

The color-reference test images `ycbcr-color-coeffs-bt709.tif` and
`ycbcr-color-refbw-limited-range.tif` are generated by the script at 64x48 as
chunky 1x1 TIFFs with non-default tags (BT.709 coefficients and limited-range
`ReferenceBlackWhite`, respectively). Their PPM references in
`tests/reference_images/` come from a direct libtiff decode of each test image
itself, so the references honor the non-default tags; no equivalent TIFF and no
patch-after-encode step is involved.

`tests/libtiff-pics/dscf0013.tif` has two YCbCr IFDs. Its references are
`tests/reference_images/dscf0013.ppm` for the 640x480 IFD and
`tests/reference_images/dscf0013-ifd1.ppm` for the 160x120 thumbnail. The C
helper `tests/ycbcr_images/dscf0013_ppm.c` walks the original IFDs with
`TIFFReadDirectory` and decodes each one with
`TIFFReadRGBAImageOriented(..., ORIENTATION_TOPLEFT, 0)`, writing one P6 PPM per
IFD. This keeps YCbCrSubsampling and YCbCrPositioning intact, so both references
begin with the RGB pixel `(5, 1, 2)`; no IFD-splitting tool is used.
