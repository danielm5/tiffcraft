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

## Generating the test images

`tests/ycbcr_images/generate_ycbcr.py` recreates the fixture set. It requires
Python 3 and ImageMagick (`magick` on `PATH`). From the repository root, run:

```sh
python3 tests/ycbcr_images/generate_ycbcr.py
```

The script writes the YCbCr TIFFs to `tests/ycbcr_images/` and the matching
Netpbm ASCII PPM (P3) ground-truth images to `tests/reference_images/`. It uses
a fixed source pattern, so rerunning it reproduces the same files. It emits only
fixtures whose attributes ImageMagick can actually write into the TIFF; cases
for attributes it cannot write were removed after the tag-level audit below.

## ImageMagick limitations

The following was verified with ImageMagick 7.1.2-35. Only attributes that
ImageMagick really writes into the TIFF are represented in the fixture set. For
every attribute it cannot write, the corresponding case was **removed** rather
than kept with a false claim.

- **YCbCrCoefficients (529):** ImageMagick uses the ITU-R BT.601 defaults;
  `-define tiff:YCbCrCoefficients=...` does not write the tag. Custom
  coefficients are not feasible with ImageMagick, so that case was removed.
- **ReferenceBlackWhite (532):** ImageMagick does not write the tag;
  `-define tiff:ReferenceBlackWhite=...` does not work. Custom or non-default
  reference black/white values are not feasible, so that case was removed.
- **YCbCrPositioning (531):** cosited positioning (`2`) cannot be set with
  `-define` or `-set`. No alternative exists, so the co-sited case was removed.
- **PlanarConfiguration (284):** separate planes (`2`) cannot be set with
  `-interlace Plane` or `-define tiff:planar-configuration=separate`. The planar
  case was removed rather than mislabeled.
- **YCbCrSubSampling (530):** non-1x1 subsampling cannot be set; even
  `-sampling-factor 2x2` leaves the tag at `[1, 1]`. The old generator used
  `-resize`, which physically shrank the images instead of writing the tag —
  misleading — so all subsampled cases were removed.
- **Tiling:** tiling **can** be set with `-define tiff:tile-geometry=64x48`
  (verified: writes `TileWidth` = 64, `TileLength` = 48, tile offsets and tile
  byte counts, and no `RowsPerStrip`). This is the only variant attribute
  ImageMagick can write, and it is used for the retained tiled fixture.

## Test image matrix

The table below lists the two retained YCbCr TIFFs in `tests/ycbcr_images/`
and their verified attributes.

| Test image | Subsampling (h x v) | Positioning | Coefficients | RefBW | Planar | Bits | Dimensions | Validates |
|---|---|---|---|---|---|---|---|---|
| `ycbcr-1x1.tif` | 1x1 | centered (1) | default | default | chunky | 8 | 64x48 | Full-resolution YCbCr; all YCbCr tags at their TIFF defaults; stripped organization. |
| `ycbcr-tiled.tif` | 1x1 | centered (1) | default | default | chunky (tiled) | 8 | 64x48 | Tiled organization (TileWidth=64, TileLength=48); otherwise all defaults. |

### Removed fixtures (audit results)

A tag-level audit showed that 16 of the original 17 images did not match their
claimed attributes. Those cases were removed (or, for the tiled case, replaced)
rather than kept with a false claim; the reasons are recorded here.

| Removed image | Claimed attribute | Audit result / reason |
|---|---|---|
| `ycbcr-2x1.tif` | 2x1 subsampling | Physically downscaled to 32x48; `YCbCrSubSampling = [1, 1]`. ImageMagick cannot set subsampling. |
| `ycbcr-1x2.tif` | 1x2 subsampling | Physically downscaled to 64x24; `YCbCrSubSampling = [1, 1]`. ImageMagick cannot set subsampling. |
| `ycbcr-2x2.tif` | 2x2 subsampling | Physically downscaled to 32x24; `YCbCrSubSampling = [1, 1]`. ImageMagick cannot set subsampling. |
| `ycbcr-4x1.tif` | 4x1 subsampling | Physically downscaled to 16x48; `YCbCrSubSampling = [1, 1]`. ImageMagick cannot set subsampling. |
| `ycbcr-1x4.tif` | 1x4 subsampling | Physically downscaled to 64x12; `YCbCrSubSampling = [1, 1]`. ImageMagick cannot set subsampling. |
| `ycbcr-4x2.tif` | 4x2 subsampling | Physically downscaled to 16x24; `YCbCrSubSampling = [1, 1]`. ImageMagick cannot set subsampling. |
| `ycbcr-2x4.tif` | 2x4 subsampling | Physically downscaled to 32x12; `YCbCrSubSampling = [1, 1]`. ImageMagick cannot set subsampling. |
| `ycbcr-4x4.tif` | 4x4 subsampling | Physically downscaled to 16x12; `YCbCrSubSampling = [1, 1]`. ImageMagick cannot set subsampling. |
| `ycbcr-cosited.tif` | co-sited positioning (2) | `YCbCrPositioning` tag absent (defaults to 1); ImageMagick cannot set cosited=2. |
| `ycbcr-custom-coeffs.tif` | custom BT.709 coefficients | `YCbCrCoefficients` tag absent (BT.601 default); ImageMagick cannot write custom coefficients. |
| `ycbcr-custom-refbw.tif` | custom reference black/white | `ReferenceBlackWhite` tag absent; ImageMagick cannot write the tag (the fixture was effectively a 1x1/default image). |
| `ycbcr-planar-2x2.tif` | separate planar configuration (2) | `PlanarConfiguration = 1` (chunky), not 2; ImageMagick cannot write separate planar. |
| `ycbcr-odd-2x2.tif` | 2x2 subsampling, odd dimensions 61x47 | Actually 31x24 and physically downscaled, with no subsampling tag; odd-dimension chroma-ceiling claims are only meaningful with real subsampling. |
| `ycbcr-odd-4x4.tif` | 4x4 subsampling, odd dimensions 63x61 | Actually 16x16 and physically downscaled, with no subsampling tag; odd-dimension chroma-ceiling claims are only meaningful with real subsampling. |
| `ycbcr-odd-mixed.tif` | 2x1 subsampling, odd dimensions 62x47 | Actually 31x47 and physically downscaled, with no subsampling tag; odd-dimension chroma-ceiling claims are only meaningful with real subsampling. |
| `ycbcr-tiled-2x2.tif` | tiled organization with 2x2 subsampling | Had no tile tags (used `RowsPerStrip = 24`) and falsely claimed 2x2 subsampling; replaced by regenerated `ycbcr-tiled.tif` (64x48, real tile tags). |

## Ground-truth reference images

`tests/reference_images/ycbcr-*.ppm` holds the RGB ground-truth images
corresponding to the YCbCr test images, in Netpbm ASCII PPM (P3) format.
