
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
    if constexpr (is_rgb_v<PixelType>) {
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
    size_t maxval;
    file >> maxval;
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
      int r, g, b;
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
}
