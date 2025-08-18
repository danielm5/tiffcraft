// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#include "TiffExporter.hpp"

#include <algorithm>
#include <iostream>
#include <string>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace TiffCraft;

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

  const Image& image() const { return image_; }
  [[nodiscard]] Image takeImage() const { return std::move(image_); }

  void operator()(
    const TiffImage::Header& header,
    const TiffImage::IFD& ifd,
    TiffImage::ImageData imageData)
  {
    try {
      TiffExporterPaletteBits<uint16_t> exporter;
      exporter(header, ifd, imageData);
      image_ = exporter.takeImage();
      return;
    }
    catch (const std::exception& ex) {
      //ignore
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

  if (tiffHandler.image().data.empty()) {
    std::cerr << "No image data loaded." << std::endl;
    return 1;
  }

  Image image = tiffHandler.takeImage();
  if (image.bitDepth == 16)
  { //convert to 8bits
    uint8_t* dst = image.dataPtr<uint8_t>();
    const uint16_t* src = image.dataPtr<uint16_t>();
    for (size_t i = 0; i < image.dataSize() / 2; ++i) {
      dst[i] = static_cast<uint8_t>(src[i] >> 8);
    }
    image.data.resize(image.dataSize() / 2);
    image.bitDepth = 8;
    image.rowStride /= 2;
    image.colStride /= 2;
    image.chanStride /= 2;
  }

  std::cout << "Saving image as: " << output_file << std::endl;
  saveImage(output_file, image);

  return 0;
}
