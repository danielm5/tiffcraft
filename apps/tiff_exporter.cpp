// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#include "TiffCraft.hpp"

#include <algorithm>
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

  static Image makeRGB8(int width, int height)
  {
    return Image{ width, height, 3, 3, width * 3, 1, 8, false, std::vector<std::byte>(width * height * 3) };
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

  // 3-channel 8-bit RGB image
  if (image.channels == 3 && image.bitDepth == 8 && !image.isFloat) {
    if (image.colStride != 3 || image.chanStride != 1) {
      throw std::runtime_error("Invalid strides for 3-channel 8-bit RGB image");
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

  template <typename T>
  std::vector<int> makeIntVec(const TiffImage::IFD::Entry& entry) const
  {
    std::vector<int> v(entry.count());
    const auto* values = reinterpret_cast<const T*>(entry.values());
    for (size_t i = 0; i < entry.count(); ++i) {
      if constexpr (std::is_same_v<T, Rational> || std::is_same_v<T, SRational>) {
        v[i] = static_cast<int>(values[i].numerator / values[i].denominator);
      }
      else {
        v[i] = static_cast<int>(values[i]);
      }
    }
    return v;
  }

  std::vector<int> getAsIntVec(const TiffImage::IFD::Entry& entry) const
  {
    switch (entry.type()) {
      case Type::BYTE:
        return makeIntVec<TypeTraits_t<Type::BYTE>>(entry);
      case Type::ASCII:
        return makeIntVec<TypeTraits_t<Type::ASCII>>(entry);
      case Type::SHORT:
        return makeIntVec<TypeTraits_t<Type::SHORT>>(entry);
      case Type::LONG:
        return makeIntVec<TypeTraits_t<Type::LONG>>(entry);
      case Type::RATIONAL:
        return makeIntVec<TypeTraits_t<Type::RATIONAL>>(entry);
      case Type::SBYTE:
        return makeIntVec<TypeTraits_t<Type::SBYTE>>(entry);
      case Type::UNDEFINED:
        return makeIntVec<TypeTraits_t<Type::UNDEFINED>>(entry);
      case Type::SSHORT:
        return makeIntVec<TypeTraits_t<Type::SSHORT>>(entry);
      case Type::SLONG:
        return makeIntVec<TypeTraits_t<Type::SLONG>>(entry);
      case Type::SRATIONAL:
        return makeIntVec<TypeTraits_t<Type::SRATIONAL>>(entry);
      case Type::FLOAT:
        return makeIntVec<TypeTraits_t<Type::FLOAT>>(entry);
      case Type::DOUBLE:
        return makeIntVec<TypeTraits_t<Type::DOUBLE>>(entry);
      default:
        throw std::runtime_error("Unknown TIFF entry type");
    }
  }

  std::vector<int> getIntVec(const TiffImage::IFD& ifd, Tag tag) const
  {
    auto it = ifd.entries().find(tag);
    if (it != ifd.entries().end()) {
      return getAsIntVec(it->second);
    }
    throw std::runtime_error("Tag not found: " + std::to_string(static_cast<int>(tag)));
  }

  int getAsInt(const TiffImage::IFD::Entry& entry) const
  {
    auto v = getAsIntVec(entry);
    if (v.size() != 1) {
      throw std::runtime_error("Expected a single value for integer tag");
    }
    return v[0];
  }

  int getInt(const TiffImage::IFD& ifd, Tag tag,
    std::optional<int> defaultValue = std::nullopt) const
  {
    auto it = ifd.entries().find(tag);
    if (it != ifd.entries().end()) {
      return getAsInt(it->second);
    }
    if (defaultValue.has_value()) {
      return defaultValue.value();
    }
    throw std::runtime_error("Tag not found: " + std::to_string(static_cast<int>(tag)));
  }

  void operator()(
    const TiffImage::Header& header,
    const TiffImage::IFD& ifd,
    TiffImage::ImageData imageData)
  {
    const int width = getInt(ifd, Tag::ImageWidth);
    const int height = getInt(ifd, Tag::ImageLength);
    const int compression = getInt(ifd, Tag::Compression, 1);
    const int photometricInterpretation = getInt(ifd, Tag::PhotometricInterpretation);
    const int samplesPerPixel = getInt(ifd, Tag::SamplesPerPixel, 1);

    //TODO check if image is striped or tiled
    // try cramps-tile.tif

    if (compression != 1) {
      throw std::runtime_error("Compression is not supported");
    }

    if (samplesPerPixel == 1) {
      // single channel images:
      // - bilevel image
      // - grayscale image
      // - palette-color image

      if (photometricInterpretation == 0 || photometricInterpretation == 1) {
        const int fillOrder = getInt(ifd, Tag::FillOrder, 1);
        if (fillOrder != 1) {
          throw std::runtime_error("Unsupported fill order for bilevel image");
        }

        // WhiteIsZero(0) or BlackIsZero(1): bilevel or grayscale image
        const int bitsPerSample = getInt(ifd, Tag::BitsPerSample, 1);

        image_ = Image::makeGray8(width, height);

        // TODO: only works for 1, 2, 4, and 8; it doesn't for 6
        if (bitsPerSample <= 8)
        { // copy the pixel data
          const int mask = (1 << bitsPerSample) - 1;
          const int pixelsPerByte = 8 / bitsPerSample;
          auto* data = image_.getData<uint8_t>();
          int row = 0, col = 0;
          for (const auto& rowStrip : imageData) {
            for (auto byte : rowStrip) {
              const uint8_t value = static_cast<uint8_t>(byte);
              for (int i = 0; i < pixelsPerByte; ++i) {
                const int shift = (pixelsPerByte - i - 1) * bitsPerSample;
                *data++ = (((value >> shift) & mask) * 255) / mask;
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

          if (photometricInterpretation == 0) { // WhiteIsZero
            // Invert the image
            uint8_t* data = image_.getData<uint8_t>();
            for (size_t i = 0; i < image_.data.size(); ++i) {
              data[i] = ~data[i];
            }
          }

          return;
        }

      }

      if (photometricInterpretation == 3)
      {
        const auto bitsPerSample = getInt(ifd, Tag::BitsPerSample, 1);
        if (bitsPerSample != 2 && bitsPerSample != 4 && bitsPerSample != 8 && bitsPerSample != 16) {
          throw std::runtime_error("Unsupported bits per sample for palette-color image");
        }

        const auto numColors = (1 << bitsPerSample);
        const auto colorMap = getIntVec(ifd, Tag::ColorMap);
        if (3 * numColors > colorMap.size()) {
          throw std::runtime_error("Color map size does not match bits per sample");
        }
        const int red = numColors * 0;
        const int green = numColors * 1;
        const int blue = numColors * 2;

        struct Pixel
        {
          uint8_t r, g, b;
        };
        static_assert(sizeof(Pixel) == 3, "Pixel size must be 3 bytes");

        image_ = Image::makeRGB8(width, height);

        if (bitsPerSample <= 8)
        { // copy the pixel data
          const int mask = (1 << bitsPerSample) - 1;
          const int pixelsPerByte = 8 / bitsPerSample;
          auto* dst = image_.getData<Pixel>();
          int row = 0, col = 0;
          for (const auto& rowStrip : imageData) {
            for (auto byte : rowStrip) {
              const int value = static_cast<int>(byte);
              for (int i = 0; i < pixelsPerByte; ++i) {
                const int index = (value >> ((pixelsPerByte - i - 1) * bitsPerSample)) & mask;
                dst->r = static_cast<uint8_t>(colorMap[red + index] / 257);
                dst->g = static_cast<uint8_t>(colorMap[green + index] / 257);
                dst->b = static_cast<uint8_t>(colorMap[blue + index] / 257);
                ++dst;
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

        if (bitsPerSample <= 16)
        { // copy the pixel data
          const int mask = (1 << bitsPerSample) - 1;
          const int pixelsPerShort = 16 / bitsPerSample;
          auto* dst = image_.getData<Pixel>();
          int row = 0, col = 0;
          for (auto& rowStrip : imageData) {
            assert(rowStrip.size() % 2 == 0);
            auto rowStripSize = rowStrip.size() / 2;
            auto* rowStripData = reinterpret_cast<uint16_t*>(rowStrip.data());
            if (!header.equalsHostByteOrder())
            {
              swapArray(rowStripData, rowStripSize);
            }
            for (size_t j = 0; j < rowStripSize; ++j) {
              const int value = static_cast<int>(rowStripData[j]);
              for (int i = 0; i < pixelsPerShort; ++i) {
                const int index = (value >> ((pixelsPerShort - i - 1) * bitsPerSample)) & mask;
                dst->r = static_cast<uint8_t>(colorMap[red + index] / 257);
                dst->g = static_cast<uint8_t>(colorMap[green + index] / 257);
                dst->b = static_cast<uint8_t>(colorMap[blue + index] / 257);
                ++dst;
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

      }

    } // if (samplesPerPixel == 1)

    if (samplesPerPixel >= 3)
    {
      const auto bitsPerSample = getIntVec(ifd, Tag::BitsPerSample);

      if (bitsPerSample.size() != static_cast<size_t>(samplesPerPixel)) {
        throw std::runtime_error("Bits per sample count does not match samples per pixel");
      }

      if (std::any_of(bitsPerSample.cbegin(), bitsPerSample.cend(),
          [](int b) { return b != 8; })) {
        // Ensure that all bits per sample are 8
        throw std::runtime_error("Unsupported bits per sample");
      }

      // handle RGB images
      if (photometricInterpretation == 2) {
        // NOTE: We ignore alpha channel if present for now

        struct Pixel
        {
          uint8_t r, g, b;
        };
        static_assert(sizeof(Pixel) == 3, "Pixel size must be 3 bytes");

        image_ = Image::makeRGB8(width, height);
        { // copy the pixel data
          auto* dst = image_.getData<Pixel>();
          int row = 0, col = 0;
          for (const auto& rowStrip : imageData) {
            const auto* src = reinterpret_cast<const Pixel*>(rowStrip.data());
            const size_t rowStripSize = rowStrip.size() / sizeof(Pixel);
            for (size_t i = 0; i < rowStripSize; ++i) {
              *dst++ = *src++;
              ++col;
              if (col >= width) {
                col = 0;
                ++row;
                if (row >= height) { break; }
              }
            }
            if (row >= height) { break; }
          }
        }

        return;
      }
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
