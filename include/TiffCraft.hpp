// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#pragma once

#include <cstdint>
#include <string>
#include <fstream>
#include <stdexcept>

#if __cpp_lib_byteswap >= 201806L
# include <bit> // C++20 byteswap
# define HAS_BYTESWAP
#endif

#if __cpp_lib_endian >= 201907L
# include <bit> // C++20 endian
# define HAS_ENDIAN
#else
namespace std {
  enum class endian
  {
  #if defined(_MSC_VER) && !defined(__clang__)
      little = 0,
      big    = 1,
      native = little
  #else
      little = __ORDER_LITTLE_ENDIAN__,
      big    = __ORDER_BIG_ENDIAN__,
      native = __BYTE_ORDER__
  #endif
  };
}
#endif


namespace TiffCraft {

  // Utility functions for byte order conversion
  #ifdef HAS_BYTESWAP
    inline uint16_t swap16(uint16_t val) { return std::byteswap(val); }
    inline uint32_t swap32(uint32_t val) { return std::byteswap(val); }
  #else
    inline uint16_t swap16(uint16_t val) {
        return (val << 8) | (val >> 8);
    }
    inline uint32_t swap32(uint32_t val) {
        return ((val << 24) & 0xFF000000) |
              ((val << 8)  & 0x00FF0000) |
              ((val >> 8)  & 0x0000FF00) |
              ((val >> 24) & 0x000000FF);
    }
  #endif // HAS_BYTESWAP

  // Detect if the system is little-endian or big-endian
  constexpr inline bool isHostLittleEndian() {
    return std::endian::native == std::endian::little;
  }
  constexpr inline bool isHostBigEndian() {
    return std::endian::native == std::endian::big;
  }

  class TiffImage {
  public:

    // Header class
    // ============
    // A TIFF file begins with an 8-byte image file header:
    // - Bytes 0-1: The byte order used within the file. Legal values are:
    //   - “II” (4949.H)
    //   - “MM” (4D4D.H)
    //   In the “II” format, byte order is always from the least significant
    //   byte to the most significant byte, for both 16-bit and 32-bit integers
    //   This is called little-endian byte order. In the “MM” format, byte order
    //   is always from most significant to least significant, for both 16-bit
    //   and 32-bit integers. This is called big-endian byte order.
    // - Bytes 2-3 An arbitrary but carefully chosen number (42) that further
    //   identifies the file as a TIFF file. The byte order depends on the value
    //   of Bytes 0-1.
    // - Bytes 4-7 The offset (in bytes) of the first IFD. The directory may be
    //   at any location in the file after the header but must begin on a word
    //   boundary.
    class Header {
      std::endian byteOrder_;
      uint32_t firstIFDOffset_;
    public:
      bool isBigEndian() const { return byteOrder_ == std::endian::big; }
      bool isLittleEndian() const { return byteOrder_ == std::endian::little; }

      bool equalsHostByteOrder() const {
        return (isHostLittleEndian() && isLittleEndian()) ||
               (isHostBigEndian() && isBigEndian());
      }

      uint32_t firstIFDOffset() const { return firstIFDOffset_; }

      static Header read(std::istream& stream) {
        if (!stream) {
          throw std::runtime_error("Stream is not valid for reading TIFF header");
        }

        Header header;

        // Byte order
        uint16_t byteOrder; // 0x4949 for "II", 0x4D4D for "MM"
        stream.read(reinterpret_cast<char*>(&byteOrder), sizeof(byteOrder));
        if (byteOrder != 0x4949 && byteOrder != 0x4D4D) {
          throw std::runtime_error("Invalid byte order in TIFF header");
        }
        header.byteOrder_ =
          (byteOrder == 0x4949) ? std::endian::little : std::endian::big;

        // Magic number
        uint16_t magicNumber;
        stream.read(reinterpret_cast<char*>(&magicNumber), sizeof(magicNumber));
        if (!header.equalsHostByteOrder()) {
          magicNumber = swap16(magicNumber);
        }
        if (magicNumber != 42) {
          throw std::runtime_error("Invalid magic number in TIFF header");
        }

        // First IFD offset
        uint32_t firstIFDOffset;
        stream.read(reinterpret_cast<char*>(&firstIFDOffset), sizeof(firstIFDOffset));
        if (!header.equalsHostByteOrder()) {
          firstIFDOffset = swap32(firstIFDOffset);
        }
        if (firstIFDOffset < 8) { // Minimum size of TIFF header
          throw std::runtime_error("Invalid first IFD offset in TIFF header");
        }
        header.firstIFDOffset_ = firstIFDOffset;

        return header;
      }
    };

    // IFD class
    // =========
    // An Image File Directory (IFD) consists of a 2-byte count of the
    // number of directory entries (i.e., the number of fields), followed
    // by a sequence of 12-byte field entries, followed by a 4-byte offset
    // of the next IFD (or 0 if none).
    struct IFD {

      //TODO: add vector of directory entries
    };



    static TiffImage read(const std::string& filename) {
      std::ifstream file(filename, std::ios::binary);
      if (!file) {
        throw std::runtime_error("Failed to open TIFF file: " + filename);
      }
      return read(file);
    }

    static TiffImage read(std::istream& stream) {
      TiffImage image;

      // Read and parse the header
      image.header_ = Header::read(stream);

      // TODO: Read IFDs and image data

      return image;
    }



    private:
      Header header_;
  };

}

std::ostream& operator<<(std::ostream& os, const TiffCraft::TiffImage::Header& header) {
  os << "Byte Order: " << (header.isLittleEndian() ? "Little Endian" : "Big Endian") << "\n"
     << "First IFD Offset: " << header.firstIFDOffset() << "\n"
     << "Equals Host Byte Order: " << (header.equalsHostByteOrder() ? "Yes" : "No") << "\n";
  return os;
}
