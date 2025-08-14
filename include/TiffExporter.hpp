// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#pragma once

#include "TiffCraft.hpp"

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
    std::vector<int> makeIntVec(const TiffImage::IFD::Entry& entry) const
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
    std::vector<int> getIntVec(const TiffImage::IFD& ifd, Tag tag,
      std::optional<std::vector<int>> defaultValue = std::nullopt) const
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
    int getInt(const TiffImage::IFD& ifd, Tag tag,
      std::optional<int> defaultValue = std::nullopt) const
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
  };

  class TiffExporterRgb8 : public TiffExporter
  {
  public:
    // Callback for TiffCraft::load() function
    void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) override
    {
      const int samplesPerPixel = getInt(ifd, Tag::SamplesPerPixel, 1);
      if (samplesPerPixel < 3)
      {
        throw std::runtime_error("Unsupported samples per pixel");
      }

      const int photometricInterpretation = getInt(ifd, Tag::PhotometricInterpretation);
      if (photometricInterpretation != 2) {
        throw std::runtime_error("Unsupported photometric interpretation");
      }

      const int compression = getInt(ifd, Tag::Compression, 1);
      if (compression != 1) {
        throw std::runtime_error("Compression is not supported");
      }

      const auto bitsPerSample = getIntVec(ifd, Tag::BitsPerSample);
      if (bitsPerSample.size() != static_cast<size_t>(samplesPerPixel)) {
        throw std::runtime_error("Bits per sample count does not match samples per pixel");
      }
      if (std::any_of(bitsPerSample.cbegin(), bitsPerSample.cend(),
          [](int b) { return b != 8; })) {
        // Ensure that all bits per sample are 8
        throw std::runtime_error("Unsupported bits per sample");
      }

      // NOTE: We ignore alpha channel if present for now

      // create the image
      const int width = getInt(ifd, Tag::ImageWidth);
      const int height = getInt(ifd, Tag::ImageLength);
      image_ = Image::make<uint8_t, 3>(width, height);

      // copy the pixel data
      int row = 0, col = 0;
      auto* dst = image_.dataPtr();
      for (auto src = begin(imageData); src != end(imageData); /* empty */) {
        *dst++ = *src++; // red
        if (src == end(imageData)) { break; }
        *dst++ = *src++; // green
        if (src == end(imageData)) { break; }
        *dst++ = *src++; // blue
        ++col;
        if (col >= width) {
          col = 0;
          ++row;
          if (row >= height) { break; }
        }
      }
      if (row < height) {
        throw std::runtime_error("Not enough pixel data for the image size");
      }
    }
  };
}