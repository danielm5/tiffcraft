[![Build and Test](https://github.com/danielm5/tiffcraft/actions/workflows/workflow.yml/badge.svg)](https://github.com/danielm5/tiffcraft/actions/workflows/workflow.yml)

# TiffCraft

TiffCraft is a header-only C++20 library for reading uncompressed TIFF images.
It provides a customizable image-data export interface and requires no separate
library binary.

# Objectives

TiffCraft is designed based on the following principles:

- Permissive license: to be used in open source and proprietary software.
- C++ header-only: to easy integration onto existing projects.
- Flexible but simple API: for ease of use.
- Feature complete: to cover a wide variety of use cases.
  - TiffCraft is NOT feature complete at this time! The objective is for guiding
    on-going development.

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
  TiffCraft::load(argv[1], std::ref(tiffExporter));
  auto image = tiffExporter.takeImage();

  // your own image processing code

  return 0;
}
```

TiffExporterAny will decode and convert pixels to a flat-buffer (i.e.
 `std::vector<std::byte>`) with 1, 2, or 4 bytes per sample the source pixel
format. For example, an image with 2-bits per sample is exported as an 8-bit
image.

TiffCraft is header-only and can be consumed by copying the headers into
your project. No build step required: no compiled artifact, no link step, and
no CMake needed (CMake-based consumption is documented separately).

# Which headers do you need?

TiffCraft is header-only; copy the headers from `include/tiffcraft/` into
your project and include them directly. No CMake required (CMake-based
consumption is documented separately).

- `TiffImage.hpp`: core TIFF reading; self-contained.
- `TiffExporter.hpp`: flat-buffer pixel decoding; requires `TiffImage.hpp`.
- `TiffStdOut.hpp`: metadata output/printing; requires `TiffImage.hpp`.
- `TiffCraft.hpp`: umbrella header; includes everything.

`TiffImage.hpp` is self-contained; the other headers assume the full
`tiffcraft/` tree or their documented dependencies.

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

# CMake consumption

## FetchContent

TiffCraft is header-only and can be consumed directly from its git
repository with CMake's `FetchContent` module:

```cmake
include(FetchContent)

FetchContent_Declare(
  TiffCraft
  GIT_REPOSITORY https://github.com/danielm5/tiffcraft.git
  GIT_TAG <release tag>   # e.g. the latest release tag
)
FetchContent_MakeAvailable(TiffCraft)

target_link_libraries(my_app PRIVATE TiffCraft::TiffCraft)
```

When consumed this way, the `TIFFCRAFT_BUILD_ALL` option defaults to `OFF`,
so the library's tests, apps, and examples are not built. The same applies
when consuming via `add_subdirectory` from a local checkout.

## find_package

To consume a CMake-installed TiffCraft package:

```sh
cmake --install /path/to/tiffcraft/build/gcc-linux-release --prefix /path/to/install
```

```cmake
find_package(TiffCraft REQUIRED)

target_link_libraries(my_app PRIVATE TiffCraft::TiffCraft)
```

The installed package provides the `TiffCraft::TiffCraft` namespaced target and
the public headers under `include/tiffcraft`.

# Feedback

Feedback is welcomed! Please don't hesitate to reach out by opening a
[GitHub issue](https://github.com/danielm5/tiffcraft/issues/new/choose)
if you have suggestions, feature requests, or bug reports.

Please let me know if you find this project useful!
