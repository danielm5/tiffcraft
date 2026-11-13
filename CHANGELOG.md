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
- README build and test instructions (including git-lfs fixture prerequisites).

### Fixed

- Configure-time check in tests/CMakeLists.txt detects missing or unresolved Git LFS fixtures in tests/libtiff-pics and fails with setup instructions instead of the cryptic "Invalid byte order in TIFF header" test failure.

## [0.1.0]

### Added

- Initial TiffCraft code for loading TIFF images and exporter for bilevel,
  grayscale, palette-color, and RGB images. Compression is not supported.
- Example `tiff_exporter` program to convert TIFF images to PNG 8-bit format.

### Changed

- Moved all headers to `include/tiffcraft`.
