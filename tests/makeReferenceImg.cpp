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
// makeReferenceImg.cpp
// ====================
// A program that uses libtiff to create reference PBM/PGM/PPM images from
// TIFF files for the unit tests.
//
// This program was created because the reference images created with
// ImageMagick for 32-bit images were not matching the expected pixel values
// as extracted from the TIFF files. The command used with ImageMagick was:
//
// - magick flower-minisblack-32.tif -compress none PGM:flower-minisblack-32.pgm
// - magick flower-rgb-contig-32.tif -compress none PPM:flower-rgb-contig-32.ppm
// - magick flower-rgb-planar-32.tif -compress none PPM:flower-rgb-planar-32.ppm
//
// This program uses libtiff to read the TIFF files instead.
// ---------------------------------------------------------------------------

#include <string>
#include <memory>
#include <filesystem>
#include <iostream>

#include <tiffio.h>

#include "netpbm.hpp"

void makeReferenceImg(const std::vector<std::string>& testFiles);

int main(int argc, char* argv[]) {

  { // Gray
    { // 32 bits
      std::vector<std::string> testFiles = {
        "libtiff-pics/depth/flower-minisblack-32.tif",
        "reference_images/flower-minisblack-32.pgm",
      };
      makeReferenceImg(testFiles);
    }
  }

  { // RGB
    { // 32 bits
      std::vector<std::string> testFiles = {
        "libtiff-pics/depth/flower-rgb-contig-32.tif",
        "reference_images/flower-rgb-contig-32.ppm",
      };
      makeReferenceImg(testFiles);
    }
  }

  { // RGB Planar
    { // 32 bits
      std::vector<std::string> testFiles = {
        "libtiff-pics/depth/flower-rgb-planar-32.tif",
        "reference_images/flower-rgb-planar-32.ppm",
      };
      makeReferenceImg(testFiles);
    }
  }

  return 0;
}

std::filesystem::path getFilePath(std::filesystem::path relativePath) {
  std::filesystem::path test_file_path(__FILE__);
  std::filesystem::path test_dir = test_file_path.parent_path();
  return test_dir / relativePath;
}

class TiffImage {
public:
  TIFF* tiff = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;

  TiffImage(const std::string& tiffFilePath) {
    tiff = TIFFOpen(tiffFilePath.c_str(), "r");
    if (!tiff) {
      std::cerr << "Failed to open TIFF file: " << tiffFilePath << std::endl;
      throw std::runtime_error("Failed to open TIFF file");
    }
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
  }

  template <typename T>
  void saveAs(const std::string& refFilePath) {
    netpbm::Image<T> img;
    img.width = width;
    img.height = height;
    img.pixels.resize(width * height);

    uint16_t samplesPerPixel = 0;
    TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    uint16_t bitsPerSample = 0;
    TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);

    if constexpr (netpbm::is_rgb_v<T>) {
      img.maxval = std::numeric_limits<typename T::value_type>::max();
      if (samplesPerPixel != 3) {
        std::cerr << "Expected 3 samples per pixel for RGB image, got "
                  << samplesPerPixel << std::endl;
        throw std::runtime_error("Unsupported samples per pixel");
      }
    } else {
      img.maxval = std::numeric_limits<T>::max();
      if (samplesPerPixel != 1) {
        std::cerr << "Expected 1 sample per pixel for grayscale image, got "
                  << samplesPerPixel << std::endl;
        throw std::runtime_error("Unsupported samples per pixel");
      }
    }

    uint16_t planarConfig = 0;
    TIFFGetField(tiff, TIFFTAG_PLANARCONFIG, &planarConfig);
    if (planarConfig != PLANARCONFIG_CONTIG && planarConfig != PLANARCONFIG_SEPARATE) {
      std::cerr << "Only contiguous and separate planar configurations are supported." << std::endl;
      throw std::runtime_error("Unsupported planar configuration");
    }

    uint32_t numOfStrips = TIFFNumberOfStrips(tiff);
    tmsize_t stripSize = TIFFStripSize(tiff);
    std::unique_ptr<void, decltype(&_TIFFfree)> stripBuf(_TIFFmalloc(stripSize), _TIFFfree);
    if (!stripBuf) {
      std::cerr << "Failed to allocate memory for strip buffer." << std::endl;
      throw std::runtime_error("Memory allocation failed");
    }

    uint32_t rowsPerStrip = 0;
    TIFFGetField(tiff, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);

    if (planarConfig == PLANARCONFIG_CONTIG) {
      for (uint32_t stripIndex = 0; stripIndex < numOfStrips; ++stripIndex) {
        if (TIFFReadEncodedStrip(tiff, stripIndex, stripBuf.get(), stripSize) < 0) {
          std::cerr << "Failed to read strip " << stripIndex << std::endl;
          continue;
        }

        uint32_t stripRow = stripIndex * rowsPerStrip;
        uint32_t rowsInThisStrip = std::min(rowsPerStrip, height - stripRow);
        for (uint32_t row = 0; row < rowsInThisStrip; ++row) {
          T* srcRow = static_cast<T*>(stripBuf.get()) + row * width; // Assuming contiguous pixels
          for (uint32_t col = 0; col < width; ++col) {
            uint32_t pixelIndex = (stripRow + row) * width + col;
            img.pixels[pixelIndex] = srcRow[col];
          }
        }
      }
    } else if (planarConfig == PLANARCONFIG_SEPARATE) {
      // Handle separate planar configuration
      // Assuming 3 samples per pixel for RGB
      if constexpr (!netpbm::is_rgb_v<T>) {
        std::cerr << "Separate planar configuration is only supported for RGB images." << std::endl;
        throw std::runtime_error("Unsupported planar configuration for grayscale");
      }
      else {
        if (numOfStrips % 3 != 0) {
          std::cerr << "Number of strips is not a multiple of 3 for RGB separate planar." << std::endl;
          throw std::runtime_error("Invalid number of strips for RGB separate planar");
        }
        uint32_t stripsPerPlane = numOfStrips / 3;
        for (uint32_t planeIndex = 0; planeIndex < 3; ++planeIndex) {
          for (uint32_t planeStripIndex = 0; planeStripIndex < stripsPerPlane; ++planeStripIndex) {
            uint32_t stripIndex = planeIndex * stripsPerPlane + planeStripIndex;
            if (TIFFReadEncodedStrip(tiff, stripIndex, stripBuf.get(), stripSize) < 0) {
              std::cerr << "Failed to read strip " << stripIndex << std::endl;
              continue;
            }

            uint32_t stripRow = planeStripIndex * rowsPerStrip;
            uint32_t rowsInThisStrip = std::min(rowsPerStrip, height - stripRow);
            for (uint32_t row = 0; row < rowsInThisStrip; ++row) {
              auto srcRow = static_cast<typename T::value_type*>(stripBuf.get()) + row * width; // Assuming contiguous pixels
              for (uint32_t col = 0; col < width; ++col) {
                uint32_t pixelIndex = (stripRow + row) * width + col;
                img.pixels[pixelIndex][planeIndex] = srcRow[col];
              }
            }
          }
        }
      }
    }

    netpbm::write(refFilePath, img);
    std::cout << "Wrote reference image: " << refFilePath << std::endl;
  }

  ~TiffImage() {
    if (tiff) {
      TIFFClose(tiff);
      tiff = nullptr;
    }
  }
};

void makeReferenceImg(const std::vector<std::string>& testFiles)
{
  if (testFiles.size() % 2 != 0) {
    throw std::runtime_error("Test files must be in pairs.");
  }
  for (size_t i = 0; i < testFiles.size() / 2; ++i) {
    const auto tiffFilePath = getFilePath(testFiles[2*i + 0]);
    const auto refFilePath = getFilePath(testFiles[2*i + 1]);
    std::cout << "Test file: " + tiffFilePath.string() << std::endl;
    std::cout << "Reference file: " + refFilePath.string() << std::endl;

    TiffImage tiffImage(tiffFilePath.string());
    if (tiffImage.width == 0 || tiffImage.height == 0) {
      std::cerr << "Invalid image dimensions." << std::endl;
      continue;
    }
    if (refFilePath.extension() == ".pgm") {
      tiffImage.saveAs<uint32_t>(refFilePath.string());
    } else if (refFilePath.extension() == ".ppm") {
      tiffImage.saveAs<netpbm::RGB32>(refFilePath.string());
    } else {
      std::cerr << "Unsupported reference file format: " << refFilePath << std::endl;
      continue;
    }
  }
}
