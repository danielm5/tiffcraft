// BSD 2-Clause License
// Copyright (c) 2025-2026 Daniel Moreno
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// ---------------------------------------------------------------------------
//
// tiffExporterTest.cpp
// ====================
// Unit tests for <TiffExporter.hpp> functions.
//

#include <tiffcraft/TiffExporter.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define DOCTEST_CONFIG_DOUBLE_STRINGIFY
#include "doctest.h"

#include <string>
#include <filesystem>
#include <iostream>
#include <functional>
#include <vector>
#include <sstream>

#include "netpbm.hpp"

using namespace TiffCraft;

std::filesystem::path getFilePath(std::filesystem::path relativePath) {
  std::filesystem::path test_file_path(__FILE__);
  std::filesystem::path test_dir = test_file_path.parent_path();
  return test_dir / relativePath;
}

template <typename PixelType>
void compareToReference(
  const Image& image,
  const std::filesystem::path& refFilePath,
  int margin = 0)
{
  auto refImg = netpbm::read<PixelType>(refFilePath.string());
  const int channels = netpbm::is_rgb_v<PixelType> ? 3 : 1;
  const int bitDepth = 8 * sizeof(PixelType) / channels;
  REQUIRE (image.width == refImg.width);
  REQUIRE (image.height == refImg.height);
  REQUIRE (image.channels == channels);
  REQUIRE (image.bitDepth == bitDepth);
  REQUIRE (image.dataSize() == channels * refImg.width * refImg.height * (bitDepth / 8));
  REQUIRE (image.dataSize<PixelType>() == refImg.pixels.size());
  REQUIRE (image.dataPtr() != nullptr);
  if constexpr (netpbm::is_rgb_v<PixelType>) {
    using T = typename PixelType::value_type;
    const auto* pixels = image.dataPtr<T>();
    const auto* refPixels = refImg.pixels.data();
    for (size_t h = 0; h < image.height; ++h) {
      const auto* row = pixels + h * image.rowStride / sizeof(T);
      const auto* refRow = refPixels + h * refImg.width;
      for (size_t col = 0; col < image.width; ++col) {
        const auto* pixel = row + col * image.colStride / sizeof(T);
        const auto red = pixel[0 * image.chanStride / sizeof(T)];
        const auto green = pixel[1 * image.chanStride / sizeof(T)];
        const auto blue = pixel[2 * image.chanStride / sizeof(T)];
        REQUIRE (std::abs(double(refRow[col].r) - red) <= margin);
        REQUIRE (std::abs(double(refRow[col].g) - green) <= margin);
        REQUIRE (std::abs(double(refRow[col].b) - blue) <= margin);
      }
    }
  } else if constexpr (std::is_same_v<PixelType, bool>) {
    const uint8_t* pixels = image.dataPtr<uint8_t>();
    for (size_t h = 0; h < image.height; ++h) {
      const uint8_t* row = pixels + h * image.rowStride / sizeof(uint8_t);
      for (size_t col = 0; col < image.width; ++col) {
        const uint8_t* pixel = row + col * image.colStride / sizeof(uint8_t);
        const int refPixel = refImg.pixels[h * refImg.width + col] ? 0x00 : 0xff; // false = white, true = black
        REQUIRE (refPixel == *pixel);
      }
    }
  } else {
    using T = PixelType;
    const auto* pixels = image.dataPtr<T>();
    const auto* refPixels = refImg.pixels.data();
    for (size_t h = 0; h < image.height; ++h) {
      const auto* row = pixels + h * image.rowStride / sizeof(T);
      const auto* refRow = refPixels + h * refImg.width;
      for (size_t col = 0; col < image.width; ++col) {
        const auto* pixel = row + col * image.colStride / sizeof(T);
        REQUIRE (std::abs(double(refRow[col]) - pixel[0]) <= margin);
      }
    }
  }
}

// This function takes an exporter as template parameter and a list of files.
// The list of files must be set in pairs: first a TIFF file to test, then a
// PPM, PGM, or PBM file to compare against.
template <typename Exporter>
void TestExporter(const std::vector<std::string>& testFiles,
                  std::optional<int> margin = std::nullopt)
{
  if (testFiles.size() % 2 != 0) {
    throw std::runtime_error("Test files must be in pairs.");
  }
  LoadParams loadParams{ .ifdIndex = 0 };
  Exporter exporter;
  for (size_t i = 0; i < testFiles.size() / 2; ++i) {
    const auto tiffFilePath = getFilePath(testFiles[2*i + 0]);
    const auto refFilePath = getFilePath(testFiles[2*i + 1]);
    INFO(("Test file: " + tiffFilePath.string()));
    INFO(("Reference file: " + refFilePath.string()));
    load(tiffFilePath.string(), std::ref(exporter), loadParams);

    const auto& image = exporter.image();

    // Check if the reference file exists
    if (!std::filesystem::exists(refFilePath)) {
      FAIL(("Reference file not found: " + refFilePath.string()));
    }

    try { // Gray 8bits
      compareToReference<uint8_t>(image, refFilePath, margin.value_or(1));
      continue;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // Gray 16bits
      compareToReference<uint16_t>(image, refFilePath, margin.value_or(1));
      continue;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // Gray 32bits
      compareToReference<uint32_t>(image, refFilePath, margin.value_or(0));
      continue;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // RGB 8bits
      compareToReference<netpbm::RGB8>(image, refFilePath, margin.value_or(1));
      continue;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // RGB 16bits
      compareToReference<netpbm::RGB16>(image, refFilePath, margin.value_or(1));
      continue;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // RGB 32bits
      compareToReference<netpbm::RGB32>(image, refFilePath, margin.value_or(0));
      continue;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // Bitmap 1bit
      compareToReference<bool>(image, refFilePath, margin.value_or(0));
      continue;
    } catch (const std::exception& ex) { /* ignore */ }

    FAIL(("Unsupported reference file format: " + refFilePath.string()));
  }
}

TEST_CASE("TiffExporterGrayTest") {
  { // up to 8 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-minisblack-02.tif",
      "reference_images/flower-minisblack-02.pgm",
      "libtiff-pics/depth/flower-minisblack-04.tif",
      "reference_images/flower-minisblack-04.pgm",
      "libtiff-pics/depth/flower-minisblack-06.tif",
      "reference_images/flower-minisblack-06.pgm",
      "libtiff-pics/depth/flower-minisblack-08.tif",
      "reference_images/flower-minisblack-08.pgm",
    };
    using Exporter = TiffExporterGray<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 9 to 15 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-minisblack-10.tif",
      "reference_images/flower-minisblack-10.pgm",
      "libtiff-pics/depth/flower-minisblack-12.tif",
      "reference_images/flower-minisblack-12.pgm",
      "libtiff-pics/depth/flower-minisblack-14.tif",
      "reference_images/flower-minisblack-14.pgm",
    };
    using Exporter = TiffExporterGray<uint16_t,uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 16 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-minisblack-16.tif",
      "reference_images/flower-minisblack-16.pgm",
    };
    using Exporter = TiffExporterGray<uint16_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 24 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-minisblack-24.tif",
      "reference_images/flower-minisblack-24.pgm",
    };
    using Exporter = TiffExporterGray<uint32_t,uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  {
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-minisblack-32.tif",
      "reference_images/flower-minisblack-32.pgm",
    };
    using Exporter = TiffExporterGray<uint32_t>;
    TestExporter<Exporter>(testFiles);
  }
}

TEST_CASE("TiffExporterPaletteTest") {
  { // up to 8 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-palette-02.tif",
      "reference_images/flower-palette-02.ppm",
      "libtiff-pics/depth/flower-palette-04.tif",
      "reference_images/flower-palette-04.ppm",
      "libtiff-pics/depth/flower-palette-08.tif",
      "reference_images/flower-palette-08.ppm",
    };
    using Exporter = TiffExporterPalette<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 16 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-palette-16.tif",
      "reference_images/flower-palette-16.ppm",
    };
    using Exporter = TiffExporterPalette<uint16_t>;
    TestExporter<Exporter>(testFiles);
  }
}

TEST_CASE("TiffExporterRgbTest") {
  { // up to 8 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-contig-02.tif",
      "reference_images/flower-rgb-contig-02.ppm",
      "libtiff-pics/depth/flower-rgb-contig-04.tif",
      "reference_images/flower-rgb-contig-04.ppm",
      "libtiff-pics/depth/flower-rgb-contig-08.tif",
      "reference_images/flower-rgb-contig-08.ppm",
      //"libtiff-pics/depth/flower-separated-contig-08.tif", // samplesPerPixel = 4 (not supported)
      //"reference_images/flower-separated-contig-08.ppm",
    };
    using Exporter = TiffExporterRgb<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 9 to 15 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-contig-10.tif",
      "reference_images/flower-rgb-contig-10.ppm",
      "libtiff-pics/depth/flower-rgb-contig-12.tif",
      "reference_images/flower-rgb-contig-12.ppm",
      "libtiff-pics/depth/flower-rgb-contig-14.tif",
      "reference_images/flower-rgb-contig-14.ppm",
    };
    using Exporter = TiffExporterRgb<uint16_t,uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 16 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-contig-16.tif",
      "reference_images/flower-rgb-contig-16.ppm",
    };
    using Exporter = TiffExporterRgb<uint16_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 24 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-contig-24.tif",
      "reference_images/flower-rgb-contig-24.ppm",
    };
    using Exporter = TiffExporterRgb<uint32_t,uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 32 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-contig-32.tif",
      "reference_images/flower-rgb-contig-32.ppm",
    };
    using Exporter = TiffExporterRgb<uint32_t>;
    TestExporter<Exporter>(testFiles);
  }
}

TEST_CASE("TiffExporterRgbTest") {
  { // up to 8 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-planar-02.tif",
      "reference_images/flower-rgb-planar-02.ppm",
      "libtiff-pics/depth/flower-rgb-planar-04.tif",
      "reference_images/flower-rgb-planar-04.ppm",
      "libtiff-pics/depth/flower-rgb-planar-08.tif",
      "reference_images/flower-rgb-planar-08.ppm",
      // "libtiff-pics/depth/flower-separated-planar-08.tif", // samplesPerPixel = 4 (not supported)
      // "reference_images/flower-separated-planar-08.ppm",
    };
    using Exporter = TiffExporterRgb<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 9 to 15 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-planar-10.tif",
      "reference_images/flower-rgb-planar-10.ppm",
      "libtiff-pics/depth/flower-rgb-planar-12.tif",
      "reference_images/flower-rgb-planar-12.ppm",
      "libtiff-pics/depth/flower-rgb-planar-14.tif",
      "reference_images/flower-rgb-planar-14.ppm",
    };
    using Exporter = TiffExporterRgb<uint16_t,uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 16 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-planar-16.tif",
      "reference_images/flower-rgb-planar-16.ppm",
    };
    using Exporter = TiffExporterRgb<uint16_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 24 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-planar-24.tif",
      "reference_images/flower-rgb-planar-24.ppm",
    };
    using Exporter = TiffExporterRgb<uint32_t,uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // 32 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-planar-32.tif",
      "reference_images/flower-rgb-planar-32.ppm",
    };
    using Exporter = TiffExporterRgb<uint32_t>;
    TestExporter<Exporter>(testFiles);
  }
}

TEST_CASE("TiffExporterYCbCrTest") {
  { // YCbCr 8 bits - Chunky images
    std::vector<std::string> testFiles = {
      "ycbcr_images/ycbcr-chunky-1x1.tif", "reference_images/ycbcr-chunky-1x1.ppm",
      "ycbcr_images/ycbcr-chunky-2x1.tif", "reference_images/ycbcr-chunky-2x1.ppm",
      "ycbcr_images/ycbcr-chunky-2x2.tif", "reference_images/ycbcr-chunky-2x2.ppm",
      "ycbcr_images/ycbcr-chunky-4x1.tif", "reference_images/ycbcr-chunky-4x1.ppm",
      "ycbcr_images/ycbcr-chunky-4x2.tif", "reference_images/ycbcr-chunky-4x2.ppm",
      "ycbcr_images/ycbcr-chunky-4x4.tif", "reference_images/ycbcr-chunky-4x4.ppm",
    };
    using Exporter = TiffExporterYCbCr<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // YCbCr 8 bits - Planar images
    std::vector<std::string> testFiles = {
      // packed planar images
      "ycbcr_images/ycbcr-planar-packed-1x1.tif", "reference_images/ycbcr-planar-packed-1x1.ppm",
      "ycbcr_images/ycbcr-planar-packed-2x1.tif", "reference_images/ycbcr-planar-packed-2x1.ppm",
      "ycbcr_images/ycbcr-planar-packed-2x2.tif", "reference_images/ycbcr-planar-packed-2x2.ppm",
      "ycbcr_images/ycbcr-planar-packed-4x1.tif", "reference_images/ycbcr-planar-packed-4x1.ppm",
      "ycbcr_images/ycbcr-planar-packed-4x2.tif", "reference_images/ycbcr-planar-packed-4x2.ppm",
      "ycbcr_images/ycbcr-planar-packed-4x4.tif", "reference_images/ycbcr-planar-packed-4x4.ppm",
      // padded planar images
      "ycbcr_images/ycbcr-planar-padded-2x1.tif", "reference_images/ycbcr-planar-padded-2x1.ppm",
      "ycbcr_images/ycbcr-planar-padded-2x2.tif", "reference_images/ycbcr-planar-padded-2x2.ppm",
      "ycbcr_images/ycbcr-planar-padded-4x1.tif", "reference_images/ycbcr-planar-padded-4x1.ppm",
      "ycbcr_images/ycbcr-planar-padded-4x2.tif", "reference_images/ycbcr-planar-padded-4x2.ppm",
      "ycbcr_images/ycbcr-planar-padded-4x4.tif", "reference_images/ycbcr-planar-padded-4x4.ppm",
    };
    using Exporter = TiffExporterYCbCr<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // YCbCr 8 bits - Tile images
    std::vector<std::string> testFiles = {
      // chunky tile images
      "ycbcr_images/ycbcr-chunky-tiled-1x1.tif", "reference_images/ycbcr-chunky-tiled-1x1.ppm",
      "ycbcr_images/ycbcr-chunky-tiled-2x1.tif", "reference_images/ycbcr-chunky-tiled-2x1.ppm",
      "ycbcr_images/ycbcr-chunky-tiled-2x2.tif", "reference_images/ycbcr-chunky-tiled-2x2.ppm",
      "ycbcr_images/ycbcr-chunky-tiled-4x1.tif", "reference_images/ycbcr-chunky-tiled-4x1.ppm",
      "ycbcr_images/ycbcr-chunky-tiled-4x2.tif", "reference_images/ycbcr-chunky-tiled-4x2.ppm",
      "ycbcr_images/ycbcr-chunky-tiled-4x4.tif", "reference_images/ycbcr-chunky-tiled-4x4.ppm",
      // planar tile images
      "ycbcr_images/ycbcr-planar-tiled-1x1.tif", "reference_images/ycbcr-planar-tiled-1x1.ppm",
      "ycbcr_images/ycbcr-planar-tiled-2x1.tif", "reference_images/ycbcr-planar-tiled-2x1.ppm",
      "ycbcr_images/ycbcr-planar-tiled-2x2.tif", "reference_images/ycbcr-planar-tiled-2x2.ppm",
      "ycbcr_images/ycbcr-planar-tiled-4x1.tif", "reference_images/ycbcr-planar-tiled-4x1.ppm",
      "ycbcr_images/ycbcr-planar-tiled-4x2.tif", "reference_images/ycbcr-planar-tiled-4x2.ppm",
      "ycbcr_images/ycbcr-planar-tiled-4x4.tif", "reference_images/ycbcr-planar-tiled-4x4.ppm",
    };
    using Exporter = TiffExporterYCbCr<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // YCbCr 8 bits - Non-default color mapping
    std::vector<std::string> testFiles = {
      "ycbcr_images/ycbcr-color-coeffs-bt709.tif", "reference_images/ycbcr-color-coeffs-bt709.ppm",
      "ycbcr_images/ycbcr-color-refbw-limited-range.tif", "reference_images/ycbcr-color-refbw-limited-range.ppm",
    };
    using Exporter = TiffExporterYCbCr<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
}

TEST_CASE("TiffExporterLibTiffPicsTest") {
  { // 1 bit
    std::vector<std::string> testFiles = {
      "libtiff-pics/jim___ah.tif",
      "reference_images/jim___ah.pbm",
    };
    using Exporter = TiffExporterGray<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // Gray 8 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/jim___cg.tif",
      "reference_images/jim___cg.pgm",
      "libtiff-pics/jim___dg.tif",
      "reference_images/jim___dg.pgm",
      "libtiff-pics/jim___gg.tif",
      "reference_images/jim___gg.pgm",
    };
    using Exporter = TiffExporterGray<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // RGB 8 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/pc260001.tif",
      "reference_images/pc260001.ppm",
    };
    using Exporter = TiffExporterRgb<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // Tiled gray 8 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/cramps-tile.tif",
      "reference_images/cramps-tile.pgm",
    };
    using Exporter = TiffExporterGray<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
  { // YCbCr 8 bits uncompressed
    std::vector<std::string> testFiles = {
      "libtiff-pics/dscf0013.tif",
      "reference_images/dscf0013.ppm",
    };
    using Exporter = TiffExporterYCbCr<uint8_t>;
    TestExporter<Exporter>(testFiles);
  }
}

TEST_CASE("TiffExporterAnyTest") {
  { // Grayscale images
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-minisblack-02.tif",
      "reference_images/flower-minisblack-02.pgm",
      "libtiff-pics/depth/flower-minisblack-04.tif",
      "reference_images/flower-minisblack-04.pgm",
      "libtiff-pics/depth/flower-minisblack-06.tif",
      "reference_images/flower-minisblack-06.pgm",
      "libtiff-pics/depth/flower-minisblack-08.tif",
      "reference_images/flower-minisblack-08.pgm",
      "libtiff-pics/depth/flower-minisblack-10.tif",
      "reference_images/flower-minisblack-10.pgm",
      "libtiff-pics/depth/flower-minisblack-12.tif",
      "reference_images/flower-minisblack-12.pgm",
      "libtiff-pics/depth/flower-minisblack-14.tif",
      "reference_images/flower-minisblack-14.pgm",
      "libtiff-pics/depth/flower-minisblack-16.tif",
      "reference_images/flower-minisblack-16.pgm",
      "libtiff-pics/depth/flower-minisblack-24.tif",
      "reference_images/flower-minisblack-24.pgm",
      "libtiff-pics/depth/flower-minisblack-32.tif",
      "reference_images/flower-minisblack-32.pgm",
    };
    TestExporter<TiffExporterAny>(testFiles);
  }
  { // Palette-color images
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-palette-02.tif",
      "reference_images/flower-palette-02.ppm",
      "libtiff-pics/depth/flower-palette-04.tif",
      "reference_images/flower-palette-04.ppm",
      "libtiff-pics/depth/flower-palette-08.tif",
      "reference_images/flower-palette-08.ppm",
      "libtiff-pics/depth/flower-palette-16.tif",
      "reference_images/flower-palette-16.ppm",
    };
    TestExporter<TiffExporterAny>(testFiles);
  }
  { // RGB images (flower)
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-contig-02.tif",
      "reference_images/flower-rgb-contig-02.ppm",
      "libtiff-pics/depth/flower-rgb-contig-04.tif",
      "reference_images/flower-rgb-contig-04.ppm",
      "libtiff-pics/depth/flower-rgb-contig-08.tif",
      "reference_images/flower-rgb-contig-08.ppm",
      //"libtiff-pics/depth/flower-separated-contig-08.tif", // samplesPerPixel = 4 (not supported)
      //"reference_images/flower-separated-contig-08.ppm",
      "libtiff-pics/depth/flower-rgb-contig-10.tif",
      "reference_images/flower-rgb-contig-10.ppm",
      "libtiff-pics/depth/flower-rgb-contig-12.tif",
      "reference_images/flower-rgb-contig-12.ppm",
      "libtiff-pics/depth/flower-rgb-contig-14.tif",
      "reference_images/flower-rgb-contig-14.ppm",
      "libtiff-pics/depth/flower-rgb-contig-16.tif",
      "reference_images/flower-rgb-contig-16.ppm",
      "libtiff-pics/depth/flower-rgb-contig-24.tif",
      "reference_images/flower-rgb-contig-24.ppm",
      "libtiff-pics/depth/flower-rgb-contig-32.tif",
      "reference_images/flower-rgb-contig-32.ppm",
    };
    TestExporter<TiffExporterAny>(testFiles);
  }
  { // RGB images planar (flower)
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-planar-02.tif",
      "reference_images/flower-rgb-planar-02.ppm",
      "libtiff-pics/depth/flower-rgb-planar-04.tif",
      "reference_images/flower-rgb-planar-04.ppm",
      "libtiff-pics/depth/flower-rgb-planar-08.tif",
      "reference_images/flower-rgb-planar-08.ppm",
      // "libtiff-pics/depth/flower-separated-planar-08.tif", // samplesPerPixel = 4 (not supported)
      // "reference_images/flower-separated-planar-08.ppm",
      "libtiff-pics/depth/flower-rgb-planar-10.tif",
      "reference_images/flower-rgb-planar-10.ppm",
      "libtiff-pics/depth/flower-rgb-planar-12.tif",
      "reference_images/flower-rgb-planar-12.ppm",
      "libtiff-pics/depth/flower-rgb-planar-14.tif",
      "reference_images/flower-rgb-planar-14.ppm",
      "libtiff-pics/depth/flower-rgb-planar-16.tif",
      "reference_images/flower-rgb-planar-16.ppm",
      "libtiff-pics/depth/flower-rgb-planar-24.tif",
      "reference_images/flower-rgb-planar-24.ppm",
      "libtiff-pics/depth/flower-rgb-planar-32.tif",
      "reference_images/flower-rgb-planar-32.ppm",
    };
    TestExporter<TiffExporterAny>(testFiles);
  }
  { // libtiff-pics
    std::vector<std::string> testFiles = {
      "libtiff-pics/jim___ah.tif",
      "reference_images/jim___ah.pbm",
      "libtiff-pics/jim___cg.tif",
      "reference_images/jim___cg.pgm",
      "libtiff-pics/jim___dg.tif",
      "reference_images/jim___dg.pgm",
      "libtiff-pics/jim___gg.tif",
      "reference_images/jim___gg.pgm",
      "libtiff-pics/pc260001.tif",
      "reference_images/pc260001.ppm",
      "libtiff-pics/cramps-tile.tif",
      "reference_images/cramps-tile.pgm",
    };
    TestExporter<TiffExporterAny>(testFiles);
  }
  { // YCbCr 8 bits - Chunky images
    std::vector<std::string> testFiles = {
      "ycbcr_images/ycbcr-chunky-1x1.tif", "reference_images/ycbcr-chunky-1x1.ppm",
      "ycbcr_images/ycbcr-chunky-2x1.tif", "reference_images/ycbcr-chunky-2x1.ppm",
      "ycbcr_images/ycbcr-chunky-2x2.tif", "reference_images/ycbcr-chunky-2x2.ppm",
      "ycbcr_images/ycbcr-chunky-4x1.tif", "reference_images/ycbcr-chunky-4x1.ppm",
      "ycbcr_images/ycbcr-chunky-4x2.tif", "reference_images/ycbcr-chunky-4x2.ppm",
      "ycbcr_images/ycbcr-chunky-4x4.tif", "reference_images/ycbcr-chunky-4x4.ppm",
    };
    TestExporter<TiffExporterAny>(testFiles);
  }
  { // YCbCr 8 bits - Planar images
    std::vector<std::string> testFiles = {
      // packed planar images
      "ycbcr_images/ycbcr-planar-packed-1x1.tif", "reference_images/ycbcr-planar-packed-1x1.ppm",
      "ycbcr_images/ycbcr-planar-packed-2x1.tif", "reference_images/ycbcr-planar-packed-2x1.ppm",
      "ycbcr_images/ycbcr-planar-packed-2x2.tif", "reference_images/ycbcr-planar-packed-2x2.ppm",
      "ycbcr_images/ycbcr-planar-packed-4x1.tif", "reference_images/ycbcr-planar-packed-4x1.ppm",
      "ycbcr_images/ycbcr-planar-packed-4x2.tif", "reference_images/ycbcr-planar-packed-4x2.ppm",
      "ycbcr_images/ycbcr-planar-packed-4x4.tif", "reference_images/ycbcr-planar-packed-4x4.ppm",
      // padded planar images
      "ycbcr_images/ycbcr-planar-padded-2x1.tif", "reference_images/ycbcr-planar-padded-2x1.ppm",
      "ycbcr_images/ycbcr-planar-padded-2x2.tif", "reference_images/ycbcr-planar-padded-2x2.ppm",
      "ycbcr_images/ycbcr-planar-padded-4x1.tif", "reference_images/ycbcr-planar-padded-4x1.ppm",
      "ycbcr_images/ycbcr-planar-padded-4x2.tif", "reference_images/ycbcr-planar-padded-4x2.ppm",
      "ycbcr_images/ycbcr-planar-padded-4x4.tif", "reference_images/ycbcr-planar-padded-4x4.ppm",
    };
    TestExporter<TiffExporterAny>(testFiles);
  }
  { // YCbCr 8 bits - Tile images
    std::vector<std::string> testFiles = {
      // chunky tile images
      "ycbcr_images/ycbcr-chunky-tiled-1x1.tif", "reference_images/ycbcr-chunky-tiled-1x1.ppm",
      "ycbcr_images/ycbcr-chunky-tiled-2x1.tif", "reference_images/ycbcr-chunky-tiled-2x1.ppm",
      "ycbcr_images/ycbcr-chunky-tiled-2x2.tif", "reference_images/ycbcr-chunky-tiled-2x2.ppm",
      "ycbcr_images/ycbcr-chunky-tiled-4x1.tif", "reference_images/ycbcr-chunky-tiled-4x1.ppm",
      "ycbcr_images/ycbcr-chunky-tiled-4x2.tif", "reference_images/ycbcr-chunky-tiled-4x2.ppm",
      "ycbcr_images/ycbcr-chunky-tiled-4x4.tif", "reference_images/ycbcr-chunky-tiled-4x4.ppm",
      // planar tile images
      "ycbcr_images/ycbcr-planar-tiled-1x1.tif", "reference_images/ycbcr-planar-tiled-1x1.ppm",
      "ycbcr_images/ycbcr-planar-tiled-2x1.tif", "reference_images/ycbcr-planar-tiled-2x1.ppm",
      "ycbcr_images/ycbcr-planar-tiled-2x2.tif", "reference_images/ycbcr-planar-tiled-2x2.ppm",
      "ycbcr_images/ycbcr-planar-tiled-4x1.tif", "reference_images/ycbcr-planar-tiled-4x1.ppm",
      "ycbcr_images/ycbcr-planar-tiled-4x2.tif", "reference_images/ycbcr-planar-tiled-4x2.ppm",
      "ycbcr_images/ycbcr-planar-tiled-4x4.tif", "reference_images/ycbcr-planar-tiled-4x4.ppm",
    };
    TestExporter<TiffExporterAny>(testFiles);
  }
  { // YCbCr 8 bits - Non-default color mapping
    std::vector<std::string> testFiles = {
      "ycbcr_images/ycbcr-color-coeffs-bt709.tif", "reference_images/ycbcr-color-coeffs-bt709.ppm",
      "ycbcr_images/ycbcr-color-refbw-limited-range.tif", "reference_images/ycbcr-color-refbw-limited-range.ppm",
    };
    TestExporter<TiffExporterAny>(testFiles);
  }
  { // YCbCr 8 bits uncompressed
    std::vector<std::string> testFiles = {
      "libtiff-pics/dscf0013.tif",
      "reference_images/dscf0013.ppm",
    };
    TestExporter<TiffExporterAny>(testFiles);
  }
}

TEST_CASE("TiffExporterAny takeImage") {
  TiffExporterAny exporter;
  load(getFilePath("libtiff-pics/jim___ah.tif").string(), std::ref(exporter));

  CHECK(exporter.image().dataSize() > 0);

  auto image = exporter.takeImage();
  CHECK(exporter.image().data.size() == 0);
  CHECK(image.dataSize() > 0);
}

TEST_CASE("TiffExporterAny multi-IFD export") {
  // Synthesize a 2-IFD 8-bit grayscale TIFF in-memory:
  //   IFD1 at offset 8,   strip pixels at offset 122 (10, 20, 30, 40)
  //   IFD2 at offset 126, strip pixels at offset 240 (200, 210, 220, 230)
  constexpr uint32_t W = 2, H = 2;
  constexpr uint32_t ifdSize = 2 + 9 * 12 + 4; // 2-byte count, 9 entries, 4-byte next IFD offset
  constexpr uint32_t ifd1Offset = 8;
  constexpr uint32_t pix1Offset = ifd1Offset + ifdSize; // 122
  constexpr uint32_t ifd2Offset = pix1Offset + 4;       // 126
  constexpr uint32_t pix2Offset = ifd2Offset + ifdSize; // 240

  const std::vector<std::byte> pix1{ std::byte{10}, std::byte{20}, std::byte{30}, std::byte{40} };
  const std::vector<std::byte> pix2{ std::byte{200}, std::byte{210}, std::byte{220}, std::byte{230} };

  std::stringstream stream;
  auto le16 = [&stream](uint16_t v) {
    const char bytes[2] = { static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff) };
    stream.write(bytes, sizeof(bytes));
  };
  auto le32 = [&stream](uint32_t v) {
    const char bytes[4] = {
      static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff),
      static_cast<char>((v >> 16) & 0xff), static_cast<char>((v >> 24) & 0xff)
    };
    stream.write(bytes, sizeof(bytes));
  };
  auto entry = [&le16, &le32](uint16_t tag, Type type, uint32_t count, uint32_t value) {
    le16(tag);
    le16(static_cast<uint16_t>(type));
    le32(count);
    le32(value); // Inline value, or the offset of the value for values larger than 4 bytes
  };
  auto writeIfd = [&](uint32_t pixOffset, uint32_t nextIfd) {
    le16(9); // 9 entries
    entry(256, Type::LONG, 1, W); // ImageWidth
    entry(257, Type::LONG, 1, H); // ImageLength
    entry(258, Type::SHORT, 1, 8); // BitsPerSample (inline, low 2 bytes)
    entry(259, Type::SHORT, 1, 1); // Compression = none
    entry(262, Type::SHORT, 1, 1); // PhotometricInterpretation = BlackIsZero
    entry(273, Type::LONG, 1, pixOffset); // StripOffsets
    entry(277, Type::SHORT, 1, 1); // SamplesPerPixel
    entry(278, Type::LONG, 1, H); // RowsPerStrip
    entry(279, Type::LONG, 1, 4); // StripByteCounts
    le32(nextIfd);
  };

  // Header: little-endian, magic 42, first IFD at offset 8
  stream.write("II", 2);
  le16(42);
  le32(ifd1Offset);
  // IFD1 + pixels
  writeIfd(pix1Offset, ifd2Offset);
  stream.write(reinterpret_cast<const char*>(pix1.data()), pix1.size());
  // IFD2 + pixels
  writeIfd(pix2Offset, 0);
  stream.write(reinterpret_cast<const char*>(pix2.data()), pix2.size());

  TiffExporterAny exporter;
  load(stream, std::ref(exporter));

  // Each IFD is exported in turn; the last successfully exported image wins
  CHECK(exporter.image().dataSize() == 4);
  const auto& data = exporter.image().data;
  REQUIRE(data.size() == 4);
  CHECK(static_cast<unsigned char>(data[0]) == 200);
  CHECK(static_cast<unsigned char>(data[1]) == 210);
  CHECK(static_cast<unsigned char>(data[2]) == 220);
  CHECK(static_cast<unsigned char>(data[3]) == 230);
}
