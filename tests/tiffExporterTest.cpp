// BSD 2-Clause License
// Copyright (c) 2025 Daniel Moreno
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

#include "TiffCraft.hpp"
#include "TiffExporter.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <string>
#include <filesystem>
#include <iostream>

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
  REQUIRE(image.width == refImg.width);
  REQUIRE(image.height == refImg.height);
  REQUIRE(image.channels == channels);
  REQUIRE(image.bitDepth == bitDepth);
  REQUIRE(image.dataSize() == channels * refImg.width * refImg.height * (bitDepth / 8));
  REQUIRE(image.dataSize<PixelType>() == refImg.pixels.size());
  REQUIRE(image.dataPtr() != nullptr);
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
        REQUIRE(double(refRow[col].r) == Catch::Approx(red).margin(margin));
        REQUIRE(double(refRow[col].g) == Catch::Approx(green).margin(margin));
        REQUIRE(double(refRow[col].b) == Catch::Approx(blue).margin(margin));
      }
    }
  } else if constexpr (std::is_same_v<PixelType, bool>) {
    const uint8_t* pixels = image.dataPtr<uint8_t>();
    for (size_t h = 0; h < image.height; ++h) {
      const uint8_t* row = pixels + h * image.rowStride / sizeof(uint8_t);
      for (size_t col = 0; col < image.width; ++col) {
        const uint8_t* pixel = row + col * image.colStride / sizeof(uint8_t);
        const int refPixel = refImg.pixels[h * refImg.width + col] ? 0x00 : 0xff; // false = white, true = black
        REQUIRE(refPixel == *pixel);
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
        REQUIRE(double(refRow[col]) == Catch::Approx(pixel[0]).margin(margin));
      }
    }
  }
}

// This function takes an exporter as template parameter and a list of files.
// The list of files must be set in pairs: first a TIFF file to test, then a
// PPM, PGM, or PBM file to compare against.
template <typename Exporter>
void TestExporter(const std::vector<std::string>& testFiles)
{
  if (testFiles.size() % 2 != 0) {
    throw std::runtime_error("Test files must be in pairs.");
  }
  LoadParams loadParams{ 0 };
  Exporter exporter;
  for (size_t i = 0; i < testFiles.size() / 2; ++i) {
    const auto tiffFilePath = getFilePath(testFiles[2*i + 0]);
    const auto refFilePath = getFilePath(testFiles[2*i + 1]);
    INFO("Test file: " + tiffFilePath.string());
    INFO("Reference file: " + refFilePath.string());
    load(tiffFilePath.string(), std::ref(exporter), loadParams);

    const auto& image = exporter.image();

    try { // Gray 8bits
      compareToReference<uint8_t>(image, refFilePath);
      return;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // Gray 16bits
      compareToReference<uint16_t>(image, refFilePath, 1);
      return;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // Gray 32bits
      compareToReference<uint32_t>(image, refFilePath);
      return;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // RGB 8bits
      compareToReference<netpbm::RGB8>(image, refFilePath, 1);
      return;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // RGB 16bits
      compareToReference<netpbm::RGB16>(image, refFilePath, 1);
      return;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // RGB 32bits
      compareToReference<netpbm::RGB32>(image, refFilePath);
      return;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // Bitmap 1bit
      compareToReference<bool>(image, refFilePath);
      return;
    } catch (const std::exception& ex) { /* ignore */ }

    FAIL("Unsupported reference file format: " + refFilePath.string());
  }
}

TEST_CASE("TiffExporterGrayTest", "[flower_image][flower_grayscale]") {
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

TEST_CASE("TiffExporterPaletteTest", "[flower_image][flower_palette]") {
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

TEST_CASE("TiffExporterRgbTest", "[flower_image][flower_rgb_contiguous]") {
  { // up to 8 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-contig-02.tif",
      "reference_images/flower-rgb-contig-02.ppm",
      "libtiff-pics/depth/flower-rgb-contig-04.tif",
      "reference_images/flower-rgb-contig-04.ppm",
      "libtiff-pics/depth/flower-rgb-contig-08.tif",
      "reference_images/flower-rgb-contig-08.ppm",
      "libtiff-pics/depth/flower-separated-contig-08.tif",
      "reference_images/flower-separated-contig-08.ppm",
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

TEST_CASE("TiffExporterRgbTest", "[flower_image][flower_rgb_planar]") {
  { // up to 8 bits
    std::vector<std::string> testFiles = {
      "libtiff-pics/depth/flower-rgb-planar-02.tif",
      "reference_images/flower-rgb-planar-02.ppm",
      "libtiff-pics/depth/flower-rgb-planar-04.tif",
      "reference_images/flower-rgb-planar-04.ppm",
      "libtiff-pics/depth/flower-rgb-planar-08.tif",
      "reference_images/flower-rgb-planar-08.ppm",
      "libtiff-pics/depth/flower-separated-planar-08.tif",
      "reference_images/flower-separated-planar-08.ppm",
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

TEST_CASE("TiffExporterLibTiffPicsTest", "[libtiff_pics]") {
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
}

TEST_CASE("TiffExporterAnyTest", "[flower_image][libtiff_pics][all_images]") {
  { // grayscale images
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
  { // palette-color images
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
      "libtiff-pics/depth/flower-separated-contig-08.tif",
      "reference_images/flower-separated-contig-08.ppm",
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
      "libtiff-pics/depth/flower-separated-planar-08.tif",
      "reference_images/flower-separated-planar-08.ppm",
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
}
