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

TEST_CASE("TiffImage Header class", "[tiff_header]") {

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
        auto startPos = os.tellp();
        os.write(reinterpret_cast<const char*>(&byteOrder), sizeof(byteOrder));
        os.write(reinterpret_cast<const char*>(&magicNumber), sizeof(magicNumber));
        os.write(reinterpret_cast<const char*>(&firstIFDOffset), sizeof(firstIFDOffset));
        auto endPos = os.tellp();
        if (endPos - startPos != sizeof(HeaderBytes)) {
          throw std::runtime_error("HeaderBytes size mismatch");
        }
        return os;
      }
  };

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

  { // Test wrong byte order
    HeaderBytes headerBytes{ 0x4D49, 42, 8 };
    std::stringstream stream;
    headerBytes.write(stream);
    // This should throw an exception due to invalid byte order
    REQUIRE_THROWS_AS(TiffImage::Header::read(stream), std::runtime_error);
  }

  { // Test wrong magic number
    HeaderBytes headerBytes{ 0x4949, 43, 8 }; // Magic number should be 42
    std::stringstream stream;
    headerBytes.write(stream);
    // This should throw an exception due to invalid magic number
    REQUIRE_THROWS_AS(TiffImage::Header::read(stream), std::runtime_error);
  }

  { // Test wrong offset
    HeaderBytes headerBytes{ 0x4949, 43, 7 }; // Offset should be >= 8
    std::stringstream stream;
    headerBytes.write(stream);
    // This should throw an exception due to invalid first IFD offset
    REQUIRE_THROWS_AS(TiffImage::Header::read(stream), std::runtime_error);
  }

  { // Test invalid stream
    std::stringstream stream;
    // This should throw an exception due to invalid stream
    REQUIRE_THROWS_AS(TiffImage::Header::read(stream), std::runtime_error);
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

TEST_CASE("TiffImage IFD::Entry class", "[tiff_IDF_entry]") {
  // Define a helper to write a valid IFD entry
  struct EntryWriter
  {
    static std::ostream& write(
      std::ostream& os,
      uint16_t tag,
      Type type,
      uint32_t count,
      const std::byte* values,
      uint32_t valueOffset = 0,
      bool mustSwap = false)
      {
        const uint32_t valueBytes =
          count * typeBytes(type) + (type == Type::ASCII ? 1 : 0);

        if (count < 1) {
          throw std::runtime_error("Count must be at least 1");
        }
        if (values == nullptr) {
          throw std::runtime_error("Values pointer cannot be null");
        }
        if (valueOffset < 12 && valueOffset != 0) {
          throw std::runtime_error("Value offset must be zero or greater or equal to 12");
        }
        if (valueBytes > 4 && valueOffset == 0) {
          throw std::runtime_error("Value offset must be provided if values do not fit in 4 bytes");
        }
        if (valueBytes <= 4 && valueOffset > 0) {
          throw std::runtime_error("Value offset must be zero if values fit in 4 bytes");
        }
        if (valueOffset % 2 != 0) {
          throw std::runtime_error("Value offset must be even");
        }

        auto startPos = os.tellp();

        writeValue(os, tag, mustSwap);
        writeValue(os, type, mustSwap);
        writeValue(os, count, mustSwap);

        if (valueBytes <= 4) {
          // Values fit in 4 bytes, write them directly
          std::vector<std::byte> valueBytesVec(4, std::byte{0});
          std::copy(values, values + valueBytes, valueBytesVec.data());
          if (mustSwap) {
            // Note that we swap values as an array, instead of as a single `uint32_t`
            // This is because the values may not be a single `uint32_t` but rather
            // a sequence of smaller types (e.g., uint16_t, uint8_t)
            swapArray(valueBytesVec.data(), type, count);
          }
          os.write(reinterpret_cast<const char*>(valueBytesVec.data()), 4);
        }
        else {
          // Values do not fit in 4 bytes, write the offset
          writeValue(os, valueOffset, mustSwap);
          std::streampos offset = startPos + static_cast<std::streampos>(valueOffset);
          if (mustSwap) {
            std::vector<std::byte> swappedValues(valueBytes);
            swapArray(swappedValues.data(), type, count);
            writeAt(os, offset, swappedValues.data(), valueBytes);
          }
          else {
            writeAt(os, offset, values, valueBytes);
          }
        }
      }
  };

  // Test entry with single values
  {
    std::stringstream stream;
    uint16_t tag = 0x0100; // ImageWidth
    Type type = Type::SHORT;
    uint32_t count = 1;
    std::byte values[] = { std::byte{0x01}, std::byte{0x00} }; // 1 pixel wide
    uint32_t valueOffset = 0; // Fits in 4 bytes
    bool mustSwap = false;
    EntryWriter::write(stream, tag, type, count, values);

    TiffImage::IFD::Entry entry = TiffImage::IFD::Entry::read(stream);
    REQUIRE(entry.tag() == tag);
    REQUIRE(entry.type() == type);
    REQUIRE(entry.count() == count);
    REQUIRE(entry.valueBytes() == 2);
    REQUIRE(entry.bytes() == count * entry.valueBytes());
    REQUIRE(entry.values() != nullptr);

    const std::byte* valuePtr = entry.values();
    for (uint32_t i = 0; i < entry.bytes(); ++i) {
      REQUIRE(valuePtr[i] == values[i]);
    }
  }

}