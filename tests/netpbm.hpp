
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>
#include <cstdint>
#include <numeric>

namespace netpbm {

  template<typename T>
  struct RGB {
    T r, g, b;
  };
  using RGB8 = RGB<uint8_t>;
  using RGB16 = RGB<uint16_t>;

  template <typename T>
  struct Image {
      int width;
      int height;
      int maxval;
      std::vector<T> pixels;
  };
  using Image8 = Image<RGB8>;
  using Image16 = Image<RGB16>;

  template <typename T>
  Image<RGB<T>> readPPM(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("Cannot open file");

    std::string line;
    // Read magic number
    std::getline(file, line);
    if (line != "P3") throw std::runtime_error("Not an ASCII PPM (P3)");

    // Skip comments
    do {
        std::getline(file, line);
    } while (line[0] == '#');

    // Read width and height
    std::istringstream wh(line);
    int width, height;
    wh >> width >> height;

    // Read max value
    int maxval;
    file >> maxval;
    if (maxval != std::numeric_limits<T>::max()) {
      throw std::runtime_error("Unsupported maxval");
    }

    // Read pixel data
    std::vector<RGB<T>> pixels;
    pixels.reserve(width * height);
    int r, g, b;
    while (file >> r >> g >> b) {
        pixels.push_back(RGB<T>{static_cast<T>(r), static_cast<T>(g), static_cast<T>(b)});
    }
    if (pixels.size() != static_cast<size_t>(width * height))
        throw std::runtime_error("Pixel count does not match image size");

    return {width, height, maxval, std::move(pixels)};
  }
}
