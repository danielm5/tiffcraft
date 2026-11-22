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
  `tests/YCBCR_SUPPORT.md` specification reference, 18 generated YCbCr test
  images under `tests/ycbcr_images/` (nine subsampling, six variant and three
  odd-dimension cases), and the corresponding `ycbcr-*.ppm` ground-truth
  reference images (child issue of the YCbCr support effort, #38).

### Changed

- CMake project version bumped to `0.2.0` to match the header version
  constants and the `[0.2.0-Unreleased]` changelog section.

### Fixed

- Completed the YCbCr documentation and test data in `tests/YCBCR_SUPPORT.md`
  and rebuilt the full 18-image fixture set with libtiff's `rgb2ycbcr`, so all
  four YCbCr tags are written into every TIFF; matching ground-truth PPMs come
  from `tiff2rgba` + `tifftopnm`. The document now carries generalized
  RGB/YCbCr conversion formulas parameterized by the luma coefficients in
  `YCbCrCoefficients`, the full `ReferenceBlackWhite` scaling formula with
  `code` terminology, the spec-versus-practice note on default reference
  values, and an 18-row matrix distinguishing conversion tests from
  tag-reading tests. Both planar fixtures contain genuine separate planes
  (six strips for the 1x1 base and three for the 2x2 base), and the generator
  automatically validates the structure of every TIFF it emits (#42).
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
