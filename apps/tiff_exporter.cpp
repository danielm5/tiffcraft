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
  int width;
  int height;
  int channels;   // Number of color channels (e.g., 3 for RGB, 4 for RGBA)
  int colStride;  // Bytes per pixel
  int rowStride;  // Bytes per row
  int chanStride; // Bytes per channel
  int bitDepth;   // Bits per channel
  bool isFloat;   // True if floating-point data, false if integer data
  std::vector<std::byte> data; // Raw pixel data
};

struct TiffHandler
{
  void operator()(const TiffImage::IFD& ifd, TiffImage::ImageData imageData)
  {
    std::cout << "Processing IFD: " << ifd << std::endl;
  }
};

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input.tif> [output.tif]" << std::endl;
    return 1;
  }

  const std::string input_file = argv[1];
  const std::string output_file = (argc >= 3) ? argv[2] : "output.tif";

  LoadParams loadParams;
  loadParams.ifdIndex = 0; // Load the first IFD by default

  TiffHandler tiffHandler;

  try {
    std::cout << "Loading TIFF file: " << input_file << std::endl;
    load(input_file, tiffHandler, loadParams);

  }
  catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
