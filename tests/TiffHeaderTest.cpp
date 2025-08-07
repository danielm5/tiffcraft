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
      const uint32_t valueBytes = count * typeBytes(type);

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
        while (os.tellp() < offset) {
          os.put(0); // Pad with zeros if necessary
        }
        if (mustSwap) {
          std::vector<std::byte> swappedValues(values, values+valueBytes);
          swapArray(swappedValues.data(), type, count);
          writeAt(os, offset, swappedValues.data(), valueBytes);
        }
        else {
          writeAt(os, offset, values, valueBytes);
        }
      }
    }
  };

  auto testEntry = [](
      uint16_t tag, Type type, uint32_t count,
      const std::vector<uint8_t>& values,
      uint32_t valueOffset = 0, bool mustSwap = false) {

    std::stringstream stream;
    EntryWriter::write(
      stream, tag, type, count,
      reinterpret_cast<const std::byte*>(values.data()),
      valueOffset, mustSwap);

    TiffImage::IFD::Entry entry = TiffImage::IFD::Entry::read(stream, mustSwap);
    REQUIRE(entry.tag() == tag);
    REQUIRE(entry.type() == type);
    REQUIRE(entry.count() == count);
    REQUIRE(entry.valueBytes() == typeBytes(type));
    REQUIRE(entry.bytes() == count * entry.valueBytes());
    REQUIRE(entry.values() != nullptr);

    const std::byte* valuePtr = entry.values();
    for (uint32_t i = 0; i < entry.bytes(); ++i) {
      REQUIRE(valuePtr[i] == static_cast<std::byte>(values[i]));
    }
  };

  // Test with BYTE type
  testEntry(0x0101, Type::BYTE, 1, { 0x01 });
  testEntry(0x0101, Type::BYTE, 2, { 0x01, 0x02 });
  testEntry(0x0101, Type::BYTE, 3, { 0x01, 0x02, 0x03 });
  testEntry(0x0101, Type::BYTE, 4, { 0x01, 0x02, 0x03, 0x04 });
  testEntry(0x0101, Type::BYTE, 5, { 0x01, 0x02, 0x03, 0x04, 0x05 }, 12);
  testEntry(0x0101, Type::BYTE, 6, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 }, 20);

  // Test with SHORT type
  testEntry(0x0102, Type::SHORT, 1, { 0x01, 0x02 });
  testEntry(0x0102, Type::SHORT, 1, { 0x01, 0x02 }, 0, true); // Must swap
  testEntry(0x0102, Type::SHORT, 2, { 0x01, 0x02, 0x03, 0x04 });
  testEntry(0x0102, Type::SHORT, 2, { 0x01, 0x02, 0x03, 0x04 }, 0, true); // Must swap
  testEntry(0x0102, Type::SHORT, 3, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 }, 18);
  testEntry(0x0102, Type::SHORT, 4, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }, 20, true); // Must swap

  // Test with LONG type
  testEntry(0x0103, Type::LONG, 1, { 0x01, 0x02, 0x03, 0x04 });
  testEntry(0x0103, Type::LONG, 1, { 0x01, 0x02, 0x03, 0x04 }, 0, true); // Must swap
  testEntry(0x0103, Type::LONG, 2, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }, 22);
  testEntry(0x0103, Type::LONG, 3, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C }, 22, true); // Must swap

  // Test with RATIONAL type
  testEntry(0x0104, Type::RATIONAL, 1, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }, 12);
  testEntry(0x0104, Type::RATIONAL, 1, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }, 12, true); // Must swap
  testEntry(0x0104, Type::RATIONAL, 2, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                         0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 }, 24);

  // Test with ASCII type
  testEntry(0x0105, Type::ASCII, 1, { '\0' });
  testEntry(0x0105, Type::ASCII, 2, { 'A', '\0' });
  testEntry(0x0105, Type::ASCII, 3, { 'A', 'B', '\0' });
  testEntry(0x0105, Type::ASCII, 4, { 'A', 'B', 'C', '\0' });
  testEntry(0x0105, Type::ASCII, 5, { 'A', 'B', 'C', 'D', '\0' }, 12);
  testEntry(0x0105, Type::ASCII, 6, { 'A', 'B', 'C', 'D', 'E', '\0' }, 16);
}