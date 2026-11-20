# YCbCr (PhotometricInterpretation = 6) Support Reference

This document describes YCbCr (`PhotometricInterpretation = 6`) images as
defined by the TIFF 6.0 specification.

## Scope

- YCbCr image encoding as defined by the TIFF 6.0 specification.
- Conversion between RGB and YCbCr.
- Chroma subsampling, positioning, and reference black/white handling.
- Minimum requirements for a valid YCbCr TIFF.
- Mapping of the test images in `tests/ycbcr_images/` to the specification
  topics they exercise.

## Tag definitions

The following TIFF tags describe the color information of an image whose
`PhotometricInterpretation = 6` (YCbCr).

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
the white value to the white end, allowing scaling of the YCbCr codes into the
full data range when converting to RGB.

For YCbCr the default reference values are black = 0 and white = 255 for the
luma component (Y), and black = 128 and white = 255 for the chroma components
(Cb/Cr).

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

## Generating the test images

`tests/ycbcr_images/generate_ycbcr.py` recreates the fixture set. It requires
Python 3 and ImageMagick (`magick` on `PATH`). From the repository root, run:

```sh
python3 tests/ycbcr_images/generate_ycbcr.py
```

The script writes the YCbCr TIFFs to `tests/ycbcr_images/` and the matching
Netpbm ASCII PPM (P3) ground-truth images to `tests/reference_images/`. It uses
a fixed source pattern, so rerunning it reproduces the same files.

## Test image matrix

The tables below list the YCbCr TIFFs in `tests/ycbcr_images/` and their
attributes.

### Subsampling coverage (all centered, default coefficients, default
`ReferenceBlackWhite`, chunky, BitsPerSample 8, base image 64x48)

| Test image | Subsampling (h x v) | Positioning | Coefficients | RefBW | Planar | Bits | Dimensions | Validates |
|---|---|---|---|---|---|---|---|---|
| `ycbcr-1x1.tif` | 1x1 | centered (1) | default | default | chunky | 8 | 64x48 | No subsampling; full-resolution YCbCr. |
| `ycbcr-2x1.tif` | 2x1 | centered (1) | default | default | chunky | 8 | 64x48 | Horizontal subsampling factor 2. |
| `ycbcr-1x2.tif` | 1x2 | centered (1) | default | default | chunky | 8 | 64x48 | Vertical subsampling factor 2. |
| `ycbcr-2x2.tif` | 2x2 | centered (1) | default | default | chunky | 8 | 64x48 | Quarter-resolution chroma (2x2 subsampling). |
| `ycbcr-4x1.tif` | 4x1 | centered (1) | default | default | chunky | 8 | 64x48 | Horizontal subsampling factor 4. |
| `ycbcr-1x4.tif` | 1x4 | centered (1) | default | default | chunky | 8 | 64x48 | Vertical subsampling factor 4. |
| `ycbcr-4x2.tif` | 4x2 | centered (1) | default | default | chunky | 8 | 64x48 | Combined subsampling factors 4 and 2. |
| `ycbcr-2x4.tif` | 2x4 | centered (1) | default | default | chunky | 8 | 64x48 | Combined subsampling factors 2 and 4. |
| `ycbcr-4x4.tif` | 4x4 | centered (1) | default | default | chunky | 8 | 64x48 | Maximum subsampling factor 4 on both axes. |

### Variants

| Test image | Subsampling (h x v) | Positioning | Coefficients | RefBW | Planar | Bits | Dimensions | Validates |
|---|---|---|---|---|---|---|---|---|
| `ycbcr-cosited.tif` | 2x2 | co-sited (2) | default | default | chunky | 8 | 64x48 | Co-sited positioning with subsampling. |
| `ycbcr-custom-coeffs.tif` | 1x1 | centered (1) | custom | default | chunky | 8 | 64x48 | Non-default YCbCr coefficients (BT.709). |
| `ycbcr-custom-refbw.tif` | 1x1 | centered (1) | default | custom | chunky | 8 | 64x48 | Non-default reference black/white values. |
| `ycbcr-planar-2x2.tif` | 2x2 | centered (1) | default | default | separate | 8 | 64x48 | Separate planar configuration (PlanarConfig=2). |
| `ycbcr-tiled-2x2.tif` | 2x2 | centered (1) | default | default | chunky (tiled) | 8 | 64x48 | Tiled image organization. |

### Odd (non-divisible) dimensions

| Test image | Subsampling (h x v) | Positioning | Coefficients | RefBW | Planar | Bits | Dimensions | Validates |
|---|---|---|---|---|---|---|---|---|
| `ycbcr-odd-2x2.tif` | 2x2 | centered (1) | default | default | chunky | 8 | 61x47 | Chroma dimension ceiling with odd dimensions. |
| `ycbcr-odd-4x4.tif` | 4x4 | centered (1) | default | default | chunky | 8 | 63x61 | Subsampling 4 with odd dimensions. |
| `ycbcr-odd-mixed.tif` | 2x1 | centered (1) | default | default | chunky | 8 | 62x47 | Mixed subsampling and odd dimension handling. |

## Ground-truth reference images

`tests/reference_images/ycbcr-*.ppm` holds the RGB ground-truth images
corresponding to the YCbCr test images, in Netpbm ASCII PPM (P3) format.
