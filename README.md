[![Build and Test](https://github.com/danielm5/tiffcraft/actions/workflows/workflow.yml/badge.svg)](https://github.com/danielm5/tiffcraft/actions/workflows/workflow.yml)

# TiffCraft

TiffCraft is a header-only C++ library for working with TIFF images. At the
moment, it requires C++20 or newer and contains functionality for reading TIFF
images in various uncompressed formats.

# Objectives

TiffCraft is designed based on the following principles:

- Permissive license: to be used in open source and proprietary software.
- C++ header-only: to easy integration onto existing projects.
- Flexible but simple API: for ease of use.
- Feature complete: to cover a wide variety of use cases.
  - TiffCraft is NOT feature complete at this time! The objective is for guiding
    on-going development.

# Building from source

Prerequisites: CMake >= 3.22, a C++20 compiler, git, and git-lfs.

Clone the repository with its submodules:

```sh
git clone --recursive https://github.com/danielm5/tiffcraft.git
```

Existing clones can fetch the fixtures with:

```sh
git submodule update --init --recursive
```

The test fixtures live in the `tests/libtiff-pics` submodule and are stored
in Git LFS. On Debian, install git-lfs with `sudo apt install git-lfs`.
`./build.sh` runs the submodule and LFS steps for you; otherwise pull the
fixtures manually:

```sh
git -C tests/libtiff-pics lfs pull
```

Build (auto-picks a preset on Linux, Windows, and macOS):

```sh
./build.sh gcc-linux-release
```

Run the tests:

```sh
ctest --preset gcc-linux-release --output-on-failure
```

or `./run_test.sh`.

Note: without git-lfs the checkout keeps Git LFS pointer files instead of
real TIFFs, and configure now stops with a clear message instead of failing
the tests with "Invalid byte order in TIFF header". To remedy, run:

```sh
git submodule update --init --recursive && git -C tests/libtiff-pics lfs pull
```

# Usage

For quick access to TIFF image pixel data with individual pixels aligned to
word boundaries (i.e. 8 bits, 16 bits, or 32 bits), you could use code like
the following:

```C++
#include <tiffcraft/TiffExporter.hpp>
#include <functional>

int main(int argc, char* argv[]) {
  if (argc < 2) { return 1; }
  TiffCraft::TiffExporterAny tiffExporter;
  TiffCraft::load(argv[1], std::ref(tiffExporter), loadParams);
  auto image = iffExporter.takeImage();

  // your own image processing code

  return 0;
}
```

TiffExporterAny will decode and convert pixels to a flat-buffer (i.e.
 `std::vector<std::byte>`) with 1, 2, or 4 bytes per sample the source pixel
format. For example, an image with 2-bits per sample is exported as an 8-bit
image.

# Advanced usage

Users that prefer to decode and handle the image pixel data themselves, or need
to handle multiple frames per image, could write their own handler:

```C++
#include <tiffcraft/TiffImage.hpp>
#include <iostream>

int main() {
  TiffCraft::load("image.tiff",
    [](const TiffCraft::TiffImage::Header& header,
        const TiffCraft::TiffImage::IFD& ifd,
        TiffCraft::TiffImage::ImageData imageData) {
      std::cout << "Loaded IFD with " << ifd.entries().size()
                << " entries." << std::endl;
      std::cout << "Image data size: " << imageData.size() << std::endl;

      // Add your own image processing code here

    });

  return 0;
}
```

# Features

TiffCraft supports the following features:

- Reading TIFF images
  - Read and display image metadata
  - Bilevel, grayscale, palette-color, and RGB images
    - Varying number of bits per sample
    - RGB images are supported only for 3-channel images (no extra samples)
- Compression is NOT supported yet.
- Writing TIFF images is NOT supported yet.

Advanced users could use TiffCraft for other configurations than those
enumerated above by writing their customizing any of the existing exporter
classes, or writing their own from scratch.

# Feedback

Feedback is welcomed! Please don't hesitate to reach out by opening a
[GitHub issue](https://github.com/danielm5/tiffcraft/issues/new/choose)
if you have suggestions, feature requests, or bug reports.

Please let me know if you find this project useful!
