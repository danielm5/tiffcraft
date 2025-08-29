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
// tiff_exporter.cpp
// =================
// A simple program to export TIFF images in PNG format.
//
// Usage: tiff_exporter <input.tif> [output.png]
//

#include "TiffExporter.hpp"

#include <algorithm>
#include <iostream>
#include <string>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace TiffCraft;

Image convertImageTo8Bits(Image image);
void saveImage(const std::string& filename, const Image& image);

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input.tif> [output.tif]" << std::endl;
    return 1;
  }

  const std::string input_file = argv[1];
  const std::string output_file = (argc >= 3) ? argv[2] : "output.png";

  LoadParams loadParams;
  loadParams.ifdIndex = 0; // Load the first IFD by default

  TiffExporterAny tiffExporter;

  try {
    std::cout << "Loading TIFF file: " << input_file << std::endl;
    load(input_file, std::ref(tiffExporter), loadParams);
  }
  catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }

  if (tiffExporter.image().data.empty()) {
    std::cerr << "No image data loaded." << std::endl;
    return 1;
  }

  const Image image = convertImageTo8Bits(tiffExporter.takeImage());
  std::cout << "Saving image as: " << output_file << std::endl;
  saveImage(output_file, image);

  return 0;
}

Image convertImageTo8Bits(Image image)
{
  Image image8;
  if (image.channels == 1)
  {
    image8 = Image::make<uint8_t, 1>(image.width, image.height);
  }
  else if (image.channels == 3)
  {
    image8 = Image::make<uint8_t, 3>(image.width, image.height);
  }
  else
  {
    throw std::runtime_error("Unsupported number of channels for conversion to 8 bits");
  }

  auto copyImage = [&]<typename SrcType>() {

    auto* src = image.dataPtr<SrcType>();
    const auto srcRowStride = image.rowStride / sizeof(SrcType);
    const auto srcColStride = image.colStride / sizeof(SrcType);
    const auto srcChanStride = image.chanStride / sizeof(SrcType);
    const auto srcShift = sizeof(SrcType) * 8 - 8;

    auto* dst = image8.dataPtr<uint8_t>();

    for (int h = 0; h < image.height; ++h) {
      const auto* srcRow = src + h * srcRowStride;
      auto* dstRow = dst + h * image8.rowStride;
      for (int w = 0; w < image.width; ++w) {
        const auto* srcPixel = srcRow + w * srcColStride;
        auto* dstPixel = dstRow + w * image8.colStride;
        for (int c = 0; c < image.channels; ++c) {
          const auto* srcChan = srcPixel + c * srcChanStride;
          auto* dstChan = dstPixel + c * image8.chanStride;
          *dstChan = static_cast<uint8_t>(*srcChan >> srcShift);
        }
      }
    }
  };

  if (image.bitDepth == 16) {
    copyImage.template operator()<uint16_t>();
  }
  else if (image.bitDepth == 32) {
    copyImage.template operator()<uint32_t>();
  }
  else if (image.bitDepth == 8) {
    copyImage.template operator()<uint8_t>();
  }
  else {
    throw std::runtime_error("Unsupported bit depth for conversion to 8 bits");
  }

  return image8;
}

void saveImage(const std::string& filename, const Image& image)
{
  // only png is supported for now
  if (filename.substr(filename.find_last_of(".") + 1) != "png") {
    throw std::runtime_error("Only PNG format is supported for saving images");
  }

  // Single channel 8-bit non-float image
  if (image.channels == 1 && image.bitDepth == 8) {
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
  if (image.channels == 3 && image.bitDepth == 8) {
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
