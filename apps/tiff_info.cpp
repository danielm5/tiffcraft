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
//
// SPDX-License-Identifier: BSD-2-Clause
//
// ---------------------------------------------------------------------------
//
// tiff_info.cpp
// =============
// A simple program to print TIFF image metadata and IFD entries.
//
// Usage: tiff_info <image.tiff> [--tags]
//

#include <tiffcraft/TiffStdOut.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

  using TiffCraft::Tag;
  using TiffCraft::Type;
  using TiffCraft::TiffImage;

  // Read the first integer value of an entry without assuming its TIFF type.
  bool firstValue(const TiffImage::IFD& ifd, Tag tag, std::uint32_t& out) {
    if (!ifd.entries().contains(tag)) {
      return false;
    }
    const auto& entry = ifd.getEntry(tag);
    switch (entry.type()) {
      case Type::SHORT:
        out = entry.values<std::uint16_t>()[0];
        return true;
      case Type::LONG:
        out = entry.values<std::uint32_t>()[0];
        return true;
      default:
        return false;
    }
  }

  std::string firstValueString(const TiffImage::IFD& ifd, Tag tag) {
    std::uint32_t value = 0;
    if (!firstValue(ifd, tag, value)) {
      return "n/a";
    }
    return std::to_string(value);
  }

  // Some tags (e.g. BitsPerSample) hold one value per sample.
  std::string allValuesString(const TiffImage::IFD& ifd, Tag tag) {
    if (!ifd.entries().contains(tag)) {
      return "n/a";
    }
    const auto& entry = ifd.getEntry(tag);
    std::string result;
    const auto append = [&result](std::uint32_t value) {
      if (!result.empty()) {
        result += ", ";
      }
      result += std::to_string(value);
    };
    switch (entry.type()) {
      case Type::SHORT:
        for (const auto value : entry.values<std::uint16_t>()) {
          append(value);
        }
        break;
      case Type::LONG:
        for (const auto value : entry.values<std::uint32_t>()) {
          append(value);
        }
        break;
      default:
        return "n/a";
    }
    return result.empty() ? "n/a" : result;
  }

  std::string layoutString(const TiffImage::IFD& ifd) {
    if (ifd.entries().contains(Tag::TileOffsets)) {
      return "tiles (count " + std::to_string(ifd.getEntry(Tag::TileOffsets).count()) + ")";
    }
    if (ifd.entries().contains(Tag::StripOffsets)) {
      return "strips (count " + std::to_string(ifd.getEntry(Tag::StripOffsets).count()) + ")";
    }
    return "n/a";
  }

  std::string totalBytesString(const TiffImage::IFD& ifd) {
    Tag countTag = Tag::Null;
    if (ifd.entries().contains(Tag::StripByteCounts)) {
      countTag = Tag::StripByteCounts;
    } else if (ifd.entries().contains(Tag::TileByteCounts)) {
      countTag = Tag::TileByteCounts;
    }
    if (countTag == Tag::Null) {
      return "n/a";
    }

    const auto& entry = ifd.getEntry(countTag);
    std::uint64_t total = 0;
    switch (entry.type()) {
      case Type::SHORT:
        for (const auto value : entry.values<std::uint16_t>()) {
          total += value;
        }
        break;
      case Type::LONG:
        for (const auto value : entry.values<std::uint32_t>()) {
          total += value;
        }
        break;
      default:
        return "n/a";
    }
    return std::to_string(total);
  }

} // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <image.tiff> [--tags]\n";
    return 1;
  }

  const std::string input_file = argv[1];
  const bool showTags = (argc > 2 && std::string(argv[2]) == "--tags");

  try {
    // read() exposes the header and IFD entries without decoding pixel data.
    TiffCraft::TiffImage image = TiffCraft::TiffImage::read(input_file);

    std::cout << "File: " << input_file << "\n";
    std::cout << "TiffCraft version " << TiffCraft::version() << "\n";
    std::cout << image.header();

    std::size_t ifdIndex = 0;
    for (const auto& ifd : image.ifds()) {
      std::cout << "IFD " << ifdIndex++ << ":\n";

      if (showTags) {
        std::cout << "Entry count: " << ifd.entries().size() << "\n";
        for (const auto& item : ifd.entries()) {
          std::cout << item.second;
        }
      } else {
        std::cout << "  Image width: " << firstValueString(ifd, Tag::ImageWidth) << "\n";
        std::cout << "  Image height: " << firstValueString(ifd, Tag::ImageLength) << "\n";
        std::cout << "  Samples per pixel: " << firstValueString(ifd, Tag::SamplesPerPixel) << "\n";
        std::cout << "  Bits per sample: " << allValuesString(ifd, Tag::BitsPerSample) << "\n";
        std::cout << "  Photometric interpretation: "
                  << firstValueString(ifd, Tag::PhotometricInterpretation) << "\n";
        std::cout << "  Compression: " << firstValueString(ifd, Tag::Compression) << "\n";
        std::cout << "  Planar configuration: "
                  << firstValueString(ifd, Tag::PlanarConfiguration) << "\n";
        std::cout << "  Strip/tile info: " << layoutString(ifd) << "\n";
        std::cout << "  Total image data bytes: " << totalBytesString(ifd) << "\n";
      }
    }
  }
  catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}
