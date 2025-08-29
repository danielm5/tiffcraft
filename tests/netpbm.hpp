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
// netpbm.hpp
// ==========
// This file contains functions to load NetPBM images (PBM, PGM, PPM).
//

#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>
#include <cstdint>
#include <numeric>
#include <type_traits>

namespace netpbm {

  template<typename T>
  struct RGB {
    T r, g, b;
    using value_type = T;
  };
  using RGB8 = RGB<uint8_t>;
  using RGB16 = RGB<uint16_t>;
  using RGB32 = RGB<uint32_t>;

  template <typename T>
  struct Image {
      int width;
      int height;
      size_t maxval;
      std::vector<T> pixels;
  };
  using Image8 = Image<RGB8>;
  using Image16 = Image<RGB16>;

  template <typename>
  struct is_rgb : std::false_type {};

  template <typename T>
  struct is_rgb<RGB<T>> : std::true_type {};

  template <typename T>
  inline constexpr bool is_rgb_v = is_rgb<T>::value;

  template <typename PixelType>
  Image<PixelType> read(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("Cannot open file");

    std::string line;

    // Read magic number
    std::getline(file, line);
    if constexpr (std::is_same_v<PixelType, bool>) {
      if (line != "P1") throw std::runtime_error("Not an ASCII PBM (P1)");
    } else if constexpr (is_rgb_v<PixelType>) {
      if (line != "P3") throw std::runtime_error("Not an ASCII PPM (P3)");
    } else {
      if (line != "P2") throw std::runtime_error("Not an ASCII PGM (P2)");
    }

    // Skip comments
    do {
        std::getline(file, line);
    } while (line[0] == '#');

    // Read width and height
    std::istringstream wh(line);
    int width, height;
    wh >> width >> height;

    // Read max value
    size_t maxval = 1;
    if constexpr (!std::is_same_v<PixelType, bool>) {
      file >> maxval;
    }
    size_t maxValue;
    if constexpr (is_rgb_v<PixelType>) {
      maxValue = std::numeric_limits<typename PixelType::value_type>::max();
    }
    else {
      maxValue = std::numeric_limits<PixelType>::max();
    }
    if (maxval != maxValue) {
      throw std::runtime_error("Unsupported maxval");
    }

    // Read pixel data
    std::vector<PixelType> pixels;
    pixels.reserve(width * height);
    if constexpr (is_rgb_v<PixelType>) {
      unsigned long int r, g, b;
      while (file >> r >> g >> b) {
        pixels.push_back(PixelType{
          static_cast<typename PixelType::value_type>(r),
          static_cast<typename PixelType::value_type>(g),
          static_cast<typename PixelType::value_type>(b)});
      }
    } else {
      unsigned long int gray;
      while (file >> gray) {
          pixels.push_back(static_cast<PixelType>(gray));
      }
    }
    if (pixels.size() != static_cast<size_t>(width * height))
        throw std::runtime_error("Pixel count does not match image size");

    return {width, height, maxval, std::move(pixels)};
  }

  template <typename T>
  Image<RGB<T>> readPPM(const std::string& filename) {
    return read<RGB<T>>(filename);
  }

  template <typename T>
  Image<T> readPGM(const std::string& filename) {
    return read<T>(filename);
  }

  inline Image<bool> readPBM(const std::string& filename) {
    return read<bool>(filename);
  }
}
