/*
 * Dump libtiff's decoded YCbCr scanlines as raw bytes to stdout.
 *
 * libtiff's TIFFRGBAImage / tiff2rgba path rejects 4x vertical chroma
 * subsampling and planar + subsampled YCbCr, while TIFFReadScanline decodes
 * the same pixel data correctly. The fixture generator converts these bytes
 * to RGB with the TIFF's own YCbCrCoefficients and ReferenceBlackWhite values.
 *
 * Output layout (all raw bytes, no headers):
 *   PlanarConfig = 1 (chunky): h scanlines of TIFFScanlineSize() bytes,
 *       read with TIFFReadScanline(..., row, 0) in increasing row order.
 *   PlanarConfig = 2 (three separate planes): plane 0 (Y), then plane 1 (Cb),
 *       then plane 2 (Cr). For each sample s the helper performs
 *       ceil(strip_bytes_for_sample / TIFFScanlineSize()) successive
 *       TIFFReadScanline(..., row, s) calls starting at row 0. libtiff reports
 *       the full-image-width scanline size for every plane, including
 *       subsampled chroma planes, so that read count is exactly the number of
 *       scanline-sized chunks that covers the plane's strips; the writes
 *       concatenate to the plane's rows in row order (Y = h rows of w bytes,
 *       Cb = Cr = up to h rows of w bytes, of which only the first ceil(w/hs)
 *       bytes of the first ceil(h/vs) rows are chroma samples). libtiff maps
 *       a scanline to strip row/RowsPerStrip in every plane, so the
 *       subsampled fixture pads its chroma strips to full RowsPerStrip
 *       scanlines; consecutive valid row reads exercise every strip of every
 *       plane (rows >= ImageLength are skipped, as TIFFReadScanline rejects
 *       them).
 *
 * Compile from the repository root with:
 *   gcc -I/usr/local/include -L/usr/local/lib \
 *       tests/ycbcr_images/ycbcr_scanline_dump.c -ltiff -o ycbcr_scanline_dump
 * Run with:
 *   ./ycbcr_scanline_dump tests/ycbcr_images/ycbcr-1x4.tif > scanlines.bin
 * Only binary scanline bytes go to stdout; diagnostics go to stderr.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tiffio.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s TIFF\n", argv[0]);
        return 2;
    }

    TIFF *tif = TIFFOpen(argv[1], "r");
    if (!tif) {
        fprintf(stderr, "TIFFOpen failed: %s\n", argv[1]);
        return 1;
    }

    uint32_t w = 0, h = 0, rows_per_strip = 0;
    uint16_t planar = 1, spp = 1;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);
    uint16_t subsampling_h = 1, subsampling_v = 1;
    TIFFGetFieldDefaulted(tif, TIFFTAG_YCBCRSUBSAMPLING,
                          &subsampling_h, &subsampling_v);

    tmsize_t scanline_size = TIFFScanlineSize(tif);
    if (w == 0 || h == 0 || scanline_size <= 0 ||
        (planar != 1 && (planar != 2 || spp != 3))) {
        fprintf(stderr, "unsupported image geometry or planar configuration\n");
        TIFFClose(tif);
        return 1;
    }
    fprintf(stderr,
            "w=%u h=%u subsampling=%u,%u rps=%u planar=%u spp=%u scanline=%ld\n",
            (unsigned)w, (unsigned)h,
            (unsigned)subsampling_h, (unsigned)subsampling_v,
            (unsigned)rows_per_strip, (unsigned)planar, (unsigned)spp,
            (long)scanline_size);

    void *buf = _TIFFmalloc(scanline_size);
    if (!buf) {
        fprintf(stderr, "could not allocate scanline buffer\n");
        TIFFClose(tif);
        return 1;
    }

    uint32_t total_reads = 0;
    uint32_t nstrips = TIFFNumberOfStrips(tif);
    uint32_t *strip_reads = (uint32_t *)calloc(nstrips, sizeof(uint32_t));
    if (!strip_reads) {
        fprintf(stderr, "could not allocate strip read counters\n");
        _TIFFfree(buf);
        TIFFClose(tif);
        return 1;
    }
    uint16_t n_samples = (planar == 1) ? 1 : spp;
    int failed = 0;
    for (uint16_t sample = 0; sample < n_samples && !failed; sample++) {
        uint32_t reads = h;
        if (planar == 2) {
            /* Strips are plane-major: sample * strips-per-plane + strip. */
            uint64_t plane_bytes = 0;
            uint32_t strips_per_plane =
                (rows_per_strip ? (h + rows_per_strip - 1) / rows_per_strip : 0);
            for (uint32_t strip = 0; strip < strips_per_plane; strip++) {
                uint32_t index = sample * strips_per_plane + strip;
                if (index >= nstrips) {
                    fprintf(stderr, "plane %u strip %u is past the %u strips "
                                    "in the TIFF\n", (unsigned)sample,
                            (unsigned)index, (unsigned)nstrips);
                    failed = 1;
                    break;
                }
                plane_bytes += TIFFGetStrileByteCount(tif, index);
            }
            if (failed)
                break;
            if (plane_bytes == 0 ||
                plane_bytes % (uint64_t)scanline_size != 0) {
                fprintf(stderr, "plane %u has %llu bytes, not a multiple of "
                                "the %ld-byte scanline size\n", (unsigned)sample,
                        (unsigned long long)plane_bytes, (long)scanline_size);
                failed = 1;
                break;
            }
            /* TIFFReadScanline rejects row >= ImageLength; padded chroma
             * strips may hold more scanlines than the image has rows. */
            uint64_t computed = plane_bytes / (uint64_t)scanline_size;
            reads = (computed < h) ? (uint32_t)computed : h;
        }
        uint32_t plane_reads = 0;
        for (uint32_t row = 0; row < reads; row++) {
            if (TIFFReadScanline(tif, buf, row, sample) < 0) {
                fprintf(stderr, "TIFFReadScanline failed at row %u "
                                "(sample %u)\n", (unsigned)row, (unsigned)sample);
                failed = 1;
                break;
            }
            uint32_t strip = TIFFComputeStrip(tif, row, sample);
            if (strip < nstrips)
                strip_reads[strip]++;
            if (fwrite(buf, 1, (size_t)scanline_size, stdout)
                    != (size_t)scanline_size) {
                fprintf(stderr, "could not write scanline %u (sample %u)\n",
                        (unsigned)row, (unsigned)sample);
                failed = 1;
                break;
            }
            plane_reads++;
        }
        if (failed)
            break;
        total_reads += plane_reads;
        fprintf(stderr, "sample %u: read %u/%u scanlines OK\n",
                (unsigned)sample, (unsigned)plane_reads, (unsigned)reads);
    }

    if (!failed) {
        uint32_t unread = 0;
        for (uint32_t strip = 0; strip < nstrips; strip++) {
            fprintf(stderr, "strip %u: %u scanlines read\n",
                    (unsigned)strip, (unsigned)strip_reads[strip]);
            if (strip_reads[strip] == 0)
                unread++;
        }
        if (unread) {
            fprintf(stderr, "%u strip(s) were never read\n", (unsigned)unread);
            failed = 1;
        }
    }
    if (!failed)
        fprintf(stderr, "read %u scanlines OK\n", (unsigned)total_reads);
    free(strip_reads);
    _TIFFfree(buf);
    TIFFClose(tif);
    return failed ? 1 : 0;
}
