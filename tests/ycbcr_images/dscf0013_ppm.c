/*
 * Decode every IFD of a multi-page TIFF into one P6 PPM per IFD.
 *
 * tiffsplit drops YCbCrSubsampling (tag 530) and YCbCrPositioning (tag 531)
 * when separating IFDs, so a tiffsplit + tiff2rgba ground-truth path silently
 * misreads cell-interleaved YCbCr data. This helper keeps the original IFDs
 * intact: it walks them with TIFFReadDirectory and decodes each one with
 * libtiff's TIFFReadRGBAImageOriented in ORIENTATION_TOPLEFT order.
 *
 * libtiff's RGBA raster is packed as 32-bit ABGR in memory. The PPM writer
 * emits R = p & 0xff, G = (p >> 8) & 0xff and B = (p >> 16) & 0xff; reversing
 * those shifts would swap red and blue.
 *
 * Compile from the repository root with:
 *   gcc -I/usr/local/include -L/usr/local/lib \
 *       tests/ycbcr_images/dscf0013_ppm.c -ltiff -o dscf0013_ppm
 * Run with:
 *   ./dscf0013_ppm tests/libtiff-pics/dscf0013.tif /tmp/dscf0013-ppms
 * Output files are named ifd00.ppm, ifd01.ppm, ... in the output directory;
 * diagnostics go to stderr, the IFD count goes to stdout.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s TIFF OUTPUT_DIR\n", argv[0]);
        return 2;
    }

    TIFF *tif = TIFFOpen(argv[1], "r");
    if (!tif) {
        fprintf(stderr, "TIFFOpen failed: %s\n", argv[1]);
        return 1;
    }

    unsigned int ifds = 0;
    int failed = 0;
    do {
        uint32_t w = 0, h = 0;
        if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w) ||
                !TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h) ||
                w == 0 || h == 0) {
            fprintf(stderr, "IFD %u has missing or invalid dimensions\n", ifds);
            failed = 1;
            break;
        }

        uint32_t *raster = (uint32_t *)_TIFFmalloc((tmsize_t)w * h
                                                   * sizeof(uint32_t));
        if (!raster) {
            fprintf(stderr, "could not allocate raster for IFD %u\n", ifds);
            failed = 1;
            break;
        }
        if (!TIFFReadRGBAImageOriented(tif, w, h, raster,
                                       ORIENTATION_TOPLEFT, 0)) {
            fprintf(stderr, "TIFFReadRGBAImageOriented failed for IFD %u\n",
                    ifds);
            _TIFFfree(raster);
            failed = 1;
            break;
        }

        char name[64];
        snprintf(name, sizeof(name), "ifd%02u.ppm", ifds);
        char *path = (char *)malloc(strlen(argv[2]) + strlen(name) + 2);
        if (!path) {
            fprintf(stderr, "could not allocate output path for IFD %u\n",
                    ifds);
            _TIFFfree(raster);
            failed = 1;
            break;
        }
        sprintf(path, "%s/%s", argv[2], name);

        FILE *out = fopen(path, "wb");
        if (!out) {
            fprintf(stderr, "fopen failed: %s\n", path);
            free(path);
            _TIFFfree(raster);
            failed = 1;
            break;
        }
        free(path);

        if (fprintf(out, "P6\n%u %u\n255\n", (unsigned)w, (unsigned)h) < 0) {
            fprintf(stderr, "could not write PPM header for IFD %u\n", ifds);
            failed = 1;
        }
        for (uint32_t i = 0; i < w * h && !failed; i++) {
            uint32_t p = raster[i];
            unsigned char rgb[3] = {
                (unsigned char)(p & 0xff),
                (unsigned char)((p >> 8) & 0xff),
                (unsigned char)((p >> 16) & 0xff)
            };
            if (fwrite(rgb, 1, sizeof(rgb), out) != sizeof(rgb)) {
                fprintf(stderr, "could not write pixel %u of IFD %u\n",
                        (unsigned)i, ifds);
                failed = 1;
            }
        }
        if (fclose(out) != 0) {
            fprintf(stderr, "fclose failed for IFD %u\n", ifds);
            failed = 1;
        }
        _TIFFfree(raster);
        ifds++;
    } while (!failed && TIFFReadDirectory(tif));

    TIFFClose(tif);
    if (failed)
        return 1;
    printf("%u\n", ifds);
    return 0;
}
