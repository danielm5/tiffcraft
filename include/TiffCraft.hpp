// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#pragma once

#include <cstdint>
#include <string>
#include <fstream>
#include <stdexcept>
#include <vector>

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

  // TIFF data types
  // ===============
  // 1 = BYTE 8-bit unsigned integer.
  // 2 = ASCII 8-bit byte that contains a 7-bit ASCII code; the last byte
  //     must be NUL (binary zero).
  // 3 = SHORT 16-bit (2-byte) unsigned integer.
  // 4 = LONG 32-bit (4-byte) unsigned integer.
  // 5 = RATIONAL Two LONGs: the first represents the numerator of a
  //     fraction; the second, the denominator.
  enum class Type : uint16_t {
    BYTE = 1,
    ASCII = 2,
    SHORT = 3,
    LONG = 4,
    RATIONAL = 5
  };

  template <Type type>
  constexpr uint32_t typeBytes() {
    switch (type) {
      case Type::BYTE:      return 1;
      case Type::ASCII:     return 1;
      case Type::SHORT:     return 2;
      case Type::LONG:      return 4;
      case Type::RATIONAL:  return 8;
      default:
        throw std::runtime_error("Unknown TIFF entry type");
    }
  }

  inline uint32_t typeBytes(Type type) {
    switch (type) {
      case Type::BYTE:      return typeBytes<Type::BYTE>();
      case Type::ASCII:     return typeBytes<Type::ASCII>();
      case Type::SHORT:     return typeBytes<Type::SHORT>();
      case Type::LONG:      return typeBytes<Type::LONG>();
      case Type::RATIONAL:  return typeBytes<Type::RATIONAL>();
      default:
        throw std::runtime_error("Unknown TIFF entry type");
    }
  }

  struct Rational {
    uint32_t numerator;
    uint32_t denominator;
  };
  static_assert(sizeof(Rational) == 8, "Rational must be 8 bytes");

  // Utility functions for byte order conversion
  #ifdef HAS_BYTESWAP
    inline uint16_t swap16(uint16_t val) { return std::byteswap(val); }
    inline uint32_t swap32(uint32_t val) { return std::byteswap(val); }
    inline uint64_t swap64(uint64_t val) { return std::byteswap(val); }
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
    inline uint64_t swap64(uint64_t val) {
        return ((val << 56) & 0xFF00000000000000ULL) |
               ((val << 40) & 0x00FF000000000000ULL) |
               ((val << 24) & 0x0000FF0000000000ULL) |
               ((val << 8)  & 0x000000FF00000000ULL) |
               ((val >> 8)  & 0x00000000FF000000ULL) |
               ((val >> 24) & 0x0000000000FF0000ULL) |
               ((val >> 40) & 0x000000000000FF00ULL) |
               ((val >> 56) & 0x00000000000000FFULL);
    }
  #endif // HAS_BYTESWAP

  template <typename T>
  inline T swap(T val) {
    if constexpr (std::is_same_v<T, uint16_t> || std::is_same_v<T, int16_t>) {
        return swap16(val);
    } else if constexpr (std::is_same_v<T, uint32_t> || std::is_same_v<T, int32_t>) {
        return swap32(val);
    } else if constexpr (std::is_same_v<T, uint64_t> || std::is_same_v<T, int64_t>) {
        return swap64(val);
    } else if constexpr (std::is_same_v<T, Type>) {
        return static_cast<Type>(swap16(static_cast<uint16_t>(val)));
    } else if constexpr (std::is_same_v<T, Rational>) {
        return { swap32(val.numerator), swap32(val.denominator) };
    } else {
        return val; // No swap needed for other types
    }
  }

  template <typename T>
  inline void swapArray(T* arr, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        arr[i] = swap(arr[i]);
    }
  }

  inline void swapArray(std::byte* arr, Type type, size_t count) {
    if (!arr || count == 0) { return; } // No work to do
    switch(type) {
      case Type::BYTE:
      case Type::ASCII:
        // No swap needed for BYTE or ASCII
        break;
      case Type::SHORT:
        swapArray(reinterpret_cast<uint16_t*>(arr), count); break;
      case Type::LONG:
        swapArray(reinterpret_cast<uint32_t*>(arr), count); break;
      case Type::RATIONAL:
        swapArray(reinterpret_cast<Rational*>(arr), count); break;
      default:
        throw std::runtime_error("Unknown TIFF entry type for swapping");
    }
  }

  // Detect if the system is little-endian or big-endian
  constexpr inline bool isHostLittleEndian() {
    return std::endian::native == std::endian::little;
  }
  constexpr inline bool isHostBigEndian() {
    return std::endian::native == std::endian::big;
  }

  // Helper functions for working with streams
  template <typename T>
  std::istream& readValue(std::istream& stream, T& value, bool mustSwap = false) {
    if (!stream) {
      throw std::runtime_error("Stream is not valid for reading");
    }
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (mustSwap) {
      value = swap(value);
    }
    return stream;
  }

  template <typename T>
  T readValue(std::istream& stream, bool mustSwap = false) {
    T value;
    readValue(stream, value, mustSwap);
    return value;
  }

  std::istream& readAt(
    std::istream& stream, std::streampos pos,
    std::byte* buffer, std::size_t size) {
    if (!stream) {
      throw std::runtime_error("Stream is not valid for reading");
    }
    struct SeekGuard {
      std::istream& stream;
      std::streampos oldPos;
      SeekGuard(std::istream& s, std::streampos p) : stream(s), oldPos(s.tellg()) {
        stream.seekg(p);
      }
      ~SeekGuard() {
        stream.seekg(oldPos);
      }
    } seekGuard(stream, pos);
    if (!stream) {
      throw std::runtime_error("Failed to seek to position in stream");
    }
    stream.read(reinterpret_cast<char*>(buffer), size);
    return stream;
  }

  template <typename T>
  std::ostream& writeValue(std::ostream& stream, T value, bool mustSwap = false) {
    if (!stream) {
      throw std::runtime_error("Stream is not valid for writing");
    }
    if (mustSwap) {
      value = swap(value);
    }
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return stream;
  }

  std::ostream& writeAt(
    std::ostream& stream, std::streampos pos,
    const std::byte* buffer, std::size_t size) {
    if (!stream) {
      throw std::runtime_error("Stream is not valid for writing");
    }
    struct SeekGuard {
      std::ostream& stream;
      std::streampos oldPos;
      SeekGuard(std::ostream& s, std::streampos p) : stream(s), oldPos(s.tellp()) {
        stream.seekp(p);
      }
      ~SeekGuard() {
        stream.seekp(oldPos);
      }
    } seekGuard(stream, pos);
    if (!stream) {
      throw std::runtime_error("Failed to seek to position in stream");
    }
    stream.write(reinterpret_cast<const char*>(buffer), size);
    return stream;
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
        Header header;

        // Byte order
        uint16_t byteOrder = readValue<uint16_t>(stream); // 0x4949 for "II", 0x4D4D for "MM"
        if (byteOrder != 0x4949 && byteOrder != 0x4D4D) {
          throw std::runtime_error("Invalid byte order in TIFF header");
        }
        header.byteOrder_ =
          (byteOrder == 0x4949) ? std::endian::little : std::endian::big;

        // Swap byte order if necessary
        const bool mustSwap = !header.equalsHostByteOrder();

        // Magic number
        uint16_t magicNumber = readValue<uint16_t>(stream, mustSwap);
        if (magicNumber != 42) {
          throw std::runtime_error("Invalid magic number in TIFF header");
        }

        // First IFD offset
        uint32_t firstIFDOffset = readValue<uint32_t>(stream, mustSwap);
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
    class IFD {
    public:

      // Entry class
      // ===========
      // Each IFD entry is a 12-byte structure that describes a field in the
      // image. The structure is as follows:
      // - Bytes 0-1: The Tag that identifies the field.
      // - Bytes 2-3: The field Type.
      // - Bytes 4-7: The number of values (Count) of the indicated Type.
      // - Bytes 8-11: The Value is expected to begin on a word boundary; the
      //   corresponding Value Offset will thus be an even number. This file
      //   offset may point anywhere in the file, even after the image data.
      //   The value of the field, or an offset to the value if it is too large
      //   to fit in 4 bytes.
      //
      // Sort Order
      // ----------
      // The entries in an IFD must be sorted in ascending order by Tag.
      // The Values to which directory entries point need not be in any
      // particular order in the file.
      //
      // Value/Offset
      // ------------
      // To save time and space the Value Offset contains the Value instead of
      // pointing to the Value if and only if the Value fits into 4 bytes. If
      // the Value is shorter than 4 bytes, it is left-justified within the
      // 4-byte Value Offset, i.e., stored in the lower-numbered bytes. Whether
      // the Value fits within 4 bytes is determined by the Type and Count of
      // the field.
      //
      // Count
      // -----
      // Count (called Length in previous versions of the specification) is the
      // number of values. Note that Count is not the total number of bytes.
      // For example, a single 16-bit word (SHORT) has a Count of 1; not 2.
      // The value of the Count part of an ASCII field entry includes the NUL.
      // If padding is necessary, the Count does not include the pad byte.
      //
      // Types
      // -----
      // The field types and their sizes are defined in the `Type` enum.
      class Entry {
      public:
        uint16_t tag() const { return tag_; }
        Type type() const { return type_; }
        uint32_t count() const { return count_; }
        std::byte* values() const { return values_.get(); }

        uint32_t valueBytes() const { return TiffCraft::typeBytes(type_); }
        uint32_t bytes() const { return count() * valueBytes(); }
        void swapValues() { swapArray(values_.get(), type_, count_); }

        static Entry read(std::istream& stream, bool mustSwap = false) {
          Entry entry;
          entry.tag_ = readValue<uint16_t>(stream, mustSwap);
          entry.type_ = static_cast<Type>(readValue<uint16_t>(stream, mustSwap));
          entry.count_ = readValue<uint32_t>(stream, mustSwap);

          // Read the value
          const uint32_t valueSize = entry.bytes();
          entry.values_.reset(new std::byte[valueSize]);
          if (valueSize <= sizeof(uint32_t)) {
            // Value fits in 4 bytes, read directly
            const uint32_t value = readValue<uint32_t>(stream);
            std::memcpy(entry.values_.get(), &value, valueSize);
          }
          else {
            // Value is too large, read as an offset
            uint32_t valueOffset = readValue<uint32_t>(stream, mustSwap);
            if (valueOffset < 8 || valueOffset % 2 != 0) {
              throw std::runtime_error("Invalid value offset in TIFF entry");
            }
            readAt(stream, valueOffset, entry.values_.get(), valueSize);
          }

          // Swap values after reading them to treat them as an array, instead
          // of a single `uint32_t`. This is because the values may not be a
          // single `uint32_t` but rather a sequence of smaller types (e.g.,
          // uint16_t, uint8_t).
          // This is necessary to ensure that the values are correctly
          // interpreted according to their type.
          if (mustSwap) {
            entry.swapValues();
          }

          if (entry.type_ == Type::ASCII) {
            // Ensure the last byte is NUL for ASCII type
            if (entry.values_[valueSize - 1] != std::byte{0}) {
              throw std::runtime_error("ASCII value must end with NUL byte");
            }
          }

          return entry;
        }

      private:
        uint16_t tag_;          // Tag identifying the field
        Type type_;             // Type of the field
        uint32_t count_;        // Number of values
        std::unique_ptr<std::byte[]> values_; // Pointer to the value data
      };

      //TODO: add vector of directory entries
      private:
        std::vector<Entry> entries_; // Vector of directory entries
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
