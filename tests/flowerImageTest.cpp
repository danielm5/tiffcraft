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

TEST_CASE("FlowerImageGray8Test", "[flower_image][gray8]") {

  const std::string filename = getTestFilePath("flower-minisblack-08.tif");

  LoadParams loadParams;
  loadParams.ifdIndex = 0; // Load the first IFD by default

  TiffExporterGray8 exporter;

  load(filename, std::ref(exporter), loadParams);

  REQUIRE(exporter.image().width == FlowerImage::width);
  REQUIRE(exporter.image().height == FlowerImage::height);
  REQUIRE(exporter.image().channels == 1);
  REQUIRE(exporter.image().bitDepth == 8);
  REQUIRE(exporter.image().dataSize() == 1 * FlowerImage::numPixels);
  REQUIRE(exporter.image().dataPtr() != nullptr);

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

TEST_CASE("FlowerImageRgb8Test", "[flower_image][rgb8]") {

  const std::string filename = getTestFilePath("flower-rgb-contig-08.tif");

  LoadParams loadParams;
  loadParams.ifdIndex = 0; // Load the first IFD by default

  TiffExporterRgb8 exporter;

  load(filename, std::ref(exporter), loadParams);

  REQUIRE(exporter.image().width == FlowerImage::width);
  REQUIRE(exporter.image().height == FlowerImage::height);
  REQUIRE(exporter.image().channels == 3);
  REQUIRE(exporter.image().bitDepth == 8);
  REQUIRE(exporter.image().dataSize() == 3 * FlowerImage::numPixels);
  REQUIRE(exporter.image().dataPtr() != nullptr);

  // compare pixel data
  const uint8_t* data1 = FlowerImage::data;
  const Rgb* data2 = exporter.image().dataPtr<Rgb>();
  for (size_t i = 0; i < FlowerImage::numPixels; ++i) {
    uint8_t pixel1[3];
    FlowerImage::nextPixel(data1, pixel1);
    REQUIRE(data2[i].r == pixel1[0]);
    REQUIRE(data2[i].g == pixel1[1]);
    REQUIRE(data2[i].b == pixel1[2]);
  }

}
