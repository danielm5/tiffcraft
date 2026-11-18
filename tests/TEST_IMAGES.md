# Test Images

This document lists the TIFF test images used by the TiffCraft test suite,
grouped by source folder. For every image the table shows the image name,
dimensions, bits per sample, samples / photometric interpretation, compression,
planar configuration and whether the current version of TiffCraft supports it.

## `tests/libtiff-pics`

Sample images contributed by the libtiff project (fetched via the
`tests/libtiff-pics` submodule).

### Test image format table

The following table lists the test images.

| Image | Dimensions | Bits per sample | Samples / Photometric | Compression | Planar config | Tiled | TiffCraft support |
|---|---|---|---|---|---|---|---|
| `caspian.tif` | 279x220 | 64 bpp x3 | RGB (3 samples) | Deflate | Separate | No | ❌ Not supported |
| `cramps-tile.tif` | 800x607 | 8 bpp | Gray (1 sample) | None | Chunky | Yes (Tile 256x256) | ✅ Supported |
| `cramps.tif` | 800x607 | 8 bpp | Gray (1 sample) | PackBits | Chunky | No | ❌ Not supported |
| `dscf0013.tif` | 160x120 | 8 bpp x3 | YCbCr (3 samples) | None | Chunky | No | ❌ Not supported |
| `fax2d.tif` | 1728x1082 | 1 bpp | Gray (1 sample) | CCITT G3 | Chunky | No | ❌ Not supported |
| `g3test.tif` | 1728x1103 | 1 bpp | Gray (1 sample) | CCITT G3 | Chunky | No | ❌ Not supported |
| `jello.tif` | 256x192 | 8 bpp | Palette (1 sample) | PackBits | Chunky | No | ❌ Not supported |
| `jim___ah.tif` | 664x813 | 1 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `jim___cg.tif` | 277x339 | 8 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `jim___dg.tif` | 277x339 | 8 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `jim___gg.tif` | 277x339 | 8 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `ladoga.tif` | 158x118 | 16 bpp | Gray (1 sample) | Deflate | Chunky | No | ❌ Not supported |
| `off_l16.tif` | 333x225 | 16 bpp | CIE L*a*b* (1 sample) | None | Chunky | No | ❌ Not supported |
| `off_luv24.tif` | 333x225 | 16 bpp x3 | CIE L*u*v* (3 samples) | None | Chunky | No | ❌ Not supported |
| `off_luv32.tif` | 333x225 | 16 bpp x3 | CIE L*u*v* (3 samples) | None | Chunky | No | ❌ Not supported |
| `oxford.tif` | 601x81 | 8 bpp x3 | RGB (3 samples) | LZW | Separate | No | ❌ Not supported |
| `pc260001.tif` | 640x480 | 8 bpp x3 | RGB (3 samples) | None | Chunky | No | ✅ Supported |
| `quad-jpeg.tif` | 512x384 | 8 bpp x3 | YCbCr (3 samples) | JPEG | Chunky | No | ❌ Not supported |
| `quad-lzw.tif` | 512x384 | 8 bpp x3 | RGB (3 samples) | LZW | Chunky | No | ❌ Not supported |
| `quad-tile.tif` | 512x384 | 8 bpp x3 | RGB (3 samples) | LZW | Chunky | Yes (Tile 128x128) | ❌ Not supported |
| `smallliz.tif` | 160x160 | 8 bpp x3 | YCbCr (3 samples) | Old JPEG | Chunky | No | ❌ Not supported |
| `strike.tif` | 256x200 | 8 bpp x4 | RGB (4 samples, extra sample) | LZW | Chunky | No | ❌ Not supported |
| `text.tif` | 1512x359 | 4 bpp | Gray (1 sample) | ThunderScan (*) | Chunky | No | ❌ Not supported |
| `ycbcr-cat.tif` | 250x325 | 8 bpp x3 | YCbCr (3 samples) | LZW | Chunky | No | ❌ Not supported |
| `zackthecat.tif` | 234x213 | 8 bpp x3 | YCbCr (3 samples) | Old JPEG | Chunky | Yes (Tile 240x224) | ❌ Not supported |

> (*) **Note:** `text.tif` uses TIFF compression code `32809`, labeled **ThunderScan**,
> which is a **non-standard** compression value. libTiff only supports ThunderScan decompression, it does not support
> writing images with ThunderScan compression.

## `tests/libtiff-pics/depth`

Generated flower images used for format coverage (bits per sample, planar and
palette combinations).

### Test image format table

The following table lists the test images.

| Image | Dimensions | Bits per sample | Samples / Photometric | Compression | Planar config | Tiled | TiffCraft support |
|---|---|---|---|---|---|---|---|
| `flower-minisblack-02.tif` | 73x43 | 2 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-minisblack-04.tif` | 73x43 | 4 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-minisblack-06.tif` | 73x43 | 6 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-minisblack-08.tif` | 73x43 | 8 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-minisblack-10.tif` | 73x43 | 10 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-minisblack-12.tif` | 73x43 | 12 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-minisblack-14.tif` | 73x43 | 14 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-minisblack-16.tif` | 73x43 | 16 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-minisblack-24.tif` | 73x43 | 24 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-minisblack-32.tif` | 73x43 | 32 bpp | Gray (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-palette-02.tif` | 73x43 | 2 bpp | Palette (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-palette-04.tif` | 73x43 | 4 bpp | Palette (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-palette-08.tif` | 73x43 | 8 bpp | Palette (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-palette-16.tif` | 73x43 | 16 bpp | Palette (1 sample) | None | Chunky | No | ✅ Supported |
| `flower-rgb-contig-02.tif` | 73x43 | 2 bpp x3 | RGB (3 samples) | None | Chunky | No | ✅ Supported |
| `flower-rgb-contig-04.tif` | 73x43 | 4 bpp x3 | RGB (3 samples) | None | Chunky | No | ✅ Supported |
| `flower-rgb-contig-08.tif` | 73x43 | 8 bpp x3 | RGB (3 samples) | None | Chunky | No | ✅ Supported |
| `flower-rgb-contig-10.tif` | 73x43 | 10 bpp x3 | RGB (3 samples) | None | Chunky | No | ✅ Supported |
| `flower-rgb-contig-12.tif` | 73x43 | 12 bpp x3 | RGB (3 samples) | None | Chunky | No | ✅ Supported |
| `flower-rgb-contig-14.tif` | 73x43 | 14 bpp x3 | RGB (3 samples) | None | Chunky | No | ✅ Supported |
| `flower-rgb-contig-16.tif` | 73x43 | 16 bpp x3 | RGB (3 samples) | None | Chunky | No | ✅ Supported |
| `flower-rgb-contig-24.tif` | 73x43 | 24 bpp x3 | RGB (3 samples) | None | Chunky | No | ✅ Supported |
| `flower-rgb-contig-32.tif` | 73x43 | 32 bpp x3 | RGB (3 samples) | None | Chunky | No | ✅ Supported |
| `flower-rgb-planar-02.tif` | 73x43 | 2 bpp x3 | RGB (3 samples) | None | Separate | No | ✅ Supported |
| `flower-rgb-planar-04.tif` | 73x43 | 4 bpp x3 | RGB (3 samples) | None | Separate | No | ✅ Supported |
| `flower-rgb-planar-08.tif` | 73x43 | 8 bpp x3 | RGB (3 samples) | None | Separate | No | ✅ Supported |
| `flower-rgb-planar-10.tif` | 73x43 | 10 bpp x3 | RGB (3 samples) | None | Separate | No | ✅ Supported |
| `flower-rgb-planar-12.tif` | 73x43 | 12 bpp x3 | RGB (3 samples) | None | Separate | No | ✅ Supported |
| `flower-rgb-planar-14.tif` | 73x43 | 14 bpp x3 | RGB (3 samples) | None | Separate | No | ✅ Supported |
| `flower-rgb-planar-16.tif` | 73x43 | 16 bpp x3 | RGB (3 samples) | None | Separate | No | ✅ Supported |
| `flower-rgb-planar-24.tif` | 73x43 | 24 bpp x3 | RGB (3 samples) | None | Separate | No | ✅ Supported |
| `flower-rgb-planar-32.tif` | 73x43 | 32 bpp x3 | RGB (3 samples) | None | Separate | No | ✅ Supported |
| `flower-separated-contig-08.tif` | 73x43 | 8 bpp x4 | CMYK (4 samples) | None | Chunky | No | ❌ Not supported |
| `flower-separated-contig-16.tif` | 73x43 | 16 bpp x4 | CMYK (4 samples) | None | Chunky | No | ❌ Not supported |
| `flower-separated-planar-08.tif` | 73x43 | 8 bpp x4 | CMYK (4 samples) | None | Separate | No | ❌ Not supported |
| `flower-separated-planar-16.tif` | 73x43 | 16 bpp x4 | CMYK (4 samples) | None | Separate | No | ❌ Not supported |
