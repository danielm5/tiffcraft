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
    struct Header {
      uint16_t byteOrder;       // 0x4949 for "II", 0x4D4D for "MM"
      uint16_t magicNumber;     // 42
      uint32_t firstIFDOffset;  // Offset of the first IFD

      bool isBigEndian() const {
        return byteOrder == 0x4D4D; // "MM"
      }

      bool isLittleEndian() const {
        return byteOrder == 0x4949; // "II"
      }

      bool equalsHostByteOrder() const {
        return (isHostLittleEndian() && isLittleEndian()) ||
               (isHostBigEndian() && isBigEndian());
      }

      static Header read(std::istream& stream) {
        Header header;

        // Read the TIFF header from the stream
        if (!stream) {
          throw std::runtime_error("Stream is not valid for reading TIFF header");
        }
        stream.read(reinterpret_cast<char*>(&header), sizeof(Header));
        if (!stream) {
          throw std::runtime_error("Failed to read TIFF header");
        }

        // Validate the byte order
        if (header.byteOrder != 0x4949 && header.byteOrder != 0x4D4D) {
          throw std::runtime_error("Invalid byte order in TIFF header");
        }

        // Convert to host byte order if necessary
        if (!header.equalsHostByteOrder()) {
          header.magicNumber = swap16(header.magicNumber);
          header.firstIFDOffset = swap32(header.firstIFDOffset);
        }

        // Validate the magic number
        if (header.magicNumber != 42) {
          throw std::runtime_error("Invalid magic number in TIFF header");
        }

        // Validate the first IFD offset
        if (header.firstIFDOffset < sizeof(Header)) {
          throw std::runtime_error("Invalid first IFD offset in TIFF header");
        }

        return header;
      }
    };
    static_assert(sizeof(Header) == 8, "Header size must be 8 bytes");

    static TiffImage read(const std::string& filename) {
      std::ifstream file(filename, std::ios::binary);
      if (!file) {
        throw std::runtime_error("Failed to open TIFF file: " + filename);
      }
      return read(file);
    }

    static TiffImage read(std::istream& stream) {
      // Implementation for reading a TIFF file and returning a TiffImage object
      // This is a placeholder; actual implementation will depend on the TIFF format
      TiffImage image;
      // Read the file and populate the image object
      return image;
    }



    private:
      Header header_;
  };

}

std::ostream& operator<<(std::ostream& os, const TiffCraft::TiffImage::Header& header) {
  os << "Byte Order: " << (header.byteOrder == 0x4949 ? "II" : "MM") << "\n"
     << "Magic Number: " << header.magicNumber << "\n"
     << "First IFD Offset: " << header.firstIFDOffset;
  return os;
}
