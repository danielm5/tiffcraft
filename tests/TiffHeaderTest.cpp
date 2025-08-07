// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#include "TiffCraft.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace TiffCraft;

std::string getTestFilePath(const std::string& filename) {
  std::filesystem::path test_file_path(__FILE__);
  std::filesystem::path test_dir = test_file_path.parent_path();
  return (test_dir / "libtiff-pics" / filename).string();
}

TEST_CASE("TiffImage Header Size", "[tiff_header]") {

  // Define the expected structure of the TIFF header
  // It should be 8 bytes in total:
  struct HeaderBytes {
      uint16_t byteOrder;       // 0x4949 for "II", 0x4D4D for "MM"
      uint16_t magicNumber;     // 42
      uint32_t firstIFDOffset;  // Offset of the first IFD

      HeaderBytes(uint16_t bo = 0x4949, uint16_t mn = 42, uint32_t ifdOffset = 8)
          : byteOrder(bo), magicNumber(mn), firstIFDOffset(ifdOffset)
      {
        if (isHostLittleEndian() && bo == 0x4D4D // "MM" big-endian
            || isHostBigEndian() && bo == 0x4949) { // "II" little-endian
          magicNumber = swap16(magicNumber);
          firstIFDOffset = swap32(firstIFDOffset);
        }
      }

      std::ostream& write(std::ostream& os) const {
        os.write(reinterpret_cast<const char*>(&byteOrder), sizeof(byteOrder));
        os.write(reinterpret_cast<const char*>(&magicNumber), sizeof(magicNumber));
        os.write(reinterpret_cast<const char*>(&firstIFDOffset), sizeof(firstIFDOffset));
        return os;
      }
  };
  SECTION("HeaderBytes size is 8 bytes") {
      static_assert(sizeof(HeaderBytes) == 8, "Header size must be 8 bytes");
  }

  { // Test little-endian header
    HeaderBytes headerBytes{ 0x4949, 42, 8 };
    std::stringstream stream;
    headerBytes.write(stream);

    TiffImage::Header header = TiffImage::Header::read(stream);
    REQUIRE(header.isLittleEndian() == true);
    REQUIRE(header.isBigEndian() == false);
    REQUIRE(header.equalsHostByteOrder() == isHostLittleEndian());
  }

  { // Test big-endian header
    HeaderBytes headerBytes{ 0x4D4D, 42, 8 };
    std::stringstream stream;
    headerBytes.write(stream);

    TiffImage::Header header = TiffImage::Header::read(stream);
    REQUIRE(header.isLittleEndian() == false);
    REQUIRE(header.isBigEndian() == true);
    REQUIRE(header.equalsHostByteOrder() == isHostBigEndian());
  }

  // TODO: Add more tests for invalid headers, such as:
  // - Invalid byte order
  // - Invalid magic number
  // - Invalid first IFD offset

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