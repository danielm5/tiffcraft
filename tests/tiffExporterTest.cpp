// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

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
  const auto* pixels = image.dataPtr<PixelType>();
  for (size_t i = 0; i < refImg.pixels.size(); ++i) {
    if constexpr (netpbm::is_rgb_v<PixelType>) {
      REQUIRE(double(refImg.pixels[i].r) == Catch::Approx(pixels[i].r).margin(margin));
      REQUIRE(double(refImg.pixels[i].g) == Catch::Approx(pixels[i].g).margin(margin));
      REQUIRE(double(refImg.pixels[i].b) == Catch::Approx(pixels[i].b).margin(margin));
    } else {
       REQUIRE(double(refImg.pixels[i]) == Catch::Approx(pixels[i]).margin(margin));
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
      compareToReference<netpbm::RGB16>(image, refFilePath);
      return;
    } catch (const std::exception& ex) { /* ignore */ }

    try { // RGB 32bits
      compareToReference<netpbm::RGB32>(image, refFilePath);
      return;
    } catch (const std::exception& ex) { /* ignore */ }

    FAIL("Unsupported reference file format: " + refFilePath.string());
  }
}

TEST_CASE("TiffExporterGray8Test", "[flower_image][gray8]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-minisblack-08.tif",
    "reference_images/flower-minisblack-08.pgm",
  };
  using Exporter = TiffExporterGray8;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterGray16Test", "[flower_image][gray16]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-minisblack-16.tif",
    "reference_images/flower-minisblack-16.pgm",
  };
  using Exporter = TiffExporterGray16;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("FlowerImageGray32Test", "[flower_image][gray32]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-minisblack-32.tif",
    "reference_images/flower-minisblack-32.pgm",
  };
  using Exporter = TiffExporterGray32;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits-up-to-8]") {
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
  using Exporter = TiffExporterGrayBits<uint8_t>;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits-9-to-15]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-minisblack-10.tif",
    "reference_images/flower-minisblack-10.pgm",
    "libtiff-pics/depth/flower-minisblack-12.tif",
    "reference_images/flower-minisblack-12.pgm",
    "libtiff-pics/depth/flower-minisblack-14.tif",
    "reference_images/flower-minisblack-14.pgm",
  };
  using Exporter = TiffExporterGrayBits<uint16_t,uint8_t>;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits-16]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-minisblack-16.tif",
    "reference_images/flower-minisblack-16.pgm",
  };
  using Exporter = TiffExporterGrayBits<uint16_t>;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits-24]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-minisblack-24.tif",
    "reference_images/flower-minisblack-24.pgm",
  };
  using Exporter = TiffExporterGrayBits<uint32_t,uint8_t>;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits-32]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-minisblack-32.tif",
    "reference_images/flower-minisblack-32.pgm",
  };
  using Exporter = TiffExporterGrayBits<uint32_t>;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterPaletteTest", "[flower_image][palette8]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-palette-08.tif",
    "reference_images/flower-palette-08.ppm"
  };
  using Exporter = TiffExporterPalette<uint8_t>;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterPaletteTest", "[flower_image][palette16]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-palette-16.tif",
    "reference_images/flower-palette-16.ppm"
  };
  using Exporter = TiffExporterPalette<uint16_t>;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterPaletteBitsTest", "[flower_image][palette-bits-up-to-8]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-palette-02.tif",
    "reference_images/flower-palette-02.ppm",
    "libtiff-pics/depth/flower-palette-04.tif",
    "reference_images/flower-palette-04.ppm",
    "libtiff-pics/depth/flower-palette-08.tif",
    "reference_images/flower-palette-08.ppm"
  };
  using Exporter = TiffExporterPaletteBits<uint8_t>;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterPaletteBitsTest", "[flower_image][palette-bits-16]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-palette-16.tif",
    "reference_images/flower-palette-16.ppm"
  };
  using Exporter = TiffExporterPaletteBits<uint16_t>;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterRgb8Test", "[flower_image][rgb8]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-rgb-contig-08.tif",
    "reference_images/flower-rgb-contig-08.ppm"
  };
  using Exporter = TiffExporterRgb8;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterRgb16Test", "[flower_image][rgb16]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-rgb-contig-16.tif",
    "reference_images/flower-rgb-contig-16.ppm"
  };
  using Exporter = TiffExporterRgb16;
  TestExporter<Exporter>(testFiles);
}

TEST_CASE("TiffExporterRgb32Test", "[flower_image][rgb32]") {
  std::vector<std::string> testFiles = {
    "libtiff-pics/depth/flower-rgb-contig-32.tif",
    "reference_images/flower-rgb-contig-32.ppm"
  };
  using Exporter = TiffExporterRgb32;
  TestExporter<Exporter>(testFiles);
}
