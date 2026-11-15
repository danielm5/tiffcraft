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
// fixtureCheck.hpp
// ================
// Shared helper for tests: fail with an actionable message when a test
// fixture is an unresolved Git LFS pointer file instead of real content.
//

#ifndef TIFFCRAFT_FIXTURECHECK_HPP
#define TIFFCRAFT_FIXTURECHECK_HPP

#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>

inline void requireRealFixture(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  char head[64] = {};
  file.read(head, sizeof(head));
  if (std::string(head, file.gcount()).rfind("version https://git-lfs", 0) == 0) {
    throw std::runtime_error(
      "Git LFS pointer file, not a TIFF: " + path.string() +
      "\nInstall git-lfs and run: git -C tests/libtiff-pics lfs pull");
  }
}

#endif // TIFFCRAFT_FIXTURECHECK_HPP
