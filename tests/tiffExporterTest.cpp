// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#include "TiffCraft.hpp"
#include "TiffExporter.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <string>
#include <filesystem>
#include <iostream>

#include "flower-rgb-contig-08.h"

using namespace TiffCraft;

std::string getTestFilePath(const std::string& filename) {
  std::filesystem::path test_file_path(__FILE__);
  std::filesystem::path test_dir = test_file_path.parent_path();
  return (test_dir / "libtiff-pics" / "depth" / filename).string();
}

void checkImageFields(const TiffExporter& exporter, int bitDepth, int channels) {
  REQUIRE(exporter.image().width == FlowerImage::width);
  REQUIRE(exporter.image().height == FlowerImage::height);
  REQUIRE(exporter.image().channels == channels);
  REQUIRE(exporter.image().bitDepth == bitDepth);
  REQUIRE(exporter.image().dataSize() == channels * FlowerImage::numPixels * (bitDepth / 8));
  REQUIRE(exporter.image().dataPtr() != nullptr);
}

template <typename DstType>
void compareImageDataGray(const Image& image, int bitsPerPixel, int margin)
{
  const int pixelBits = 8 * sizeof(DstType);
  const int shift = pixelBits > 8 ? pixelBits - 8 : 8 - bitsPerPixel;
  const uint8_t* data1 = FlowerImage::data;
  const DstType* data2 = image.dataPtr<DstType>();
  for (size_t i = 0; i < FlowerImage::numPixels; ++i) {
    uint8_t pixel1[3];
    FlowerImage::nextPixel(data1, pixel1);
    uint8_t gray1 = static_cast<uint8_t>(0.299f * pixel1[0] + 0.587f * pixel1[1] + 0.114f * pixel1[2]);
    DstType value = static_cast<DstType>(gray1) << (8 * (sizeof(DstType) - 1));
    REQUIRE(static_cast<double>(data2[i] >> shift) == Catch::Approx(value >> shift).margin(margin));
  }
}

TEST_CASE("TiffExporterGray8Test", "[flower_image][gray8]") {
  LoadParams loadParams{ 0 };
  TiffExporterGray8 exporter;
  const std::string filename = getTestFilePath("flower-minisblack-08.tif");
  load(filename, std::ref(exporter), loadParams);

  checkImageFields(exporter, 8, 1);

  INFO("Test file: " + filename);
  compareImageDataGray<uint8_t>(exporter.image(), 8, 3);
}

TEST_CASE("TiffExporterGray16Test", "[flower_image][gray16]") {
  LoadParams loadParams{ 0 };
  TiffExporterGray16 exporter;
  const std::string filename = getTestFilePath("flower-minisblack-16.tif");
  load(filename, std::ref(exporter), loadParams);

  checkImageFields(exporter, 16, 1);

  INFO("Test file: " + filename);
  compareImageDataGray<uint16_t>(exporter.image(), 16, 4);
}

TEST_CASE("FlowerImageGray32Test", "[flower_image][gray32]") {
  LoadParams loadParams{ 0 };
  TiffExporterGray32 exporter;
  const std::string filename = getTestFilePath("flower-minisblack-32.tif");
  load(filename, std::ref(exporter), loadParams);

  checkImageFields(exporter, 32, 1);

  INFO("Test file: " + filename);
  compareImageDataGray<uint32_t>(exporter.image(), 32, 4);
}

template <typename DstType, typename SrcType = DstType>
void TiffExporterGrayBitsTest(
  const std::vector<std::string>& testFiles,
  const std::vector<int> margins)
{
  if (testFiles.size() != margins.size()) {
    throw std::runtime_error("Test files and margins must have the same size.");
  }

  LoadParams loadParams{ 0 };
  TiffExporterGrayBits<DstType, SrcType> exporter;
  for (size_t i = 0; i < testFiles.size(); ++i) {
    const auto& testFile = testFiles[i];
    const int margin = margins[i];
    const int bitsPerPixel = std::stoi(testFile.substr(18, 2));
    std::cout << "Testing file: " << testFile
      << " (bits per pixel: " << bitsPerPixel << ")" << std::endl;
    const std::string filename = getTestFilePath(testFile);
    load(filename, std::ref(exporter), loadParams);

    const int pixelBits = 8 * sizeof(DstType);
    checkImageFields(exporter, pixelBits, 1);

    INFO("Test file: " + testFile);
    compareImageDataGray<DstType>(exporter.image(), bitsPerPixel, margin);
  }
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits-up-to-8]") {
  std::vector<std::string> testFiles = {
    "flower-minisblack-02.tif",
    "flower-minisblack-04.tif",
    "flower-minisblack-06.tif",
    "flower-minisblack-08.tif"
  };
  std::vector<int> margins = { 1, 1, 1, 3 };
  TiffExporterGrayBitsTest<uint8_t>(testFiles, margins);
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits-9-to-15]") {
  std::vector<std::string> testFiles = {
    "flower-minisblack-10.tif",
    "flower-minisblack-12.tif",
    "flower-minisblack-14.tif",
  };
  std::vector<int> margins = {
    3,
    4,
    4,
  };
  TiffExporterGrayBitsTest<uint16_t,uint8_t>(testFiles, margins);
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits-16]") {
  std::vector<std::string> testFiles = { "flower-minisblack-16.tif" };
  std::vector<int> margins = { 4 };
  TiffExporterGrayBitsTest<uint16_t>(testFiles, margins);
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits-24]") {
  std::vector<std::string> testFiles = { "flower-minisblack-24.tif" };
  std::vector<int> margins = { 4 };
  TiffExporterGrayBitsTest<uint32_t,uint8_t>(testFiles, margins);
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits-32]") {
  std::vector<std::string> testFiles = { "flower-minisblack-32.tif" };
  std::vector<int> margins = { 4 };
  TiffExporterGrayBitsTest<uint32_t>(testFiles, margins);
}

TEST_CASE("TiffExporterRgb8Test", "[flower_image][rgb8]") {
  LoadParams loadParams{ 0 };
  TiffExporterRgb8 exporter;
  const std::string filename = getTestFilePath("flower-rgb-contig-08.tif");
  load(filename, std::ref(exporter), loadParams);

  checkImageFields(exporter, 8, 3);

  // compare pixel data
  const uint8_t* data1 = FlowerImage::data;
  const Rgb8* data2 = exporter.image().dataPtr<Rgb8>();
  for (size_t i = 0; i < FlowerImage::numPixels; ++i) {
    uint8_t pixel1[3];
    FlowerImage::nextPixel(data1, pixel1);
    REQUIRE(data2[i].r == pixel1[0]);
    REQUIRE(data2[i].g == pixel1[1]);
    REQUIRE(data2[i].b == pixel1[2]);
  }
}

TEST_CASE("TiffExporterRgb16Test", "[flower_image][rgb16]") {
  LoadParams loadParams{ 0 };
  TiffExporterRgb16 exporter;
  const std::string filename = getTestFilePath("flower-rgb-contig-16.tif");
  load(filename, std::ref(exporter), loadParams);

  checkImageFields(exporter, 16, 3);

  // compare pixel data
  const uint8_t* data1 = FlowerImage::data;
  const Rgb16* data2 = exporter.image().dataPtr<Rgb16>();
  for (size_t i = 0; i < FlowerImage::numPixels; ++i) {
    uint8_t pixel1[3];
    FlowerImage::nextPixel(data1, pixel1);
    REQUIRE(static_cast<double>(data2[i].r >> 8) == Catch::Approx(pixel1[0]).margin(2));
    REQUIRE(static_cast<double>(data2[i].g >> 8) == Catch::Approx(pixel1[1]).margin(2));
    REQUIRE(static_cast<double>(data2[i].b >> 8) == Catch::Approx(pixel1[2]).margin(2));
  }
}

TEST_CASE("TiffExporterRgb32Test", "[flower_image][rgb32]") {
  LoadParams loadParams{ 0 };
  TiffExporterRgb32 exporter;
  const std::string filename = getTestFilePath("flower-rgb-contig-32.tif");
  load(filename, std::ref(exporter), loadParams);

  checkImageFields(exporter, 32, 3);

  // compare pixel data
  const uint8_t* data1 = FlowerImage::data;
  const Rgb32* data2 = exporter.image().dataPtr<Rgb32>();
  for (size_t i = 0; i < FlowerImage::numPixels; ++i) {
    uint8_t pixel1[3];
    FlowerImage::nextPixel(data1, pixel1);
    REQUIRE(static_cast<double>(data2[i].r >> 24) == Catch::Approx(pixel1[0]).margin(2));
    REQUIRE(static_cast<double>(data2[i].g >> 24) == Catch::Approx(pixel1[1]).margin(2));
    REQUIRE(static_cast<double>(data2[i].b >> 24) == Catch::Approx(pixel1[2]).margin(2));
  }
}