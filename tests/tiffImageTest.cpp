// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#include <TiffCraft.hpp>
#include <TiffStdOut.hpp>

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <initializer_list>

using namespace TiffCraft;

constexpr bool isHostLittleEndian() { return (std::endian::native == std::endian::little); }
constexpr bool isHostBigEndian() { return (std::endian::native == std::endian::big); }

namespace TiffCraft {
  template <typename T>
  bool operator==(const RationalT<T>& lhs, const RationalT<T>& rhs) {
    return lhs.numerator == rhs.numerator && lhs.denominator == rhs.denominator;
  }
}

#ifdef HAS_SPAN
  template <typename T>
  bool operator==(const std::span<const T>& lhs, const std::span<const T>& rhs) {
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
  }

  template <typename T>
  std::span<const T> make_span(std::initializer_list<T> values) {
    return std::span<const T>(values.begin(), values.size());
  }
#endif // HAS_SPAN

std::string getTestFilePath(const std::string& filename) {
  std::filesystem::path test_file_path(__FILE__);
  std::filesystem::path test_dir = test_file_path.parent_path();
  return (test_dir / "libtiff-pics" / filename).string();
}

std::ostream& write_IFD_entry(
  std::ostream& os,
  Tag tag,
  Type type,
  uint32_t count,
  const std::byte* values,
  uint32_t valueOffset = 0,
  bool mustSwap = false)
{
  const uint32_t valueBytes = count * typeBytes(type);

  // Current position in the stream
  const uint32_t startPos = static_cast<uint32_t>(os.tellp());

  if (count < 1) {
    throw std::runtime_error("Count must be at least 1");
  }
  if (values == nullptr) {
    throw std::runtime_error("Values pointer cannot be null");
  }
  if (valueOffset - startPos < 12 && valueOffset != 0) {
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
    std::streampos offset = static_cast<std::streampos>(valueOffset);
    if (mustSwap) {
      std::vector<std::byte> swappedValues(values, values+valueBytes);
      swapArray(swappedValues.data(), type, count);
      writeAt(os, offset, swappedValues.data(), valueBytes);
    }
    else {
      writeAt(os, offset, values, valueBytes);
    }
  }

  return os;
}

std::ostream& write_IFD(
  std::ostream& os,
  const std::vector<TiffImage::IFD::Entry>& entries,
  bool mustSwap = false)
{
  // We write the IFD first, and the entry data afterwards. Therefore, we need
  // calculate the offsets of the entries first.
  const uint32_t IFD_bytes = 2 + 12 * entries.size(); // 2 bytes for the count,
                                                      // and 12 bytes per entry

  // Current position in the stream
  std::streampos currentPos = os.tellp();

  uint32_t nextOffset = IFD_bytes + static_cast<uint32_t>(currentPos);
  std::vector<uint32_t> offsets;
  offsets.reserve(entries.size());
  for (const auto& entry : entries) {
    if (entry.bytes() > 4) {
      // If the entry value does not fit in 4 bytes, we need to write it at a later offset
      offsets.emplace_back(nextOffset);
      nextOffset += entry.bytes();
    }
    else {
      // If the entry value fits in 4 bytes, we can write it directly
      offsets.emplace_back(0);
    }
  }

  // Write the number of entries
  writeValue(os, static_cast<uint16_t>(entries.size()), mustSwap);

  // Write the entries
  for (const auto& entry : entries) {
    write_IFD_entry(os, entry.tag(), entry.type(), entry.count(),
      entry.values(), offsets[&entry - &entries[0]], mustSwap);
  }

  return os;
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
        if (std::endian::native == std::endian::little && bo == 0x4D4D // "MM" big-endian
            || std::endian::native == std::endian::big && bo == 0x4949) { // "II" little-endian
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
    REQUIRE(header.byteOrder() == std::endian::little);
    REQUIRE(header.equalsHostByteOrder() == isHostLittleEndian());
  }

  { // Test big-endian header
    HeaderBytes headerBytes{ 0x4D4D, 42, 8 };
    std::stringstream stream;
    headerBytes.write(stream);

    TiffImage::Header header = TiffImage::Header::read(stream);
    REQUIRE(header.byteOrder() == std::endian::big);
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
    std::cout << header << std::endl;
  }
}

TEST_CASE("TiffImage IFD::Entry class", "[tiff_IDF_entry]") {

  auto testEntry = [](
      uint16_t tagInt, Type type, uint32_t count,
      const std::vector<uint8_t>& values,
      uint32_t valueOffset = 0, bool mustSwap = false) {

    std::stringstream stream;
    write_IFD_entry(
      stream, static_cast<Tag>(tagInt), type, count,
      reinterpret_cast<const std::byte*>(values.data()),
      valueOffset, mustSwap);

    TiffImage::IFD::Entry entry = TiffImage::IFD::Entry::read(stream, mustSwap);

    const Tag tag = static_cast<Tag>(mustSwap ? swap16(tagInt) : tagInt);

    REQUIRE(entry.tag() == tag);
    REQUIRE(entry.type() == type);
    REQUIRE(entry.count() == count);
    REQUIRE(entry.bytes() == count * TiffCraft::typeBytes(type));
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

TEST_CASE("TiffImage IFD class", "[tiff_IFD]") {

  // Create a few entries
  std::vector<TiffImage::IFD::Entry> entries;
  {
    auto addEntry = [&entries](
      uint16_t tag, Type type, uint32_t count,
      const std::vector<uint8_t>& values,
      uint32_t valueOffset = 0, bool mustSwap = false) {
        std::stringstream stream;
        // Write the entry
        write_IFD_entry(
          stream, static_cast<Tag>(tag), type, count,
          reinterpret_cast<const std::byte*>(values.data()),
          valueOffset, mustSwap);
        // Read the entry back
        TiffImage::IFD::Entry entry = TiffImage::IFD::Entry::read(stream, mustSwap);
        // Add the entry to the vector
        entries.push_back(entry);
    };
    addEntry(0x0101, Type::BYTE, 6, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 }, 12);
    addEntry(0x0102, Type::SHORT, 1, { 0x01, 0x02 });
    addEntry(0x0103, Type::LONG, 2, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }, 12);
    addEntry(0x0104, Type::RATIONAL, 1, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }, 12);
    addEntry(0x0105, Type::ASCII, 6, { 'A', 'B', 'C', 'D', 'E', '\0' }, 12);
  }
  REQUIRE(entries.size() == 5);

  { // Test writing and reading IFD
    std::stringstream stream;
    write_IFD(stream, entries, false);

    // Read the IFD back
    TiffImage::IFD ifd = TiffImage::IFD::read(stream, false);

    // Check the number of entries
    REQUIRE(ifd.entries().size() == entries.size());

    // Check each entry
    auto it = ifd.entries().begin();
    for (size_t i = 0; i < entries.size(); ++i, ++it) {
      REQUIRE(it->second == entries[i]);
    }
  }
}

TEST_CASE("TiffImage class", "[tiff_image]") {

  { // jim___ah.tif
    // Dimensions: 664x813
    // Resolution: 300 DPI
    // Bit depth: 1
    // Compression: Uncompressed
    // Resolution unit: 2
    const std::string test_file = "jim___ah.tif";
    std::cout << "Test file path: " << getTestFilePath(test_file) << std::endl;
    TiffImage image = TiffImage::read(getTestFilePath(test_file));
    std::cout << image << std::endl;

    const auto& ifds = image.ifds();
    REQUIRE(ifds.size() == 1);

    const auto& entries = ifds[0].entries();
    REQUIRE(entries.size() == 14);

  #ifdef HAS_SPAN
    #define value_span(type, tag) entries.at(tag).values<type>()

    REQUIRE(value_span(uint16_t, Tag::ImageWidth) == make_span<uint16_t>({ 664 }));
    REQUIRE(value_span(uint16_t, Tag::ImageLength) == make_span<uint16_t>({ 813 }));
    REQUIRE(value_span(uint16_t, Tag::Compression) == make_span<uint16_t>({ 1 }));
    REQUIRE(value_span(uint16_t, Tag::PhotometricInterpretation) == make_span<uint16_t>({ 0 }));
    REQUIRE(value_span(Rational, Tag::XResolution) == make_span({ Rational{300, 1} }));
    REQUIRE(value_span(Rational, Tag::YResolution) == make_span({ Rational{300, 1} }));

    #undef value_span
  #endif

    auto imageData = image.readImageStrips(image.stream(), ifds[0]);
    constexpr size_t expectedImageDataSize = 664 * 813 / 8; // 1 bit per pixel
    size_t actualImageDataSize = 0;
    for (const auto& data : imageData) {
      actualImageDataSize += data.size();
    }
    REQUIRE(actualImageDataSize == expectedImageDataSize);
  }

  { // jim___ah.tif: using load function
    const std::string test_file = "jim___ah.tif";
    std::cout << "Test file path: " << getTestFilePath(test_file) << std::endl;
    load(getTestFilePath(test_file),
      [](const TiffImage::Header& header, const TiffImage::IFD& ifd, TiffImage::ImageData imageData) {
        std::cout << "Loaded IFD with " << ifd.entries().size() << " entries." << std::endl;
        std::cout << "Image data size: " << imageData.size() << std::endl;

        const auto& entries = ifd.entries();

      #ifdef HAS_SPAN
        #define value_span(type, tag) entries.at(tag).values<type>()

        REQUIRE(value_span(uint16_t, Tag::ImageWidth) == make_span<uint16_t>({ 664 }));
        REQUIRE(value_span(uint16_t, Tag::ImageLength) == make_span<uint16_t>({ 813 }));
        REQUIRE(value_span(uint16_t, Tag::Compression) == make_span<uint16_t>({ 1 }));
        REQUIRE(value_span(uint16_t, Tag::PhotometricInterpretation) == make_span<uint16_t>({ 0 }));
        REQUIRE(value_span(Rational, Tag::XResolution) == make_span({ Rational{300, 1} }));
        REQUIRE(value_span(Rational, Tag::YResolution) == make_span({ Rational{300, 1} }));

        #undef value_span
      #endif

        constexpr size_t expectedImageDataSize = 664 * 813 / 8; // 1 bit per pixel
        size_t actualImageDataSize = 0;
        for (const auto& data : imageData) { actualImageDataSize += data.size(); }
        REQUIRE(actualImageDataSize == expectedImageDataSize);
      }
    );
  }

  { // cramps-tile.tif: using load function
    const std::string test_file = "cramps-tile.tif";
    std::cout << "Test file path: " << getTestFilePath(test_file) << std::endl;
    load(getTestFilePath(test_file),
      [](const TiffImage::Header& header, const TiffImage::IFD& ifd, TiffImage::ImageData imageData) {
        std::cout << "Loaded IFD with " << ifd.entries().size() << " entries." << std::endl;
        std::cout << "Image data size: " << imageData.size() << std::endl;

        const auto& entries = ifd.entries();

      #ifdef HAS_SPAN
        #define value_span(type, tag) entries.at(tag).values<type>()

        REQUIRE(value_span(uint16_t, Tag::ImageWidth) == make_span<uint16_t>({ 800 }));
        REQUIRE(value_span(uint16_t, Tag::ImageLength) == make_span<uint16_t>({ 607 }));
        REQUIRE(value_span(uint16_t, Tag::Compression) == make_span<uint16_t>({ 1 }));
        REQUIRE(value_span(uint16_t, Tag::PhotometricInterpretation) == make_span<uint16_t>({ 0 }));
        REQUIRE(value_span(uint16_t, Tag::TileWidth) == make_span<uint16_t>({ 256 }));
        REQUIRE(value_span(uint16_t, Tag::TileLength) == make_span<uint16_t>({ 256 }));
        REQUIRE(value_span(uint16_t, Tag::SamplesPerPixel) == make_span<uint16_t>({ 1 }));
        REQUIRE(value_span(uint16_t, Tag::BitsPerSample) == make_span<uint16_t>({ 8 }));

        #undef value_span
      #endif

        constexpr size_t tilesAcross = (800 + (256 - 1)) / 256;
        constexpr size_t tilesDown = (607 + (256 - 1)) / 256;
        constexpr size_t tilesInImage = tilesAcross * tilesDown;
        REQUIRE(imageData.size() == tilesInImage);

        constexpr size_t expectedTileSize = 256 * 256;
        for (const auto& tile : imageData) {
          REQUIRE(tile.size() == expectedTileSize);
        }
      }
    );
  }
}
