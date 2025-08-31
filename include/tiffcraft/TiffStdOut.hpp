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
