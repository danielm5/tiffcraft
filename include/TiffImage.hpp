// BSD 2-Clause License
// Copyright (c) 2025 Daniel Moreno
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// ---------------------------------------------------------------------------
//
// TiffImage.hpp
// =============
// This file contains the TiffImage class and related functionality. This class
// holds TIFF image metadata and provides access to the image structure and
// pixel data. No functionality for manipulating or interpreting the image data
// is provided here.
//
// A program like the following could be used for loading a TIFF image with your
// custom processing:
//
//     #include <TiffImage.hpp>
//     #include <iostream>
//
//     int main() {
//       TiffCraft::load("image.tiff",
//         [](const TiffCraft::TiffImage::Header& header,
//            const TiffCraft::TiffImage::IFD& ifd,
//            TiffCraft::TiffImage::ImageData imageData) {
//           std::cout << "Loaded IFD with " << ifd.entries().size()
//                     << " entries." << std::endl;
//           std::cout << "Image data size: " << imageData.size() << std::endl;
//
//           // Add your own image processing code here
//
//         });
//
//       return 0;
//     }
//

#pragma once

#include "TiffTags.hpp"

#include <type_traits>
#include <functional>
#include <stdexcept>
#include <optional>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <map>

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

#if __cpp_lib_span >= 20202L
# include <span>
# define HAS_SPAN
#endif

namespace TiffCraft {

  // TiffCraft version
  constexpr int MAJOR_VERSION = 0;
  constexpr int MINOR_VERSION = 1;
  constexpr int PATCH_VERSION = 0;

  std::string version() {
    return std::to_string(MAJOR_VERSION)
      + "." + std::to_string(MINOR_VERSION)
      + "." + std::to_string(PATCH_VERSION);
  }

  template <typename T>
  struct RationalT {
    T numerator;
    T denominator;
  };

  template <typename>
  struct is_rational : std::false_type {};

  template <typename U>
  struct is_rational<RationalT<U>> : std::true_type {};

  template <typename T>
  constexpr bool is_rational_v = is_rational<T>::value;

  using Rational = RationalT<uint32_t>;
  static_assert(sizeof(Rational) == 8, "Rational must be 8 bytes");

  using SRational = RationalT<int32_t>;
  static_assert(sizeof(SRational) == 8, "SRational must be 8 bytes");

  // TIFF data types
  // ===============
  // 1 = BYTE 8-bit unsigned integer.
  // 2 = ASCII 8-bit byte that contains a 7-bit ASCII code; the last byte
  //     must be NUL (binary zero).
  // 3 = SHORT 16-bit (2-byte) unsigned integer.
  // 4 = LONG 32-bit (4-byte) unsigned integer.
  // 5 = RATIONAL Two LONGs: the first represents the numerator of a
  //     fraction; the second, the denominator.
  // 6 = SBYTE An 8-bit signed (twos-complement) integer.
  // 7 = UNDEFINED An 8-bit byte that may contain anything, depending on
  //     the definition of the field.
  // 8 = SSHORT A 16-bit (2-byte) signed (twos-complement) integer.
  // 9 = SLONG A 32-bit (4-byte) signed (twos-complement) integer.
  // 10 = SRATIONAL Two SLONG’s: the first represents the numerator of a
  //      fraction, the second the denominator.
  // 11 = FLOAT Single precision (4-byte) IEEE format.
  // 12 = DOUBLE Double precision (8-byte) IEEE format
  enum class Type : uint16_t {
    BYTE = 1,
    ASCII = 2,
    SHORT = 3,
    LONG = 4,
    RATIONAL = 5,
    SBYTE = 6,
    UNDEFINED = 7,
    SSHORT = 8,
    SLONG = 9,
    SRATIONAL = 10,
    FLOAT = 11,
    DOUBLE = 12
  };

  std::string_view toString(Type type) {
    switch (type) {
      case TiffCraft::Type::BYTE:      return "BYTE";
      case TiffCraft::Type::ASCII:     return "ASCII";
      case TiffCraft::Type::SHORT:     return "SHORT";
      case TiffCraft::Type::LONG:      return "LONG";
      case TiffCraft::Type::RATIONAL:  return "RATIONAL";
      case TiffCraft::Type::SBYTE:     return "SBYTE";
      case TiffCraft::Type::UNDEFINED: return "UNDEFINED";
      case TiffCraft::Type::SSHORT:    return "SSHORT";
      case TiffCraft::Type::SLONG:     return "SLONG";
      case TiffCraft::Type::SRATIONAL: return "SRATIONAL";
      case TiffCraft::Type::FLOAT:     return "FLOAT";
      case TiffCraft::Type::DOUBLE:    return "DOUBLE";
      default:                         return "!UNKNOWN";
    }
  }

  template <Type type> struct TypeTraits;
  template <> struct TypeTraits<Type::BYTE> { using type = uint8_t; };
  template <> struct TypeTraits<Type::ASCII> { using type = char; };
  template <> struct TypeTraits<Type::SHORT> { using type = uint16_t; };
  template <> struct TypeTraits<Type::LONG> { using type = uint32_t; };
  template <> struct TypeTraits<Type::RATIONAL> { using type = Rational; };
  template <> struct TypeTraits<Type::SBYTE> { using type = int8_t; };
  template <> struct TypeTraits<Type::UNDEFINED> { using type = std::byte; };
  template <> struct TypeTraits<Type::SSHORT> { using type = int16_t; };
  template <> struct TypeTraits<Type::SLONG> { using type = int32_t; };
  template <> struct TypeTraits<Type::SRATIONAL> { using type = SRational; };
  template <> struct TypeTraits<Type::FLOAT> { using type = float; };
  template <> struct TypeTraits<Type::DOUBLE> { using type = double; };

  template <Type type> using TypeTraits_t = typename TypeTraits<type>::type;

  template <typename F>
  decltype(auto) dispatchType(Type type, F&& f) {
    switch (type) {
      case Type::BYTE:      return f.template operator()<Type::BYTE>();
      case Type::ASCII:     return f.template operator()<Type::ASCII>();
      case Type::SHORT:     return f.template operator()<Type::SHORT>();
      case Type::LONG:      return f.template operator()<Type::LONG>();
      case Type::RATIONAL:  return f.template operator()<Type::RATIONAL>();
      case Type::SBYTE:     return f.template operator()<Type::SBYTE>();
      case Type::UNDEFINED: return f.template operator()<Type::UNDEFINED>();
      case Type::SSHORT:    return f.template operator()<Type::SSHORT>();
      case Type::SLONG:     return f.template operator()<Type::SLONG>();
      case Type::SRATIONAL: return f.template operator()<Type::SRATIONAL>();
      case Type::FLOAT:     return f.template operator()<Type::FLOAT>();
      case Type::DOUBLE:    return f.template operator()<Type::DOUBLE>();
      default:
        throw std::runtime_error("Unknown TIFF entry type");
    }
  }

  inline uint32_t typeBytes(Type type) {
    return dispatchType(type, []<Type type>() -> uint32_t {
      return sizeof(TypeTraits_t<type>);
    });
  }

  template <typename SrcType, typename DstType>
  void copyVector(const std::byte* src, size_t count, std::vector<DstType>& dest) {
    if constexpr (std::is_convertible_v<SrcType, DstType>) {
      if (sizeof(DstType) < sizeof(SrcType)) {
        throw std::runtime_error("Destination type size is smaller than source type size");
      }
      dest.resize(count);
      const SrcType* srcPtr = reinterpret_cast<const SrcType*>(src);
      for (size_t i = 0; i < count; ++i) {
        dest[i] = static_cast<DstType>(srcPtr[i]);
      }
    } else {
      throw std::runtime_error("Source type is not convertible to destination type");
    }
  }

  template <typename DstType>
  void copyVector(Type type, const std::byte* src, size_t count, std::vector<DstType>& dest) {
    dispatchType(type, [&]<Type type>() {
      copyVector<TypeTraits_t<type>, DstType>(src, count, dest);
    });
  }

  // Utility functions for byte order conversion
  #ifdef HAS_BYTESWAP
    inline uint16_t swap16(uint16_t val) { return std::byteswap(val); }
    inline uint32_t swap32(uint32_t val) { return std::byteswap(val); }
    inline uint64_t swap64(uint64_t val) { return std::byteswap(val); }
    inline int16_t swap16(int16_t val) { return std::byteswap(val); }
    inline int32_t swap32(int32_t val) { return std::byteswap(val); }
    inline int64_t swap64(int64_t val) { return std::byteswap(val); }
  #else
    inline uint16_t swap16(uint16_t val) {
      return (val << 8) | (val >> 8);
    }
    inline uint32_t swap32(uint32_t val) {
      return  ((val << 24) & 0xFF000000) |
              ((val << 8)  & 0x00FF0000) |
              ((val >> 8)  & 0x0000FF00) |
              ((val >> 24) & 0x000000FF);
    }
    inline uint64_t swap64(uint64_t val) {
      return  ((val << 56) & 0xFF00000000000000ULL) |
              ((val << 40) & 0x00FF000000000000ULL) |
              ((val << 24) & 0x0000FF0000000000ULL) |
              ((val << 8)  & 0x000000FF00000000ULL) |
              ((val >> 8)  & 0x00000000FF000000ULL) |
              ((val >> 24) & 0x0000000000FF0000ULL) |
              ((val >> 40) & 0x000000000000FF00ULL) |
              ((val >> 56) & 0x00000000000000FFULL);
    }
    inline int16_t swap16(int16_t val) {
      return static_cast<int16_t>(swap16(static_cast<uint16_t>(val)));
    }
    inline int32_t swap32(int32_t val) {
      return static_cast<int32_t>(swap32(static_cast<uint32_t>(val)));
    }
    inline int64_t swap64(int64_t val) {
      return static_cast<int64_t>(swap64(static_cast<uint64_t>(val)));
    }
  #endif // HAS_BYTESWAP
  inline Type swap16(Type val) {
    return static_cast<Type>(swap16(static_cast<uint16_t>(val)));
  }
  inline float swap32(float val) {
    uint32_t u32;
    std::memcpy(&u32, &val, sizeof(u32));
    u32 = swap32(u32);
    std::memcpy(&val, &u32, sizeof(val));
    return val;
  }
  inline double swap64(double val) {
    uint64_t u64;
    std::memcpy(&u64, &val, sizeof(u64));
    u64 = swap64(u64);
    std::memcpy(&val, &u64, sizeof(val));
    return val;
  }

  template <typename T>
  inline T swap(T val) {
    if constexpr (std::is_same_v<T, uint16_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, Type>) {
        return swap16(val);
    } else if constexpr (std::is_same_v<T, uint32_t> || std::is_same_v<T, int32_t> || std::is_same_v<T, float>) {
        return swap32(val);
    } else if constexpr (std::is_same_v<T, uint64_t> || std::is_same_v<T, int64_t> || std::is_same_v<T, double>) {
        return swap64(val);
    } else if constexpr (std::is_same_v<T, Rational> || std::is_same_v<T, SRational>) {
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
    dispatchType(type, [&]<Type type>() {
      swapArray(reinterpret_cast<TypeTraits_t<type>*>(arr), count);
    });
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
        // try to seek to the specified position
        stream.seekp(p);
        if (stream.fail()) {
          // if seeking fails, we check if we tried to seek beyond the end of
          // the stream and if so, we pad the stream with zeros until we reach
          // the desired position
          stream.clear(); // Clear any error flags
          stream.seekp(0, std::ios_base::end);
          while (stream && stream.tellp() < p) {
            stream.put(0); // Pad with zeros if necessary
          }
          assert(stream.tellp() == p && "SeekGuard did not reach the desired position");
        }
      }
      ~SeekGuard() {
        stream.seekp(oldPos);
      }
    } seekGuard(stream, pos);
    if (!stream || stream.tellp() != pos) {
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
      std::endian byteOrder() const { return byteOrder_; }

      bool equalsHostByteOrder() const {
        return std::endian::native == byteOrder_;
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
        Tag tag() const { return tag_; }
        Type type() const { return type_; }
        uint32_t count() const { return count_; }
        const std::byte* values() const { return values_.data(); }

#ifdef HAS_SPAN
        template <typename T>
        std::span<const T> values() const {
          if (sizeof(T) != TiffCraft::typeBytes(type_)) {
            throw std::runtime_error("Invalid type size for values span");
          }
          return std::span<const T>(reinterpret_cast<const T*>(values_.data()), count());
        }
#endif // HAS_SPAN

        uint32_t bytes() const { return count() * TiffCraft::typeBytes(type_); }

        static Entry read(std::istream& stream, bool mustSwap = false) {
          Entry entry;
          entry.tag_ = static_cast<Tag>(readValue<uint16_t>(stream, mustSwap));
          entry.type_ = static_cast<Type>(readValue<uint16_t>(stream, mustSwap));
          entry.count_ = readValue<uint32_t>(stream, mustSwap);

          // Read the value
          const uint32_t valueSize = entry.bytes();
          entry.values_.resize(valueSize);
          if (valueSize <= sizeof(uint32_t)) {
            // Value fits in 4 bytes, read directly
            const uint32_t value = readValue<uint32_t>(stream);
            std::memcpy(entry.values_.data(), &value, valueSize);
          }
          else {
            // Value is too large, read as an offset
            uint32_t valueOffset = readValue<uint32_t>(stream, mustSwap);
            if (valueOffset < 8 || valueOffset % 2 != 0) {
              throw std::runtime_error("Invalid value offset in TIFF entry");
            }
            readAt(stream, valueOffset, entry.values_.data(), valueSize);
          }

          // Swap values after reading them to treat them as an array, instead
          // of a single `uint32_t`. This is because the values may not be a
          // single `uint32_t` but rather a sequence of smaller types (e.g.,
          // uint16_t, uint8_t).
          // This is necessary to ensure that the values are correctly
          // interpreted according to their type.
          if (mustSwap) {
            swapArray(entry.values_.data(), entry.type_, entry.count_);
          }

          if (entry.type_ == Type::ASCII) {
            // Ensure the last byte is NUL for ASCII type
            if (entry.values_[valueSize - 1] != std::byte{0}) {
              throw std::runtime_error("ASCII value must end with NUL byte");
            }
          }

          return entry;
        }

        bool operator==(const Entry& other) const {
          return tag_ == other.tag_ &&
                 type_ == other.type_ &&
                 count_ == other.count_ &&
                 values_ == other.values_;
        }

      private:
        Tag tag_;                       // Tag identifying the field
        Type type_;                     // Type of the field
        uint32_t count_;                // Number of values
        std::vector<std::byte> values_; // Pointer to the value data
      };

      const std::map<Tag, Entry>& entries() const { return entries_; }

      const Entry& getEntry(Tag tag,
        std::string errorMessage = "Entry not found") const {
        auto it = entries_.find(tag);
        if (it == entries_.end()) {
          throw std::runtime_error(errorMessage);
        }
        return it->second;
      }

      static IFD read(std::istream& stream, bool mustSwap = false) {
        IFD ifd;

        // Read the number of entries
        uint16_t entryCount = readValue<uint16_t>(stream, mustSwap);

        // Read each entry
        Tag lastTag = Tag::Null;
        for (uint16_t i = 0; i < entryCount; ++i) {
          Entry entry = Entry::read(stream, mustSwap);
          ifd.entries_[entry.tag()] = std::move(entry);
          if (entry.tag() <= lastTag) {
            throw std::runtime_error("Entries must be sorted by tag in ascending order");
          }
          lastTag = entry.tag();
        }

        return ifd;
      }

      private:
        std::map<Tag, Entry> entries_; // Map of directory entries
    };

    using ImageData = std::vector<std::vector<std::byte>>;

    const Header& header() const { return header_; }
    const std::vector<IFD>& ifds() const { return ifds_; }

    std::istream& stream() const {
      if (!stream_) {
        throw std::runtime_error("Stream is not set for this TIFF image");
      }
      return *stream_;
    }

    static TiffImage read(const std::string& filename) {
      std::ifstream file(filename, std::ios::binary);
      if (!file) {
        throw std::runtime_error("Failed to open TIFF file: " + filename);
      }
      auto image = read(file);
      image.stream_ = std::make_unique<std::ifstream>(std::move(file));
      return image;
    }

    static TiffImage read(std::istream& stream) {
      TiffImage image;

      // Read and parse the header
      image.header_ = Header::read(stream);

      // Read the IFDs
      const bool mustSwap = !image.header_.equalsHostByteOrder();
      uint32_t offset = image.header_.firstIFDOffset();
      while (offset > 0) {
        // Seek to the IFD offset
        stream.seekg(offset);
        if (stream.fail()) {
          throw std::runtime_error("Failed to seek to IFD offset");
        }

        // Read the IFD
        TiffImage::IFD ifd = TiffImage::IFD::read(stream, mustSwap);
        image.ifds_.push_back(std::move(ifd));

        // Read the next IFD offset
        offset = readValue<uint32_t>(stream, mustSwap);
      }

      return image;
    }

    static ImageData readImageStrips(std::istream& stream, const IFD& ifd) {
      const auto& entries = ifd.entries();
      std::vector<uint32_t> stripOffsets, stripByteCounts;
      { //copy offsets
        const auto& entry = ifd.getEntry(Tag::StripOffsets, "StripOffsets entry not found in IFD");
        copyVector<uint32_t>(entry.type(), entry.values(), entry.count(), stripOffsets);
      }
      { //copy byte counts
        const auto& entry = ifd.getEntry(Tag::StripByteCounts, "StripByteCounts entry not found in IFD");
        copyVector<uint32_t>(entry.type(), entry.values(), entry.count(), stripByteCounts);
      }
      if (stripOffsets.size() != stripByteCounts.size()) {
        throw std::runtime_error("Mismatch between number of StripOffsets and StripByteCounts");
      }
      ImageData imageData;
      for (size_t i = 0; i < stripOffsets.size(); ++i) {
        const uint32_t offset = stripOffsets[i];
        const uint32_t byteCount = stripByteCounts[i];
        if (offset < 8 || byteCount == 0) {
          throw std::runtime_error("Invalid strip offset or byte count");
        }
        imageData.emplace_back(byteCount);
        readAt(stream, offset, imageData.back().data(), byteCount);
      }
      return imageData;
    }

    static ImageData readImageTiles(std::istream& stream, const IFD& ifd) {
      const auto& entries = ifd.entries();
      std::vector<uint32_t> tileOffsets, tileByteCounts;
      { //copy offsets
        const auto& entry = ifd.getEntry(Tag::TileOffsets, "TileOffsets entry not found in IFD");
        copyVector<uint32_t>(entry.type(), entry.values(), entry.count(), tileOffsets);
      }
      { //copy byte counts
        const auto& entry = ifd.getEntry(Tag::TileByteCounts, "TileByteCounts entry not found in IFD");
        copyVector<uint32_t>(entry.type(), entry.values(), entry.count(), tileByteCounts);
      }
      if (tileOffsets.size() != tileByteCounts.size()) {
        throw std::runtime_error("Mismatch between number of TileOffsets and TileByteCounts");
      }
      ImageData imageData;
      for (size_t i = 0; i < tileOffsets.size(); ++i) {
        const uint32_t offset = tileOffsets[i];
        const uint32_t byteCount = tileByteCounts[i];
        if (offset < 8 || byteCount == 0) {
          throw std::runtime_error("Invalid tile offset or byte count");
        }
        imageData.emplace_back(byteCount);
        readAt(stream, offset, imageData.back().data(), byteCount);
      }
      return imageData;
    }

    private:
      Header header_;
      std::vector<IFD> ifds_; // List of IFDs in the TIFF image
      std::unique_ptr<std::istream> stream_;
  };

  struct LoadParams {
    std::optional<uint16_t> ifdIndex; // Optional IFD index to load
  };

  using LoadCallback = std::function<void(
    const TiffImage::Header&, const TiffImage::IFD&, TiffImage::ImageData)>;

  void load(std::istream& stream, LoadCallback&& callback, const LoadParams& params = {}) {
    // Read the TIFF image from the stream
    TiffImage image = TiffImage::read(stream);
    const auto& header = image.header();

    if (params.ifdIndex && params.ifdIndex.value() >= image.ifds().size()) {
      throw std::runtime_error("Requested IFD index is out of bounds");
    }

    for (size_t i = 0; i < image.ifds().size(); ++i) {
      if (!params.ifdIndex || params.ifdIndex.value() == i) {
        // If a specific IFD index is requested, only process that IFD
        // Otherwise, process all IFDs
        const auto& ifd = image.ifds()[i];
        if (ifd.entries().contains(Tag::StripOffsets)) {
          // image contains strip offsets
          callback(header, ifd, TiffImage::readImageStrips(stream, ifd));
        } else if (ifd.entries().contains(Tag::TileByteCounts)) {
          // image contains tile byte counts
          callback(header, ifd, TiffImage::readImageTiles(stream, ifd));
        } else {
          throw std::runtime_error("Unsupported IFD format");
        }
      }
    }
  }

  void load(const std::string& filename, LoadCallback&& callback, const LoadParams& params = {}) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
      throw std::runtime_error("Failed to open TIFF file: " + std::string(filename));
    }
    load(file, std::move(callback), params);
  }

} // namespace TiffCraft
