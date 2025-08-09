// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#include "TiffCraft.hpp"

#include <iostream>
#include <string>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace TiffCraft;

struct Image
{
  int width = 0;
  int height = 0;
  int channels = 0;       // Number of color channels (e.g., 3 for RGB, 4 for RGBA)
  int colStride = 0;      // Bytes per pixel
  int rowStride = 0;      // Bytes per row
  int chanStride = 0;     // Bytes per channel
  int bitDepth = 0;       // Bits per channel
  bool isFloat = false;   // True if floating-point data, false if integer data
  std::vector<std::byte> data; // Raw pixel data

  static Image makeGray8(int width, int height)
  {
    return Image{ width, height, 1, 1, width, 1, 8, false, std::vector<std::byte>(width * height) };
  }

  template <typename T>
  T* getData() { return reinterpret_cast<T*>(data.data()); }

  template <typename T>
  const T* getData() const { return reinterpret_cast<const T*>(data.data()); }
};

void saveImage(const std::string& filename, const Image& image)
{
  // only png is supported for now
  if (filename.substr(filename.find_last_of(".") + 1) != "png") {
    throw std::runtime_error("Only PNG format is supported for saving images");
  }

  // Single channel 8-bit non-float image
  if (image.channels == 1 && image.bitDepth == 8 && !image.isFloat) {
    if (image.colStride != 1 || image.chanStride != 1) {
      throw std::runtime_error("Invalid strides for single channel 8-bit image");
    }
    if (!stbi_write_png(filename.c_str(), image.width, image.height, image.channels,
          reinterpret_cast<const uint8_t*>(image.data.data()), image.rowStride)) {
      throw std::runtime_error("Failed to save PNG image");
    }
    return; // Success
  }

  throw std::runtime_error("Unsupported image format for saving");
}

class TiffHandler
{
  Image image_;
public:

  const Image& getImage() const { return image_; }


  int getAsInt(const TiffImage::IFD::Entry& entry) const
  {
    switch (entry.type()) {
      case Type::BYTE:
        return static_cast<int>(*reinterpret_cast<const TypeTraits_t<Type::BYTE>*>(entry.values()));
      case Type::ASCII:
        return static_cast<int>(*reinterpret_cast<const TypeTraits_t<Type::ASCII>*>(entry.values()));
      case Type::SHORT:
        return static_cast<int>(*reinterpret_cast<const TypeTraits_t<Type::SHORT>*>(entry.values()));
      case Type::LONG:
        return static_cast<int>(*reinterpret_cast<const TypeTraits_t<Type::LONG>*>(entry.values()));
      case Type::RATIONAL:
      {
        auto* rational = reinterpret_cast<const TypeTraits_t<Type::RATIONAL>*>(entry.values());
        return static_cast<int>(rational->numerator / rational->denominator);
      }
      default:
        throw std::runtime_error("Unknown TIFF entry type");
    }
  }

  int getInt(const TiffImage::IFD& ifd, Tag tag) const
  {
    auto it = ifd.entries().find(tag);
    if (it != ifd.entries().end()) {
      return getAsInt(it->second);
    }
    throw std::runtime_error("Tag not found: " + std::to_string(static_cast<int>(tag)));
  }

  int getInt(const TiffImage::IFD& ifd, Tag tag, int defaultValue) const
  {
    auto it = ifd.entries().find(tag);
    if (it != ifd.entries().end()) {
      return getAsInt(it->second);
    }
    return defaultValue;
  }

  void operator()(const TiffImage::IFD& ifd, TiffImage::ImageData imageData)
  {
    const int width = getInt(ifd, Tag::ImageWidth);
    const int height = getInt(ifd, Tag::ImageLength);
    const int compression = getInt(ifd, Tag::Compression, 1);
    const int photometricInterpretation = getInt(ifd, Tag::PhotometricInterpretation);
    const int bitsPerSample = getInt(ifd, Tag::BitsPerSample, 1);
    const int samplesPerPixel = getInt(ifd, Tag::SamplesPerPixel, 1);
    const int fillOrder = getInt(ifd, Tag::FillOrder, 1);

    // handle bilevel images  (1-bit per pixel)
    if (bitsPerSample == 1) {
      if (compression != 1) {
        throw std::runtime_error("Unsupported compression for bilevel image");
      }
      if (fillOrder != 1) {
        throw std::runtime_error("Unsupported fill order for bilevel image");
      }

      uint8_t zero = 0x00, one = 0xff; // BlackIsZero
      if (photometricInterpretation == 0) { // WhiteIsZero
        std::swap(zero, one);
      }

      image_ = Image::makeGray8(width, height);
      auto* data = image_.getData<uint8_t>();
      int row = 0, col = 0;
      for (const auto& rowStrip : imageData) {
        for (auto byte : rowStrip) {
          for (int i = 0; i < 8; ++i) {
            *data++ = (static_cast<uint8_t>(byte) & (1 << (7 - i))) ? one : zero;
            ++col;
            if (col >= width) {
              col = 0;
              ++row;
              break;
            }
          }
          if (row >= height) { break; }
        }
        if (row >= height) { break; }
      }

      return;
    }

    // otherwise image format is not supported
    throw std::runtime_error("Unsupported image format");
  }
};

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input.tif> [output.tif]" << std::endl;
    return 1;
  }

  const std::string input_file = argv[1];
  const std::string output_file = (argc >= 3) ? argv[2] : "output.png";

  LoadParams loadParams;
  loadParams.ifdIndex = 0; // Load the first IFD by default

  TiffHandler tiffHandler;

  try {
    std::cout << "Loading TIFF file: " << input_file << std::endl;
    load(input_file, std::ref(tiffHandler), loadParams);
  }
  catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }

  if (tiffHandler.getImage().data.empty()) {
    std::cerr << "No image data loaded." << std::endl;
    return 1;
  }

  std::cout << "Saving image as: " << output_file << std::endl;
  saveImage(output_file, tiffHandler.getImage());

  return 0;
}
