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
// tiffStdOutTest.cpp
// ==================
// Unit tests for <TiffStdOut.hpp> functions.
//

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <tiffcraft/TiffStdOut.hpp>

#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>

using namespace TiffCraft;

namespace {
  bool contains(const std::string& text, const std::string& fragment) {
    return text.find(fragment) != std::string::npos;
  }
}

TEST_CASE("toString(Tag) maps known tags and formats unknown tags") {
  CHECK(toString(Tag::ImageWidth) == "ImageWidth");
  CHECK(toString(Tag::PhotometricInterpretation) == "PhotometricInterpretation");
  CHECK(toString(Tag::Null) == "Null");
  CHECK(toString(static_cast<Tag>(0xFFFF)) == "0xFFFF");
}

TEST_CASE("toString(Type) maps every type and formats unknown types") {
  CHECK(toString(Type::BYTE) == "BYTE");
  CHECK(toString(Type::ASCII) == "ASCII");
  CHECK(toString(Type::SHORT) == "SHORT");
  CHECK(toString(Type::LONG) == "LONG");
  CHECK(toString(Type::RATIONAL) == "RATIONAL");
  CHECK(toString(Type::SBYTE) == "SBYTE");
  CHECK(toString(Type::UNDEFINED) == "UNDEFINED");
  CHECK(toString(Type::SSHORT) == "SSHORT");
  CHECK(toString(Type::SLONG) == "SLONG");
  CHECK(toString(Type::SRATIONAL) == "SRATIONAL");
  CHECK(toString(Type::FLOAT) == "FLOAT");
  CHECK(toString(Type::DOUBLE) == "DOUBLE");
  CHECK(toString(static_cast<Type>(0xFFFF)) == "!UNKNOWN");
}

TEST_CASE("operator<< streams Tag and Type directly") {
  std::ostringstream tagStream;
  tagStream << Tag::ImageWidth;
  CHECK(contains(tagStream.str(), "ImageWidth"));

  std::ostringstream typeStream;
  typeStream << Type::LONG;
  CHECK(contains(typeStream.str(), "LONG"));
}

TEST_CASE("operator<< streams Header, Entry, IFD, and TiffImage") {
  const auto path = std::filesystem::path(__FILE__).parent_path()
    / "libtiff-pics" / "pc260001.tif";
  REQUIRE_MESSAGE(std::filesystem::exists(path),
    "Test image fixture is missing: ", path.string());

  const TiffImage image = TiffImage::read(path.string());

  std::ostringstream headerStream;
  headerStream << image.header();
  CHECK(contains(headerStream.str(), "TIFF Header:"));
  CHECK(contains(headerStream.str(), "Byte Order"));
  CHECK(contains(headerStream.str(), "First IFD Offset"));
  CHECK(contains(headerStream.str(), "Equals Host Byte Order"));

  REQUIRE(!image.ifds().empty());
  const auto& ifd = image.ifds().front();

  std::ostringstream entryStream;
  REQUIRE(!ifd.entries().empty());
  const auto& entry = ifd.entries().begin()->second;
  entryStream << entry;
  CHECK(contains(entryStream.str(), "Tag:"));
  CHECK(contains(entryStream.str(), "Type:"));
  CHECK(contains(entryStream.str(), "Count:"));
  CHECK(contains(entryStream.str(), "Value:"));

  std::ostringstream ifdStream;
  ifdStream << ifd;
  CHECK(contains(ifdStream.str(), "TIFF IFD:"));
  CHECK(contains(ifdStream.str(), "Entry count:"));

  std::ostringstream imageStream;
  imageStream << image;
  CHECK(contains(imageStream.str(), "TIFF IMAGE START"));
  CHECK(contains(imageStream.str(), "TIFF IMAGE END"));
}

TEST_CASE("operator<<(Entry) marks truncation only when values are omitted") {
  // Build a minimal TIFF-like stream: 8-byte preamble, 12-byte IFD entry,
  // then the out-of-line values, so Entry::read can parse the synthetic entry.
  const auto makeByteEntry = [](std::uint32_t count) {
    std::string bytes(20, '\0');
    const auto writeU16 = [&bytes](std::size_t pos, std::uint16_t value) {
      bytes[pos] = static_cast<char>(value & 0xFF);
      bytes[pos + 1] = static_cast<char>((value >> 8) & 0xFF);
    };
    const auto writeU32 = [&bytes](std::size_t pos, std::uint32_t value) {
      for (int b = 0; b < 4; ++b) {
        bytes[pos + b] = static_cast<char>((value >> (8 * b)) & 0xFF);
      }
    };
    writeU16(8, static_cast<std::uint16_t>(Tag::ImageWidth));
    writeU16(10, static_cast<std::uint16_t>(Type::BYTE));
    writeU32(12, count);
    writeU32(16, 20);  // even offset, within the stream
    bytes.resize(20 + count);
    for (std::uint32_t i = 0; i < count; ++i) {
      bytes[20 + i] = static_cast<char>(i + 1);
    }
    std::istringstream stream(bytes);
    stream.seekg(8);
    return TiffImage::IFD::Entry::read(stream);
  };

  std::ostringstream exact;
  exact << makeByteEntry(7);
  CHECK(contains(exact.str(), "Value: 1 2 3 4 5 6 7"));
  CHECK_FALSE(contains(exact.str(), "..."));

  std::ostringstream truncated;
  truncated << makeByteEntry(8);
  CHECK(contains(truncated.str(), "Value: 1 2 3 4 5 6 7 ..."));
}
