// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#include "TiffCraft.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <filesystem>
#include <fstream>

using namespace TiffCraft;

std::string getTestFilePath(const std::string& filename) {
  std::filesystem::path test_file_path(__FILE__);
  std::filesystem::path test_dir = test_file_path.parent_path();
  return (test_dir / "libtiff-pics" / filename).string();
}

TEST_CASE("TiffImage Header Size", "[tiff_header]") {
  SECTION("Header size is 8 bytes") {
      static_assert(sizeof(TiffImage::Header) == 8, "Header size must be 8 bytes");
  }

  { // Test little-endian header
    TiffImage::Header header;
    header.byteOrder = 0x4949; // "II"
    header.magicNumber = 42;
    header.firstIFDOffset = 8;

    REQUIRE(header.isLittleEndian() == true);
    REQUIRE(header.isBigEndian() == false);
    REQUIRE(header.equalsHostByteOrder() == isHostLittleEndian());
  }

  { // Test big-endian header
    TiffImage::Header header;
    header.byteOrder = 0x4D4D; // "MM"
    header.magicNumber = 42;
    header.firstIFDOffset = 8;

    REQUIRE(header.isLittleEndian() == false);
    REQUIRE(header.isBigEndian() == true);
    REQUIRE(header.equalsHostByteOrder() == isHostBigEndian());
  }

  {
    TiffImage::Header header;
    header.byteOrder = 0x4D4D; // "MM"
    header.magicNumber = 42;
    header.firstIFDOffset = 8;

    REQUIRE(header.isLittleEndian() == false);
    REQUIRE(header.isBigEndian() == true);
  }

  { // fax2d.tif: big-endian
    const std::string test_file = "fax2d.tif";
    std::cout << "Test file path: " << getTestFilePath(test_file) << std::endl;
    std::ifstream file(getTestFilePath(test_file), std::ios::binary);
    REQUIRE(file.is_open());
    REQUIRE(file.good());
    auto header = TiffImage::Header::read(file);
    std::cout << "header: " << header << std::endl;
  }
}