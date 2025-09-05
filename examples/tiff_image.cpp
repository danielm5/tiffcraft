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
// tiff_image.cpp
// ==============
// This is a minimal code example of how to use `TiffCraft::load()`.
//

#include <tiffcraft/TiffImage.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <image.tiff>" << std::endl;
    return 1;
  }

  std::cout << "TiffCraft version " << TiffCraft::version() << std::endl;
  std::cout << "Loading image: " << argv[1] << std::endl;

  TiffCraft::load(argv[1],
    [](const TiffCraft::TiffImage::Header& header,
        const TiffCraft::TiffImage::IFD& ifd,
        TiffCraft::TiffImage::ImageData imageData) {
      std::cout << "Loaded IFD with " << ifd.entries().size()
                << " entries." << std::endl;

      size_t totalBytes = 0;
      for (const auto& vec : imageData) {
          totalBytes += vec.size();
      }
      std::cout << "Image data bytes: " << totalBytes << std::endl;

      std::cout << "Image width: "
                << ifd.getEntry(TiffCraft::Tag::ImageWidth).values<uint16_t>()[0]
                << std::endl;
      std::cout << "Image height: "
                << ifd.getEntry(TiffCraft::Tag::ImageLength).values<uint16_t>()[0]
                << std::endl;

      // Add your own image processing code here

    });

  return 0;
}