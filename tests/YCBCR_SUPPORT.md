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
| 529 | YCbCrCoefficients | 0x0211 | RATIONAL (5) / SRATIONAL (10) | 3 | ITU-R BT.601 luma coefficients | Coefficients used for the transformation from RGB to YCbCr. |
| 530 | YCbCrSubsampling | 0x0212 | SHORT (3) | 2 | 2, 2 | Horizontal and vertical subsampling factors of the chroma components. |
| 531 | YCbCrPositioning | 0x0213 | SHORT (3) | 1 | 1 | Positioning of the chroma samples relative to the luminance samples. |
| 532 | ReferenceBlackWhite | 0x0214 | RATIONAL (5) | 6 | (0, 255, 128, 255, 128, 255) | Reference black and white data values (headroom/footroom) per component. |

Notes on the individual tags:

- **YCbCrCoefficients (529 / 0x0211):** Type RATIONAL (TIFF data type 5) or
  SRATIONAL (TIFF data type 10), with **3 values** (the RGB -> YCbCr
  transformation coefficients, e.g. the luma row). Defaults to the ITU-R BT.601
  coefficients (approximately 0.299, 0.587, 0.114 for the red, green and blue
  contributions to luma).
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
  `PhotometricInterpretation = 6` image (Y, Cb, Cr). Default value set:
  Y black/white = 0/255, Cb black/white = 128/255, Cr black/white = 128/255.

## Conversion formulas

### Forward conversion (RGB -> YCbCr), ITU-R BT.601

- `Y  = 0.299 * R + 0.587 * G + 0.114 * B`
- `Cb = -0.168736 * R - 0.331264 * G + 0.5 * B + 128`
- `Cr = 0.5 * R - 0.418688 * G - 0.081312 * B + 128`

### Reverse conversion (YCbCr -> RGB), ITU-R BT.601

- `R = Y + 1.402 * (Cr - 128)`
- `G = Y - 0.344136 * (Cb - 128) - 0.714136 * (Cr - 128)`
- `B = Y + 1.772 * (Cb - 128)`

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
chroma scaling reduces to `(code - 128) * 127 / (255 - 128) = code - 128`. This
is why the no-headroom case amounts to simply subtracting 128, and why the `128`
in `R = Y + 1.402 * (Cr - 128)` is the `ReferenceBlack` value for Cb/Cr in the
no-headroom case, not a magic number.

For YCbCr the default reference values are black = 0 and white = 255 for the
luma component (Y), and black = 128 and white = 255 for the chroma components
(Cb/Cr). Merely subtracting 128 is correct only for these defaults. Images with
headroom/footroom carry other reference values: for example, with
`ReferenceBlackWhite = [15, 235, 128, 240, 128, 240]` a luma code of 235 maps to
`255` and a luma code of 15 maps to `0`, using
`(code - 15) * 255 / (235 - 15)`. The chroma white reference is 240 rather than
the default 255, so subtracting 128 alone leaves chroma under-scaled. Readers
must apply the tag's scaling instead of assuming the defaults.

The TIFF 6.0 specification says the default `ReferenceBlackWhite` values are
inappropriate for Class Y images and that the tag must be supplied explicitly.
In practice many images omit it, so readers must fall back to the YCbCr-specific
defaults above (Y 0/255, Cb/Cr 128/255).

## Subsampling

- The `YCbCrSubsampling` factors describe how the chroma components are
  subsampled relative to the luma component.
- Legal subsampling factors are **1, 2 and 4**.
- The chroma image dimensions are computed by rounding **up** (ceiling) the
  luma dimensions divided by the subsampling factor, so that no chroma sample is
  dropped for images whose dimensions are not evenly divisible.
- For images using subsampling, strip organization (`RowsPerStrip`) must be
  consistent with the luma/chroma grid; `RowsPerStrip` is expressed in luma
  rows.

## Positioning

- `YCbCrPositioning = 1` (default): **centered** positioning of the chroma
  samples with respect to the luminance samples.
- `YCbCrPositioning = 2`: **co-sited** positioning (chroma sample points aligned
  with luma sample points, e.g. recommended for 4:2:2 workflows).

## Minimum requirements for YCbCr images

In addition to the general Baseline TIFF requirements, a YCbCr file must have:

- `SamplesPerPixel = 3` (Y, Cb, Cr).
- `BitsPerSample = 8, 8, 8`.
- `Compression = 1` (none), `5` (LZW) or `7` (JPEG; TIFF Technical Note 2).
- `PhotometricInterpretation = 6` (YCbCr).
- `PlanarConfig = 1` (chunky) or `2` (separate).
- `ReferenceBlackWhite`: required by the TIFF 6.0 specification. Readers must
  nevertheless handle its absence by falling back to the YCbCr-specific
  defaults above (Y 0/255, Cb/Cr 128/255), because many real-world files omit
  tag 532.

## Generating the test images

`tests/ycbcr_images/generate_ycbcr.py` recreates the fixture set. It requires
Python 3 and the libtiff and netpbm command-line tools (`pnmtotiff`,
`rgb2ycbcr`, `tiffset`, `tiffcp`, `tiff2rgba`, `tifftopnm`). From the repository
root, run:

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
4. Attributes that `rgb2ycbcr` cannot produce directly are applied with
   `tiffset`. RATIONAL tags are set without a count, e.g.
   `tiffset -s YCbCrCoefficients 0.2126 0.7152 0.0722 file`. Tiled organization
   uses `tiffcp -t -w 16 -l 16 -c none`; because `tiffcp` refuses subsampled
   input it tiles a 1x1 image and the four YCbCr tags plus
   `PhotometricInterpretation` are restored afterwards.
5. `tiff2rgba -n -c none <tif> <rgba.tif>` followed by
   `tifftopnm <rgba.tif> > ycbcr-*.ppm` produces each ground-truth P6 PPM.

The 17 fixtures are described in the matrix below. Five of them (the variants)
are tag-reading tests only: `tiffset` changes tag values without reorganizing
pixel data. The tiled fixture is additionally limited in that its pixel data is
**not** subsampled — the `YCbCrSubsampling` tag is set to `2 2` after tiling —
so it is not usable for conversion verification.

## ImageMagick limitations

ImageMagick is a general-purpose image converter, but its TIFF writer has a
fixed set of capabilities that matter for YCbCr coverage:

- It does **not** write `YCbCrCoefficients` (529), `YCbCrSubsampling` (530),
  `YCbCrPositioning` (531), or `ReferenceBlackWhite` (532). Requests through
  `-define` or `-sampling-factor` do not create those tags; subsampling through
  image resizing changes the pixel dimensions instead of writing the tag.
- It does **not** support separate planar configuration
  (`PlanarConfiguration = 2`).
- It can write only basic YCbCr TIFFs: `PhotometricInterpretation = 6`,
  `SamplesPerPixel = 3`, 8-bit samples, uncompressed data, and default or absent
  YCbCr tags. Tiling is available through
  `-define tiff:tile-geometry=<value>`.

These limits are why the fixtures in this repository are generated with
libtiff's `rgb2ycbcr` rather than ImageMagick.

## Test image matrix

All files are in `tests/ycbcr_images/`. The "Kind" column marks a fixture as a
**conversion** test (tags and pixel data agree) or a **tag-reading** test.

### Subsampling fixtures

Nine 64x48 images with default BT.601 coefficients, centered positioning,
default reference values, 8-bit contiguous planes and no compression. Each
carries a real `YCbCrSubsampling` value written by `rgb2ycbcr`.

| Test image | Subsampling | Kind | Validates |
|---|---|---|---|
| `ycbcr-1x1.tif` | 1x1 | conversion | Full-resolution YCbCr with all four tags at their defaults; stripped organization. |
| `ycbcr-2x1.tif` | 2x1 | conversion | Horizontal-only chroma subsampling. |
| `ycbcr-1x2.tif` | 1x2 | conversion | Vertical-only chroma subsampling. |
| `ycbcr-2x2.tif` | 2x2 | conversion | Half-resolution chroma on both axes. |
| `ycbcr-4x1.tif` | 4x1 | conversion | Quarter-resolution horizontal chroma. |
| `ycbcr-1x4.tif` | 1x4 | conversion | Quarter-resolution vertical chroma. |
| `ycbcr-4x2.tif` | 4x2 | conversion | Mixed 4x2 chroma grid. |
| `ycbcr-2x4.tif` | 2x4 | conversion | Mixed 2x4 chroma grid. |
| `ycbcr-4x4.tif` | 4x4 | conversion | Quarter-resolution chroma on both axes. |

### Variant fixtures

Five 64x48 images with one non-default attribute each. These are tag-reading
tests: `tiffset` changes the tag without reorganizing pixel data.

| Test image | Attribute | Kind | Validates |
|---|---|---|---|
| `ycbcr-cosited.tif` | Positioning = 2 (co-sited) on a 2x2 base | tag-reading | Co-sited chroma positioning. |
| `ycbcr-custom-coeffs.tif` | Coefficients = 0.2126/0.7152/0.0722 on a 1x1 base | tag-reading | Non-default luma coefficients (tag 529). |
| `ycbcr-custom-refbw.tif` | RefBW = 15/235/128/240/128/240 on a 1x1 base | tag-reading | Headroom/footroom reference values (tag 532). |
| `ycbcr-planar-2x2.tif` | PlanarConfig = 2 on a 2x2 base | tag-reading | Separate-plane configuration (tag 284). |
| `ycbcr-tiled-2x2.tif` | 16x16 tiles, 4x3 grid (12 tiles) | tag-reading | Tile tag path together with all four YCbCr tags. |

`ycbcr-tiled-2x2.tif` is a special case: `tiffcp` cannot tile subsampled input,
so the image was tiled while still 1x1 and the `YCbCrSubsampling` tag was set to
`2 2` afterwards. Its pixel data is therefore not subsampled; the fixture
exercises tag reading and tile traversal only.

### Odd-dimension fixtures

Three images whose width and height are not multiples of the subsampling
factors, so the chroma dimensions use the ceiling rule. Tags and pixel data
agree, so all three are conversion tests.

| Test image | Dimensions | Subsampling | Kind | Validates |
|---|---|---|---|---|
| `ycbcr-odd-2x2.tif` | 61x47 | 2x2 | conversion | Ceiling of odd luma dimensions for 2x2 chroma. |
| `ycbcr-odd-4x4.tif` | 63x61 | 4x4 | conversion | Ceiling of odd luma dimensions for 4x4 chroma. |
| `ycbcr-odd-mixed.tif` | 62x47 | 2x1 | conversion | Ceiling on one axis only for mixed subsampling. |

## Ground-truth reference images

`tests/reference_images/ycbcr-*.ppm` holds the RGB ground-truth images
corresponding to the 17 YCbCr test images, in binary Netpbm PPM (P6) format.
Each is produced with `tiff2rgba` followed by `tifftopnm`, and each P6 header
matches the dimensions of its TIFF (64x48, or the odd dimensions 61x47, 63x61
and 62x47). libtiff's YCbCr RGBA reader cannot decode 1x4 and 2x4 subsampling,
so those two references are rendered from the matching RGB source TIFF through
the same `tiff2rgba` + `tifftopnm` pipeline.
