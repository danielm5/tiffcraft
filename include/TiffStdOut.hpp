// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#pragma once

#include "TiffTags.hpp"

#include <iostream>
#include <iomanip>

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
