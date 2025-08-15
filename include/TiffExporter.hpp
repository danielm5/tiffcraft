// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#pragma once

#include "TiffCraft.hpp"

#include <functional>
#include <algorithm>
#include <string>

namespace TiffCraft {

  struct Image
  {
    int width = 0;          // Image width
    int height = 0;         // Image height
    int channels = 0;       // Number of color channels (e.g., 3 for RGB, 4 for RGBA)
    int rowStride = 0;      // Bytes per row
    int colStride = 0;      // Bytes per pixel
    int chanStride = 0;     // Bytes per channel
    int bitDepth = 0;       // Bits per channel
    bool isFloat = false;   // True if floating-point data, false if integer data
    std::vector<std::byte> data;  // Raw pixel data

    template <typename T, int N = 1>
    static Image make(int width, int height)
    {
      constexpr int sT = static_cast<int>(sizeof(T));
      return Image{
        width, height, N,             // width, height, channels
        width * N * sT,  N * sT, sT,  // strides: row, column, channel
        sT * 8,                       // bit depth
        std::is_floating_point_v<T>,  // is float
        std::vector<std::byte>(width * height * N * sT) };
    }

    size_t dataSize() const { return data.size(); }

    std::byte* dataPtr() { return data.data(); }

    const std::byte* dataPtr() const { return data.data(); }

    template <typename T>
    T* dataPtr() { return reinterpret_cast<T*>(data.data()); }

    template <typename T>
    const T* dataPtr() const { return reinterpret_cast<const T*>(data.data()); }
  };

  struct Rgb
  {
    uint8_t r, g, b;
  };
  static_assert(sizeof(Rgb) == 3, "Rgb size must be 3 bytes");

  // Base class for TIFF exporters
  class TiffExporter
  {
  protected:
    Image image_;

  public:
    // Destructor
    virtual ~TiffExporter() = default;

    // Accessor for the exported image
    const Image& image() const { return image_; }

    // Callback for TiffCraft::load() function
    virtual void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) = 0;

    // Copies the values from a TIFF entry to a vector of integers.
    template <typename T>
    static std::vector<int> makeIntVec(const TiffImage::IFD::Entry& entry)
    {
      std::vector<int> v(entry.count());
      const auto* values = reinterpret_cast<const T*>(entry.values());
      for (size_t i = 0; i < entry.count(); ++i) {
        if constexpr (is_rational_v<T>) {
          v[i] = static_cast<int>(values[i].numerator / values[i].denominator);
        }
        else {
          v[i] = static_cast<int>(values[i]);
        }
      }
      return v;
    }

    // Copies the values from a TIFF entry to a vector of integers.
    // \param ifd The IFD containing the entry.
    // \param tag The tag to look for.
    // \param defaultValue An optional default value to return if the tag is
    //                     not found.
    // \return A vector of integers containing the values from the TIFF entry.
    static std::vector<int> getIntVec(const TiffImage::IFD& ifd, Tag tag,
      std::optional<std::vector<int>> defaultValue = std::nullopt)
    {
      auto it = ifd.entries().find(tag);
      if (it != ifd.entries().end()) {
        const auto& entry = it->second;
        return dispatchType(entry.type(), [&, entry]<typename ValueType>() {
            return makeIntVec<ValueType>(entry);
        });
      }
      if (defaultValue.has_value()) {
        return defaultValue.value();
      }
      throw std::runtime_error("Tag not found: " + std::to_string(static_cast<int>(tag)));
    }

    // Gets one value from a TIFF entry as an integer.
    // \param ifd The IFD containing the entry.
    // \param tag The tag to look for.
    // \param defaultValue An optional default value to return if the tag is
    //                     not found.
    // \return The integer value from the TIFF entry, or the default value if
    //         not found.
    static int getInt(const TiffImage::IFD& ifd, Tag tag,
      std::optional<int> defaultValue = std::nullopt)
    {
      std::optional<std::vector<int>> defaultValueVec;
      if (defaultValue.has_value()) {
        defaultValueVec = std::vector<int>{ defaultValue.value() };
      }
      auto v = getIntVec(ifd, tag, defaultValueVec);
      if (v.size() != 1) {
        throw std::runtime_error("Expected a single value for integer tag");
      }
      return v[0];
    }

    template <typename Comp = std::equal_to<>>
    static int require(
      const TiffImage::IFD& ifd, Tag tag, std::optional<int> defaultValue,
      int requiredValue, Comp&& comp = {})
    {
      const int samplesPerPixel = getInt(ifd, tag, defaultValue);
      if (!comp(samplesPerPixel, requiredValue)) {
        throw std::runtime_error(
          "Unsupported " + std::to_string(static_cast<int>(tag)) +
          " value: " + std::to_string(samplesPerPixel) +
          ", expected: " + std::to_string(requiredValue));
      }
      return samplesPerPixel;
    }

    template <typename Comp = std::equal_to<>>
    static int requireSamplesPerPixel(
      const TiffImage::IFD& ifd, int requiredValue, Comp&& comp = {})
    {
      return require(ifd, Tag::SamplesPerPixel, 1, requiredValue, comp);
    }

    template <typename Comp = std::equal_to<>>
    static int requirePhotometricInterpretation(
      const TiffImage::IFD& ifd, int requiredValue, Comp&& comp = {})
    {
      return require(ifd, Tag::PhotometricInterpretation, std::nullopt, requiredValue, comp);
    }

    template <typename Comp = std::equal_to<>>
    static int requireCompression(
      const TiffImage::IFD& ifd, int requiredValue, Comp&& comp = {})
    {
      return require(ifd, Tag::Compression, 1, requiredValue, comp);
    }

    template <typename Comp = std::equal_to<>>
    static int requireBitsPerSample(
      const TiffImage::IFD& ifd, int requiredValue, Comp&& comp = {})
    {
      return require(ifd, Tag::BitsPerSample, std::nullopt, requiredValue, comp);
    }

    template <typename Comp = std::equal_to<>>
    static std::vector<int> requireBitsPerSample(
      const TiffImage::IFD& ifd, const std::vector<int>& requiredValue,
      Comp&& comp = {})
    {
      auto bitsPerSample = getIntVec(ifd, Tag::BitsPerSample);
      if (!comp(bitsPerSample, requiredValue)) {
        throw std::runtime_error("Unsupported bits per sample");
      }
      return bitsPerSample;
    }

    template <typename Comp = std::equal_to<>>
    static int requireFillOrder(
      const TiffImage::IFD& ifd, int requiredValue, Comp&& comp = {})
    {
      return require(ifd, Tag::FillOrder, 1, requiredValue, comp);
    }

    static int getWidth(const TiffImage::IFD& ifd)
    {
      return getInt(ifd, Tag::ImageWidth);;
    }

    static int getHeight(const TiffImage::IFD& ifd)
    {
      return getInt(ifd, Tag::ImageLength);
    }

    // Iterator for TiffImage::ImageData
    struct Iterator
    {
      using Container = TiffImage::ImageData;
      using Iter1 = typename Container::iterator;
      using Iter2 = typename Container::value_type::iterator;

      // Iterator traits
      using difference_type = typename Iter2::difference_type;
      using value_type = typename Iter2::value_type;
      using pointer = typename Iter2::pointer;
      using reference = typename Iter2::reference;
      using iterator_category = typename Iter2::iterator_category;

      // Constructor
      Iterator() = default;

      // Iterator operations
      reference operator*() const { return *iter2_; }
      pointer operator->() const { return &(*iter2_); }

      // Prefix increment
      Iterator& operator++() {
        ++iter2_;
        if (iter2_ == iter1_->end()) {
          ++iter1_;
          if (iter1_ != container_->end()) {
            iter2_ = iter1_->begin();
          }
          else {
            iter2_ = Iter2(); // Set to end iterator if no more elements
          }
        }
        return *this;
      }

      // Postfix increment
      Iterator operator++(int) {
        Iterator tmp = *this;
        operator++();
        return tmp;
      }

      // Equality comparison
      friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
        return lhs.container_ == rhs.container_
          && lhs.iter1_ == rhs.iter1_ && lhs.iter2_ == rhs.iter2_;
      }

      // Inequality comparison
      friend bool operator!=(const Iterator& lhs, const Iterator& rhs) {
        return !(lhs == rhs);
      }

      // Create an iterator to the beginning
      static Iterator begin(Container& container) {
        if (container.empty()) {
          return Iterator(&container, container.end(), Iter2());
        }
        return Iterator(&container, container.begin(), container.front().begin());
      }

      // Create an iterator to the end
      static Iterator end(Container& container) {
        if (container.empty()) {
          return Iterator(&container, container.end(), Iter2());
        }
        return Iterator(&container, container.end(), Iter2());
      }

    private:
      Container* container_ = nullptr;
      Iter1 iter1_;
      Iter2 iter2_;

      // Private constructor
      Iterator(Container* container, Iter1 iter1, Iter2 iter2)
        : container_(container), iter1_(iter1), iter2_(iter2) {}
    };

    static Iterator begin(TiffImage::ImageData& container) {
      return Iterator::begin(container);
    }

    static Iterator end(TiffImage::ImageData& container) {
      return Iterator::end(container);
    }

    static void copy(
      const TiffImage::ImageData& imageData, std::vector<std::byte>& v)
    {
      size_t copied = 0;
      auto it = v.begin();
      for (const auto& strip : imageData) {
        if (strip.empty()) continue; // Skip empty strips
        if (it + strip.size() > v.end()) {
          throw std::runtime_error("Not enough space in vector to copy image data");
        }
        std::copy(strip.begin(), strip.end(), it);
        it += strip.size();
        copied += strip.size();
      }
      if (copied != v.size()) {
        throw std::runtime_error("Not enough pixel data for the image size");
      }
    }

    void invertColors() {
      uint8_t* data = image_.dataPtr<uint8_t>();
      for (size_t i = 0; i < image_.data.size(); ++i) {
        data[i] = ~data[i];
      }
    }
  };

  // TiffExporter implementation for Grayscale 8-bit images ------------------
  class TiffExporterGray8 : public TiffExporter
  {
  public:
    // Callback for TiffCraft::load() function
    void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) override
    {
      const int samplesPerPixel = requireSamplesPerPixel(ifd, 1);
      const int photometricInterpretation = requirePhotometricInterpretation(ifd, 1, std::less_equal<>());
      const int compression = requireCompression(ifd, 1);
      const auto bitsPerSample = requireBitsPerSample(ifd, 8);
      const int fillOrder = requireFillOrder(ifd, 1);

      // create the image and copy the pixel data
      image_ = Image::make<uint8_t, 1>(getWidth(ifd), getHeight(ifd));
      copy(imageData, image_.data);

      if (photometricInterpretation == 0) { // WhiteIsZero
        invertColors();
      }
    }
  };

  // TiffExporter implementation for Grayscale 16-bit images ------------------
  class TiffExporterGray16 : public TiffExporter
  {
  public:
    // Callback for TiffCraft::load() function
    void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) override
    {
      const int samplesPerPixel = requireSamplesPerPixel(ifd, 1);
      const int photometricInterpretation = requirePhotometricInterpretation(ifd, 1, std::less_equal<>());
      const int compression = requireCompression(ifd, 1);
      const auto bitsPerSample = requireBitsPerSample(ifd, 16);
      const int fillOrder = requireFillOrder(ifd, 1);

      // create the image and copy the pixel data
      image_ = Image::make<uint16_t, 1>(getWidth(ifd), getHeight(ifd));
      copy(imageData, image_.data);

      if (!header.equalsHostByteOrder()) {
        swapArray(image_.dataPtr<uint16_t>(), image_.data.size() / sizeof(uint16_t));
      }

      if (photometricInterpretation == 0) { // WhiteIsZero
        invertColors();
      }
    }
  };

    // TiffExporter implementation for Grayscale 32-bit images ------------------
  class TiffExporterGray32 : public TiffExporter
  {
  public:
    // Callback for TiffCraft::load() function
    void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) override
    {
      const int samplesPerPixel = requireSamplesPerPixel(ifd, 1);
      const int photometricInterpretation = requirePhotometricInterpretation(ifd, 1, std::less_equal<>());
      const int compression = requireCompression(ifd, 1);
      const auto bitsPerSample = requireBitsPerSample(ifd, 32);
      const int fillOrder = requireFillOrder(ifd, 1);

      // create the image and copy the pixel data
      image_ = Image::make<uint32_t, 1>(getWidth(ifd), getHeight(ifd));
      copy(imageData, image_.data);

      if (!header.equalsHostByteOrder()) {
        swapArray(image_.dataPtr<uint32_t>(), image_.data.size() / sizeof(uint32_t));
      }

      if (photometricInterpretation == 0) { // WhiteIsZero
        invertColors();
      }
    }
  };

  // TiffExporter implementation for RGB8 images -----------------------------
  class TiffExporterRgb8 : public TiffExporter
  {
  public:
    // Callback for TiffCraft::load() function
    void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) override
    {
      const int samplesPerPixel = requireSamplesPerPixel(ifd, 3, std::greater_equal<>());
      const int photometricInterpretation = requirePhotometricInterpretation(ifd, 2);
      const int compression = requireCompression(ifd, 1);
      const auto bitsPerSample = requireBitsPerSample(ifd, std::vector<int>(samplesPerPixel, 8));

      // NOTE: We ignore alpha channel if present for now

      // create the image and copy the pixel data
      image_ = Image::make<uint8_t, 3>(getWidth(ifd), getHeight(ifd));
      copy(imageData, image_.data);
    }
  };

      // int row = 0, col = 0;
      // auto* dst = image_.dataPtr();
      // for (auto src = begin(imageData); src != end(imageData); /* empty */) {
      //   *dst++ = *src++; // red
      //   if (src == end(imageData)) { break; }
      //   *dst++ = *src++; // green
      //   if (src == end(imageData)) { break; }
      //   *dst++ = *src++; // blue
      //   if (++col >= width) { // advance column and row counters
      //     col = 0; if (++row >= height) { break; }
      //   }
      // }
      // if (row < height) {
      //   throw std::runtime_error("Not enough pixel data for the image size");
      // }


}