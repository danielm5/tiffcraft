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
// TiffStdOut.hpp
// ==============
// This file contains the standard output functionality for TIFF images. This
// is intended to provide easy output of TIFF image metadata and structure
// information to standard output for debugging and exploration.

#pragma once

#include "TiffImage.hpp"

#include <string>
#include <iostream>
#include <iomanip>

namespace TiffCraft {
  inline std::string toString(Tag tag)
  {
    switch (tag) {
      case TiffCraft::Tag::Null: return "Null";
      case TiffCraft::Tag::NewSubfileType: return "NewSubfileType";
      case TiffCraft::Tag::SubfileType: return "SubfileType";
      case TiffCraft::Tag::ImageWidth: return "ImageWidth";
      case TiffCraft::Tag::ImageLength: return "ImageLength";
      case TiffCraft::Tag::BitsPerSample: return "BitsPerSample";
      case TiffCraft::Tag::Compression: return "Compression";
      case TiffCraft::Tag::PhotometricInterpretation: return "PhotometricInterpretation";
      case TiffCraft::Tag::Thresholding: return "Thresholding";
      case TiffCraft::Tag::FillOrder: return "FillOrder";
      case TiffCraft::Tag::DocumentName: return "DocumentName";
      case TiffCraft::Tag::ImageDescription: return "ImageDescription";
      case TiffCraft::Tag::Make: return "Make";
      case TiffCraft::Tag::Model: return "Model";
      case TiffCraft::Tag::StripOffsets: return "StripOffsets";
      case TiffCraft::Tag::Orientation: return "Orientation";
      case TiffCraft::Tag::SamplesPerPixel: return "SamplesPerPixel";
      case TiffCraft::Tag::RowsPerStrip: return "RowsPerStrip";
      case TiffCraft::Tag::StripByteCounts: return "StripByteCounts";
      case TiffCraft::Tag::MinSampleValue: return "MinSampleValue";
      case TiffCraft::Tag::MaxSampleValue: return "MaxSampleValue";
      case TiffCraft::Tag::XResolution: return "XResolution";
      case TiffCraft::Tag::YResolution: return "YResolution";
      case TiffCraft::Tag::PlanarConfiguration: return "PlanarConfiguration";
      case TiffCraft::Tag::ResolutionUnit: return "ResolutionUnit";
      case TiffCraft::Tag::PageName: return "PageName";
      case TiffCraft::Tag::Software: return "Software";
      case TiffCraft::Tag::DateTime: return "DateTime";
      case TiffCraft::Tag::Artist: return "Artist";
      case TiffCraft::Tag::ColorMap: return "ColorMap";
      case TiffCraft::Tag::HalftoneHints: return "HalftoneHints";
      case TiffCraft::Tag::TileWidth: return "TileWidth";
      case TiffCraft::Tag::TileLength: return "TileLength";
      case TiffCraft::Tag::TileOffsets: return "TileOffsets";
      case TiffCraft::Tag::TileByteCounts: return "TileByteCounts";
      case TiffCraft::Tag::SampleFormat: return "SampleFormat";
      default: /* unknown tag */ break;
    }
    char buf[8] = "0x0000";
    std::snprintf(buf, sizeof(buf), "0x%04X", static_cast<uint16_t>(tag));
    return buf;
  }

  inline std::string_view toString(Type type) {
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
}

std::ostream& operator<<(std::ostream& os, TiffCraft::Tag tag) {
  return os << TiffCraft::toString(tag);
}

std::ostream& operator<<(std::ostream& os, TiffCraft::Type type) {
  return os << TiffCraft::toString(type);
}

std::ostream& operator<<(std::ostream& os, const TiffCraft::TiffImage::Header& header) {
  os << "TIFF Header:\n"
     << " - Byte Order: " << (header.byteOrder() == std::endian::little ? "Little Endian" : "Big Endian") << "\n"
     << " - First IFD Offset: " << header.firstIFDOffset() << "\n"
     << " - Equals Host Byte Order: " << (header.equalsHostByteOrder() ? "Yes" : "No") << "\n";
  return os;
}

std::ostream& operator<<(std::ostream& os, const TiffCraft::TiffImage::IFD::Entry& entry) {
  using namespace TiffCraft;
  os << "Tag: " << entry.tag() << "; "
     << "Type: " << entry.type() << "; "
     << "Count: " << entry.count() << ": "
     << "Value:";
  if (entry.type() == Type::ASCII) {
    // For ASCII type, print as a string
    os << " " << std::string(reinterpret_cast<const char*>(entry.values()), entry.bytes() - 1);
  } else {
    for (int i = 0; i < entry.count(); ++i) {
      const std::byte* value = entry.values() + i * TiffCraft::typeBytes(entry.type());
      switch (entry.type()) {
        case Type::BYTE:
          os << " " << *reinterpret_cast<const TypeTraits_t<Type::BYTE>*>(value);
          break;
        case Type::SHORT:
          os << " " << *reinterpret_cast<const TypeTraits_t<Type::SHORT>*>(value);
          break;
        case Type::LONG:
          os << " " << *reinterpret_cast<const TypeTraits_t<Type::LONG>*>(value);
          break;
        case Type::RATIONAL: {
          const auto* rational = reinterpret_cast<const TypeTraits_t<Type::RATIONAL>*>(value);
          os << " " << rational->numerator << "/" << rational->denominator;
          break;
        }
        case Type::SBYTE:
          os << " " << static_cast<int>(*reinterpret_cast<const TypeTraits_t<Type::SBYTE>*>(value));
          break;
        case Type::UNDEFINED: {
          const auto* undefined = reinterpret_cast<const TypeTraits_t<Type::UNDEFINED>*>(value);
          os << " 0x" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>(*undefined) << std::dec << std::setw(1) <<std::setfill(' ');
          break;
        }
        case Type::SSHORT:
          os << " " << *reinterpret_cast<const TypeTraits_t<Type::SSHORT>*>(value);
          break;
        case Type::SLONG:
          os << " " << *reinterpret_cast<const TypeTraits_t<Type::SLONG>*>(value);
          break;
        case Type::SRATIONAL: {
          const auto* rational = reinterpret_cast<const TypeTraits_t<Type::SRATIONAL>*>(value);
          os << " " << rational->numerator << "/" << rational->denominator;
          break;
        }
        case Type::FLOAT:
          os << " " << *reinterpret_cast<const TypeTraits_t<Type::FLOAT>*>(value);
          break;
        case Type::DOUBLE:
          os << " " << *reinterpret_cast<const TypeTraits_t<Type::DOUBLE>*>(value);
          break;
        default:
          os << " <Unsupported Type>";
      }
      if (i > 5) {
        // if there are more than 5 values, skip the rest
        os << " ...";
        break;
      }
    }
  }
  os << "\n";
  return os;
}

std::ostream& operator<<(std::ostream& os, const TiffCraft::TiffImage::IFD& ifd) {
  struct AutoRestore {
    std::ostream& os_;
    std::ios_base::fmtflags flags_;
    AutoRestore(std::ostream& os) : os_(os), flags_(os.flags()) {}
    ~AutoRestore() { os_.flags(flags_); }
  } autoRestore(os);

  os << "TIFF IFD:\n"
     << "  Entry count: " << ifd.entries().size() << "\n";
  int i = 0;
  for (const auto& entry : ifd.entries()) {
    os << std::setw(4) << i++ << "# " << entry.second;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const TiffCraft::TiffImage& image) {
  struct AutoRestore {
    std::ostream& os_;
    std::ios_base::fmtflags flags_;
    AutoRestore(std::ostream& os) : os_(os), flags_(os.flags()) {}
    ~AutoRestore() { os_.flags(flags_); }
  } autoRestore(os);

  os << "TIFF IMAGE START -----------------------\n"
     << image.header()
     << "IFD count: " << image.ifds().size() << "\n";
  for (int i = 0; i < image.ifds().size(); ++i) {
    os << std::setw(2) << i << ") " << image.ifds()[i];
  }
  os << "TIFF IMAGE END -------------------------\n";
  return os;
}
