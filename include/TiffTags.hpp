// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

// This file is part is intended to be included only by `TiffCraft.hpp`.

#ifndef TIFF_TAGS_HPP_0A7E2B2C_8F8B_4E6A_9B2E_7D3C1A2F4B5C
# pragma error "This file should not be included directly. Include TiffCraft.hpp instead."
#else
# undef TIFF_TAGS_HPP_0A7E2B2C_8F8B_4E6A_9B2E_7D3C1A2F4B5C
#endif // TIFF_TAGS_HPP_0A7E2B2C_8F8B_4E6A_9B2E_7D3C1A2F4B5C

#include <cstdint>

namespace TiffCraft {

  enum class Tag : uint16_t {
    // Image File Directory (IFD) tags
    Null = 0x0000, // Not a real tag, used for padding or as a placeholder
    NewSubfileType = 0x00FE,
    ImageWidth = 0x0100,
    ImageLength = 0x0101,
    BitsPerSample = 0x0102,
    Compression = 0x0103,
    PhotometricInterpretation = 0x0106,
    Thresholding = 0x0107,
    FillOrder = 0x010A,
    ImageDescription = 0x010E,
    StripOffsets = 0x0111,
    Orientation = 0x0112,
    SamplesPerPixel = 0x0115,
    RowsPerStrip = 0x0116,
    StripByteCounts = 0x0117,
    XResolution = 0x011A,
    YResolution = 0x011B,
    PlanarConfiguration = 0x011C,
    PageName = 0x011D,
    ResolutionUnit = 0x0128,
    Software = 0x0131,
    DateTime = 0x0132,
    Artist = 0x013B,
    HalftoneHints = 0x0141,
    SampleFormat = 0x0153,

  };

}

std::ostream& operator<<(std::ostream& os, const TiffCraft::Tag& type) {
  struct AutoRestore {
    char fill_;
    std::ios_base::fmtflags flags_;
    std::ostream& os_;
    AutoRestore(std::ostream& os) : os_(os), flags_(os.flags()), fill_(os.fill()) {}
    ~AutoRestore() { os_.flags(flags_); os_.fill(fill_); }
  } autoRestore(os);

  switch (type) {
    case TiffCraft::Tag::Null: os << "Null"; break;
    case TiffCraft::Tag::NewSubfileType: os << "NewSubfileType"; break;
    case TiffCraft::Tag::ImageWidth: os << "ImageWidth"; break;
    case TiffCraft::Tag::ImageLength: os << "ImageLength"; break;
    case TiffCraft::Tag::BitsPerSample: os << "BitsPerSample"; break;
    case TiffCraft::Tag::Compression: os << "Compression"; break;
    case TiffCraft::Tag::PhotometricInterpretation: os << "PhotometricInterpretation"; break;
    case TiffCraft::Tag::Thresholding: os << "Thresholding"; break;
    case TiffCraft::Tag::FillOrder: os << "FillOrder"; break;
    case TiffCraft::Tag::ImageDescription: os << "ImageDescription"; break;
    case TiffCraft::Tag::StripOffsets: os << "StripOffsets"; break;
    case TiffCraft::Tag::Orientation: os << "Orientation"; break;
    case TiffCraft::Tag::SamplesPerPixel: os << "SamplesPerPixel"; break;
    case TiffCraft::Tag::RowsPerStrip: os << "RowsPerStrip"; break;
    case TiffCraft::Tag::StripByteCounts: os << "StripByteCounts"; break;
    case TiffCraft::Tag::XResolution: os << "XResolution"; break;
    case TiffCraft::Tag::YResolution: os << "YResolution"; break;
    case TiffCraft::Tag::PlanarConfiguration: os << "PlanarConfiguration"; break;
    case TiffCraft::Tag::ResolutionUnit: os << "ResolutionUnit"; break;
    case TiffCraft::Tag::PageName: os << "PageName"; break;
    case TiffCraft::Tag::Software: os << "Software"; break;
    case TiffCraft::Tag::DateTime: os << "DateTime"; break;
    case TiffCraft::Tag::Artist: os << "Artist"; break;
    case TiffCraft::Tag::HalftoneHints: os << "HalftoneHints"; break;
    case TiffCraft::Tag::SampleFormat: os << "SampleFormat"; break;
    default: os << "0x" << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<uint16_t>(type); break;
  }
  return os;
}