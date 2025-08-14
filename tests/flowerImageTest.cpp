// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#include "TiffCraft.hpp"
#include "TiffExporter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <filesystem>

#include "flower-rgb-contig-08.h"

using namespace TiffCraft;

std::string getTestFilePath(const std::string& filename) {
  std::filesystem::path test_file_path(__FILE__);
  std::filesystem::path test_dir = test_file_path.parent_path();
  return (test_dir / "libtiff-pics" / "depth" / filename).string();
}

TEST_CASE("Flower Image Test", "[flower_image]") {

  const std::string filename = getTestFilePath("flower-rgb-contig-08.tif");

  LoadParams loadParams;
  loadParams.ifdIndex = 0; // Load the first IFD by default

  TiffExporterRgb8 exporter;

  load(filename, std::ref(exporter), loadParams);

  REQUIRE(exporter.image().width == FlowerImage::width);
  REQUIRE(exporter.image().height == FlowerImage::height);
  REQUIRE(exporter.image().channels == FlowerImage::channels);
  REQUIRE(exporter.image().bitDepth == FlowerImage::bitDepth);
  REQUIRE(exporter.image().dataSize() == FlowerImage::numPixels * FlowerImage::channels);
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
