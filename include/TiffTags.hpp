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
// TiffTags.hpp
// ============
// This file contains an enumeration of TIFF tags for easy access and display.
//

#pragma once

#include <string>
#include <cstdint>

namespace TiffCraft {
  enum class Tag : uint16_t {
    // Image File Directory (IFD) tags
    Null = 0x0000, // Not a real tag, used for padding or as a placeholder
    NewSubfileType = 0x00FE,
    SubfileType = 0x00FF,
    ImageWidth = 0x0100,
    ImageLength = 0x0101,
    BitsPerSample = 0x0102,
    Compression = 0x0103,
    PhotometricInterpretation = 0x0106,
    Thresholding = 0x0107,
    FillOrder = 0x010A,
    DocumentName = 0x010D,
    ImageDescription = 0x010E,
    Make = 0x010F,
    Model = 0x0110,
    StripOffsets = 0x0111,
    Orientation = 0x0112,
    SamplesPerPixel = 0x0115,
    RowsPerStrip = 0x0116,
    StripByteCounts = 0x0117,
    MinSampleValue = 0x0118,
    MaxSampleValue = 0x0119,
    XResolution = 0x011A,
    YResolution = 0x011B,
    PlanarConfiguration = 0x011C,
    PageName = 0x011D,
    ResolutionUnit = 0x0128,
    Software = 0x0131,
    DateTime = 0x0132,
    Artist = 0x013B,
    ColorMap = 0x0140,
    HalftoneHints = 0x0141,
    TileWidth = 0x0142,
    TileLength = 0x0143,
    TileOffsets = 0x0144,
    TileByteCounts = 0x0145,
    SampleFormat = 0x0153,
  };

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
}
