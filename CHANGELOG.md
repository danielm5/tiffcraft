# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Given a version number MAJOR.MINOR.PATCH, increment the:

    MAJOR version when you make incompatible API changes.
    MINOR version when you add functionality in a backward compatible manner.
    PATCH version when you make backward compatible bug fixes.

## [0.2.0-Unreleased]

### Added

- Code examples
- `TiffStdOut` documentation and tooling: a header usage example, a README
  section, a new `tiff_info` app for printing metadata and IFD tags, and the
  `tiffStdOutTest` doctest suite.
- README now documents which headers are needed and the copy-only
  consumption path (header-only; copy the headers into the project, no build
  step required).
- CMake consumption improvements: added the `TiffCraft::TiffCraft` namespaced
  alias, an installable/exportable target with generated
  `TiffCraftConfig.cmake`/`TiffCraftConfigVersion.cmake` (for `find_package`),
  and a `TIFFCRAFT_BUILD_ALL` option that defaults to `ON` when building
  TiffCraft standalone and to `OFF` when consumed via `FetchContent` or
  `add_subdirectory`.
- Version-consistency test between the CMake `PROJECT_VERSION` and the header
  version constants (`TiffCraft::version()`), failing with both values named on
  mismatch.
- YCbCr (PI=6) test coverage: added the TIFF 6.0 Section 21
  `tests/YCBCR_SUPPORT.md` specification reference, 17 generated YCbCr test
  images under `tests/ycbcr_images/` (nine subsampling, five variant and three
  odd-dimension cases), and the corresponding `ycbcr-*.ppm` ground-truth
  reference images (child issue of the YCbCr support effort, #38).
- YCbCr (PI=6) export support for 1x1 subsampling (#45): new `TiffExporterYCbCr`
  class converts Y, Cb and Cr samples to RGB using `YCbCrCoefficients` and
  `ReferenceBlackWhite` (default ITU-R BT.601 values), for both chunky and
  separate-plane data. Coefficients are validated to sum to 1. `TiffExporterAny`
  dispatches PI=6 images to it, and `TiffStdOut` prints the four YCbCr tag
  names. Conversion tests cover `ycbcr-1x1.tif` and `ycbcr-planar-1x1.tif`
  against their PPM ground truth.

### Changed

- Corrected `tests/YCBCR_SUPPORT.md` against TIFF 6.0 / Technical Note 2:
  generic `ReferenceBlackWhite` default, no-headroom YCbCr profile,
  subsampling constraints and default, and legacy vs new-style JPEG codes.
- Vendored `doctest` test framework upgraded from 2.4.12 to 2.5.0.
- Test-image documentation now lists the YCbCr fixtures and explains why the
  8-bit CMYK images are excluded from the exporter tests.
- CMake project version bumped to `0.2.0` to match the header version
  constants and the `[0.2.0-Unreleased]` changelog section.

### Fixed

- `readmeUsageTest` now catches exceptions and reports them on stderr instead
  of aborting, so IDE test-discovery probes (e.g. VS Code TestMate C++) no
  longer pop up error dialogs while running the executable.
- `TestExporter` returned after the first successful reference comparison, so
  only the first TIFF/reference pair in each test list was actually verified.
  It now continues through every pair. With all images exercised, the 4-channel
  CMYK fixtures were confirmed unsupported by the RGB exporter; their test cases
  were removed, and `tests/TEST_IMAGES.md` lists them as not supported and now
  explains why they are excluded from the exporter tests.
- Completed the YCbCr documentation and test data in `tests/YCBCR_SUPPORT.md`
  and rebuilt the 17-image fixture set. Every ground-truth PPM now comes
  from libtiff decoding of the SAME TIFF: `tiff2rgba` + `tifftopnm` for 14
  fixtures, and an in-repo libtiff `TIFFReadScanline` helper for the
  1x4/2x4/planar-2x2 references that the `tiff2rgba` RGBA path rejects — no
  equivalent or substitute image is used.
  The document now carries generalized RGB/YCbCr conversion formulas
  parameterized by the luma coefficients in `YCbCrCoefficients`, the full
  `ReferenceBlackWhite` scaling formula with `code` terminology, the
  spec-versus-practice note on default reference values, and a 17-row matrix
  of conversion tests. The custom-coefficient and custom-reference-value
  fixtures encode their pixel data with the tag values they carry. Both planar
  fixtures contain genuine separate
  planes (six strips for the 1x1 base and three for the 2x2 base), the tiled 2x2
  fixture carries real subsampled data in 16x16 tiles (4x3 grid, 12 tiles), and
  the generator automatically validates the structure of every TIFF it emits
  (#42, #48).
- `TiffExporter.hpp` no longer includes the umbrella header `TiffCraft.hpp`;
  it now includes `TiffImage.hpp` directly, resolving the include cycle and
  making the exporter header standalone.
- ODR violations in headers: `inline` on `version()`, `readAt()`, `writeAt()`,
  and both `load()` overloads (`TiffImage.hpp`); the six `operator<<` overloads
  moved into `namespace TiffCraft` and marked `inline` (`TiffStdOut.hpp`).
  Note: the stream operators are no longer visible in the global namespace;
  they are found via ADL, which applies to all TiffCraft types.
- Crash/hang fixes for malformed TIFF input: ASCII entry with count 0 no
  longer reads out of bounds (throws `std::runtime_error`); RATIONAL/SRATIONAL
  entries with a zero denominator no longer divide by zero (throws
  `std::runtime_error`); cyclic IFD offsets are detected and rejected (throws
  `std::runtime_error`).
- `readAt()` now detects short reads and throws `std::runtime_error`, so
  truncated strips/tiles no longer silently yield zero-filled buffers.
- `takeImage()` no longer copies instead of moving; it moves the image buffer
  out of the exporter, leaving it empty.
- `TiffExporterAny` now resets its multi-IFD state per IFD; each IFD is
  exported and the last successfully exported image wins (previously later
  IFDs were silently skipped).
- Example `tiff_image` reads `ImageWidth`/`ImageLength` type-agnostically, so
  LONG-typed entries no longer throw.
- README quick-start snippet fixed (removed undeclared `loadParams` argument;
  fixed `iffExporter` typo).
- `Type::BYTE` entries print as integers instead of raw characters.

## [0.1.0]

### Added

- Initial TiffCraft code for loading TIFF images and exporter for bilevel,
  grayscale, palette-color, and RGB images. Compression is not supported.
- Example `tiff_exporter` program to convert TIFF images to PNG 8-bit format.

### Changed

- Moved all headers to `include/tiffcraft`.
