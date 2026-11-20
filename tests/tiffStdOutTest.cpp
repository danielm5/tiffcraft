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
