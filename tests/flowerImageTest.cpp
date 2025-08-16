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

TEST_CASE("FlowerImageGray8Test", "[flower_image][gray8]") {
  LoadParams loadParams{ 0 };
  TiffExporterGray8 exporter;
  const std::string filename = getTestFilePath("flower-minisblack-08.tif");
  load(filename, std::ref(exporter), loadParams);

  checkImageFields(exporter, 8, 1);

  // compare pixel data
  const uint8_t* data1 = FlowerImage::data;
  const uint8_t* data2 = exporter.image().dataPtr<uint8_t>();
  for (size_t i = 0; i < FlowerImage::numPixels; ++i) {
    uint8_t pixel1[3];
    FlowerImage::nextPixel(data1, pixel1);
    uint8_t gray1 = static_cast<uint8_t>(0.299f * pixel1[0] + 0.587f * pixel1[1] + 0.114f * pixel1[2]);
    REQUIRE(static_cast<double>(data2[i]) == Catch::Approx(gray1).margin(3));
  }
}

TEST_CASE("FlowerImageGray16Test", "[flower_image][gray16]") {
  LoadParams loadParams{ 0 };
  TiffExporterGray16 exporter;
  const std::string filename = getTestFilePath("flower-minisblack-16.tif");
  load(filename, std::ref(exporter), loadParams);

  checkImageFields(exporter, 16, 1);

  // compare pixel data
  const uint8_t* data1 = FlowerImage::data;
  const uint16_t* data2 = exporter.image().dataPtr<uint16_t>();
  for (size_t i = 0; i < FlowerImage::numPixels; ++i) {
    uint8_t pixel1[3];
    FlowerImage::nextPixel(data1, pixel1);
    uint8_t gray1 = static_cast<uint8_t>(0.299f * pixel1[0] + 0.587f * pixel1[1] + 0.114f * pixel1[2]);
    REQUIRE(static_cast<double>(data2[i] >> 8) == Catch::Approx(gray1).margin(4));
  }
}

TEST_CASE("FlowerImageGray16Test", "[flower_image][gray32]") {
  LoadParams loadParams{ 0 };
  TiffExporterGray32 exporter;
  const std::string filename = getTestFilePath("flower-minisblack-32.tif");
  load(filename, std::ref(exporter), loadParams);

  checkImageFields(exporter, 32, 1);

  // compare pixel data
  const uint8_t* data1 = FlowerImage::data;
  const uint32_t* data2 = exporter.image().dataPtr<uint32_t>();
  for (size_t i = 0; i < FlowerImage::numPixels; ++i) {
    uint8_t pixel1[3];
    FlowerImage::nextPixel(data1, pixel1);
    uint8_t gray1 = static_cast<uint8_t>(0.299f * pixel1[0] + 0.587f * pixel1[1] + 0.114f * pixel1[2]);
    REQUIRE(static_cast<double>(data2[i] >> 24) == Catch::Approx(gray1).margin(4));
  }
}

TEST_CASE("TiffExporterGrayUpTo8bitsTest", "[flower_image][gray-1-2-4-8-bits]") {
  LoadParams loadParams{ 0 };
  TiffExporterGrayUpTo8bits exporter;
  const std::string filename = getTestFilePath("flower-minisblack-02.tif");
  load(filename, std::ref(exporter), loadParams);

  checkImageFields(exporter, 8, 1);

  // compare pixel data
  const uint8_t* data1 = FlowerImage::data;
  const uint8_t* data2 = exporter.image().dataPtr<uint8_t>();
  for (size_t i = 0; i < FlowerImage::numPixels; ++i) {
    uint8_t pixel1[3];
    FlowerImage::nextPixel(data1, pixel1);
    uint8_t gray1 = static_cast<uint8_t>(0.299f * pixel1[0] + 0.587f * pixel1[1] + 0.114f * pixel1[2]);
    REQUIRE(static_cast<double>(data2[i] >> 6) == Catch::Approx(gray1 >> 6).margin(1));
  }
}

TEST_CASE("TiffExporterGrayBitsTest", "[flower_image][gray-bits]") {
  LoadParams loadParams{ 0 };
  TiffExporterGrayBits exporter;
  std::vector<std::string> testFiles = {
    "flower-minisblack-02.tif",
    "flower-minisblack-04.tif",
    "flower-minisblack-06.tif",
    "flower-minisblack-08.tif"
  };
  for (auto& testFile : testFiles) {
    std::cout << "Testing file: " << testFile << std::endl;
    const std::string filename = getTestFilePath(testFile);
    load(filename, std::ref(exporter), loadParams);

    checkImageFields(exporter, 8, 1);

    const int bitsPerPixel = std::stoi(testFile.substr(18, 2));
    const int margin = (bitsPerPixel < 8 ? 1 : 3);

    // compare pixel data
    const uint8_t* data1 = FlowerImage::data;
    const uint8_t* data2 = exporter.image().dataPtr<uint8_t>();
    for (size_t i = 0; i < FlowerImage::numPixels; ++i) {
      uint8_t pixel1[3];
      FlowerImage::nextPixel(data1, pixel1);
      uint8_t gray1 = static_cast<uint8_t>(0.299f * pixel1[0] + 0.587f * pixel1[1] + 0.114f * pixel1[2]);
      REQUIRE(static_cast<double>(data2[i] >> (8 - bitsPerPixel))
        == Catch::Approx(gray1 >> (8 - bitsPerPixel)).margin(margin));
    }
  }
}

TEST_CASE("FlowerImageRgb8Test", "[flower_image][rgb8]") {
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

TEST_CASE("FlowerImageRgb16Test", "[flower_image][rgb16]") {
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

TEST_CASE("FlowerImageRgb32Test", "[flower_image][rgb32]") {
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