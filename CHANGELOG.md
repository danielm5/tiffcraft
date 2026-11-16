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
- README now documents which headers are needed and the copy-only
  consumption path (header-only; copy the headers into the project, no build
  step required).
- CMake consumption improvements: added the `TiffCraft::TiffCraft` namespaced
  alias, an installable/exportable target with generated
  `TiffCraftConfig.cmake`/`TiffCraftConfigVersion.cmake` (for `find_package`),
  and a `TIFFCRAFT_BUILD_ALL` option that defaults to `ON` when building
  TiffCraft standalone and to `OFF` when consumed via `FetchContent` or
  `add_subdirectory`.

### Fixed

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
