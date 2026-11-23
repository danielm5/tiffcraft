# YCbCr (PhotometricInterpretation = 6) Support Reference

This document describes YCbCr (`PhotometricInterpretation = 6`) images as
defined by Section 21 of the TIFF 6.0 specification.

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
with the default reference values Cb/Cr run from code 128 to code 255, so the
chroma scaling reduces to `(code - 128) * 127 / (255 - 128) = code - 128`. In
other words `Cb_full = Cb_code - 128` and `Cr_full = Cr_code - 128` under the
defaults. The generalized formulas above consume these full-range values, so
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
  1x1. TiffCraft's exporter currently supports only explicit 1x1 subsampling,
  so files that omit the tag are rejected rather than treated as full
  resolution.
- All TIFF dimensions (`ImageWidth`, `ImageLength`, `RowsPerStrip`,
  `TileWidth`, `TileLength`) are measured in luma samples. TIFF 6.0 originally
  required image dimensions to be multiples of the subsampling factors;
  Technical Note 2 relaxes this: the source is padded to the next multiple
  before downsampling, so the number of stored chroma samples is the **ceiling**
  of the luma dimension divided by the factor, and readers ignore the extra
  samples after upsampling. That is the source of the ceiling rule used by the
  odd-dimension fixtures below.
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

libtiff 4.7.2 stores this tag but never consults it while decoding: rendering
the same TIFF with `tiff2rgba` is byte-for-byte identical for values 1 and 2.
The fixture set therefore contains no co-sited image, since the tag would have
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

## Generating the test images

`tests/ycbcr_images/generate_ycbcr.py` recreates the fixture set. It requires
Python 3, gcc with libtiff development headers, and the libtiff and netpbm
command-line tools (`pnmtotiff`, `rgb2ycbcr`, `tiffset`, `tiffcp`, `tiff2rgba`,
`tifftopnm`). From the repository root, run:

```sh
python3 tests/ycbcr_images/generate_ycbcr.py
```

The pipeline for each fixture is:

1. Write a deterministic binary P6 source PPM in pure Python (standard-library
   `bytes` and `zlib.crc32`; no external imaging library).
2. `pnmtotiff < src.ppm > src.tif` creates an RGB TIFF.
3. `rgb2ycbcr -c none -h H -v V -r HEIGHT src.tif out.tif` writes the YCbCr
   image. This writes all four YCbCr tags (529, 530, 531, 532) with the BT.601
   coefficients, centered positioning and default reference values.
4. Non-default tag values are produced together with their pixel data, not
   patched onto an unrelated image. `ycbcr-custom-coeffs.tif` and
   `ycbcr-custom-refbw.tif` are built by a pure-Python encoder and TIFF writer:
   the encoder applies the inverse of libtiff's `TIFFYCbCrtoRGB` transform to
   the deterministic source pixels using exactly the values written into
   `YCbCrCoefficients` (BT.709 luma) or `ReferenceBlackWhite` (headroom/
   footroom), and the writer writes those tag values into the TIFF itself.
   Separate-plane coverage for `ycbcr-planar-1x1.tif` uses `tiffcp -p separate`
   followed by restoring `PhotometricInterpretation` and the four YCbCr tags
   with `tiffset`, giving real separate-plane data. `ycbcr-planar-2x2.tif` and
   `ycbcr-tiled-2x2.tif` are written by pure-Python `struct`-based TIFF
   writers: the first de-interleaves the cell-interleaved chunky strip into
   three real Y/Cb/Cr planes with consistent strip arrays, and the second lays
   the same stripped 2x2 cell data out as 16x16 tiles in a 4x3 grid (12 tiles
   of 384 bytes) with all four YCbCr tags and `PhotometricInterpretation = 6`
   written into the tiled TIFF itself. `tiffcp` is not used for tiled output:
   it refuses subsampled input.
5. Every ground-truth P6 PPM is produced by libtiff decoding the SAME TIFF it
   belongs to. For 14 fixtures that is `tiff2rgba -n -c none <tif> <rgba.tif>`
   followed by `tifftopnm <rgba.tif> > ycbcr-*.ppm`. For `ycbcr-1x4.tif`,
   `ycbcr-2x4.tif` and `ycbcr-planar-2x2.tif`, libtiff's `TIFFRGBAImage` /
   `tiff2rgba` path rejects the image (4x vertical subsampling, or planar +
   subsampled), while `TIFFReadScanline` decodes their pixels. The generator
   compiles the in-repo C helper
   `tests/ycbcr_images/ycbcr_scanline_dump.c`, runs it on each final TIFF, and
   converts the libtiff scanline bytes to RGB with the TIFF's own
   `YCbCrCoefficients` and `ReferenceBlackWhite` values, using nearest-cell
   chroma replication and writing the pixels in PPM raster order. No reference
   is ever rendered from an equivalent or substitute image.
6. Every emitted TIFF is checked by the generator's `validate_fixture`
   routine: it parses the IFD with `struct`, verifies the YCbCr tags and
   values, checks the strip/tile array lengths against the image geometry
   (catching inconsistent planar strip arrays), and rejects any `tiffinfo`
   warning or error.

The 17 fixtures are described in the matrix below. Every fixture is a
conversion test: its tag values and pixel data describe the same image. The
two non-default-tag variants encode the source pixels with the coefficients or
reference values they carry; the two planar fixtures hold genuinely separate
planes; and the tiled fixture holds genuine 2x2-subsampled tile data.

## Test image matrix

All files are in `tests/ycbcr_images/`.

### Subsampling fixtures

Nine 64x48 images with default BT.601 coefficients, centered positioning,
default reference values, 8-bit contiguous planes and no compression. Each
carries a real `YCbCrSubsampling` value written by `rgb2ycbcr`.

| Test image | Subsampling | Validates |
|---|---|---|
| `ycbcr-1x1.tif` | 1x1 | Full-resolution YCbCr with all four tags at their defaults; stripped organization. |
| `ycbcr-2x1.tif` | 2x1 | Horizontal-only chroma subsampling. |
| `ycbcr-1x2.tif` | 1x2 | Vertical-only chroma subsampling. |
| `ycbcr-2x2.tif` | 2x2 | Half-resolution chroma on both axes. |
| `ycbcr-4x1.tif` | 4x1 | Quarter-resolution horizontal chroma. |
| `ycbcr-1x4.tif` | 1x4 | Quarter-resolution vertical chroma. |
| `ycbcr-4x2.tif` | 4x2 | Mixed 4x2 chroma grid. |
| `ycbcr-2x4.tif` | 2x4 | Mixed 2x4 chroma grid. |
| `ycbcr-4x4.tif` | 4x4 | Quarter-resolution chroma on both axes. |

### Variant fixtures

Five 64x48 images with one non-default attribute each. The two non-default-tag
fixtures encode their source pixels with the tag values they carry, the two
planar fixtures hold genuine separate planes, and the tiled fixture holds
genuine tiled, subsampled pixel data.

| Test image | Attribute | Validates |
|---|---|---|
| `ycbcr-custom-coeffs.tif` | Coefficients = 0.2126/0.7152/0.0722 on a 1x1 base | Non-default luma coefficients (tag 529) applied to the pixel data; libtiff's decoded PPM differs from the source by a mean of 0.33 per channel. |
| `ycbcr-custom-refbw.tif` | RefBW = 15/235/128/240/128/240 on a 1x1 base | Headroom/footroom reference values (tag 532) applied to the pixel data; libtiff's decoded PPM differs from the source by a mean of 0.84 per channel. |
| `ycbcr-planar-1x1.tif` | PlanarConfig = 2, separate planes on a 1x1 base | Genuine separate planes: Y, Cb and Cr each in their own strips (6 strips total), with all four YCbCr tags. |
| `ycbcr-planar-2x2.tif` | PlanarConfig = 2, separate planes on a 2x2 base | Genuine separate planes: Y, Cb and Cr each in their own strips (3 strips total), with consistent strip arrays and all four YCbCr tags. |
| `ycbcr-tiled-2x2.tif` | 16x16 tiles, 4x3 grid (12 tiles of 384 bytes), real 2x2 subsampling | Tile tag path with genuinely subsampled tiled YCbCr data and all four tags; its PPM matches the stripped 2x2 reference byte-for-byte. |

`ycbcr-tiled-2x2.tif` is a genuine conversion fixture: a pure-Python TIFF tile
writer lays the stripped 2x2 cell-interleaved strip out into 16x16 luma tiles,
with all four YCbCr tags and `PhotometricInterpretation = 6` written into the
tiled TIFF itself. `tiffcp` cannot tile subsampled input, so it is not used
for this fixture.

### Odd-dimension fixtures

Three images whose width and height are not multiples of the subsampling
factors, so the chroma dimensions use the ceiling rule. Tags and pixel data
agree.

| Test image | Dimensions | Subsampling | Validates |
|---|---|---|---|
| `ycbcr-odd-2x2.tif` | 61x47 | 2x2 | Ceiling of odd luma dimensions for 2x2 chroma. |
| `ycbcr-odd-4x4.tif` | 63x61 | 4x4 | Ceiling of odd luma dimensions for 4x4 chroma. |
| `ycbcr-odd-mixed.tif` | 62x47 | 2x1 | Ceiling on one axis only for mixed subsampling. |

## Ground-truth reference images

`tests/reference_images/ycbcr-*.ppm` holds the RGB ground-truth images
corresponding to the 17 YCbCr test images, in binary Netpbm PPM (P6) format.
Every reference is produced by libtiff decoding the SAME TIFF fixture it
belongs to: 14 with `tiff2rgba` followed by `tifftopnm`, and three with the
in-repo libtiff `TIFFReadScanline` helper
`tests/ycbcr_images/ycbcr_scanline_dump.c` (`ycbcr-1x4.tif`, `ycbcr-2x4.tif`
and `ycbcr-planar-2x2.tif`). The helper's raw YCbCr bytes are converted to RGB
with each TIFF's own coefficients and reference values, using nearest-cell
chroma replication and PPM raster order. `tiff2rgba` cannot decode those three
images (4x vertical subsampling, or planar + subsampled), but
`TIFFReadScanline` decodes their pixels correctly. No reference is rendered
from an equivalent or substitute image. Each P6 header matches the dimensions
of its TIFF (64x48, or the odd dimensions 61x47, 63x61 and 62x47).
