// Copyright (c) 2025 Daniel Moreno
// All rights reserved.

#pragma once

#include "TiffCraft.hpp"

#include <functional>
#include <algorithm>
#include <numeric>
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
    static Image make(int width, int height, bool isPlanar = false)
    {
      constexpr int sT = static_cast<int>(sizeof(T));
      if (isPlanar) {
        return Image{
          width, height, N,                     // width, height, channels
          width * sT,  sT, width * height * sT, // strides: row, column, channel
          sT * 8,                               // bit depth
          std::is_floating_point_v<T>,          // is float
          std::vector<std::byte>(width * height * N * sT) };
      }
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
    size_t dataSize() const { return data.size() / sizeof(T); }

    template <typename T>
    T* dataPtr() { return reinterpret_cast<T*>(data.data()); }

    template <typename T>
    const T* dataPtr() const { return reinterpret_cast<const T*>(data.data()); }
  };

  template <typename PixelType>
  struct Rgb
  {
    PixelType r, g, b;
  };

  using Rgb8 = Rgb<uint8_t>;
  using Rgb16 = Rgb<uint16_t>;
  using Rgb32 = Rgb<uint32_t>;

  static_assert(sizeof(Rgb8) == 3, "Rgb8 size must be 3 bytes");
  static_assert(sizeof(Rgb16) == 6, "Rgb16 size must be 6 bytes");
  static_assert(sizeof(Rgb32) == 12, "Rgb32 size must be 12 bytes");

  class FormatNotSupportedError : public std::runtime_error
  {
  public:
    explicit FormatNotSupportedError(const std::string& format)
      : std::runtime_error("Format not supported: " + format) {}
  };

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

    // Accessor for the exported image
    [[nodiscard]] Image takeImage() const { return std::move(image_); }

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
        return dispatchType(entry.type(), [&, entry]<Type type>() {
            return makeIntVec<TypeTraits_t<type>>(entry);
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
      const int value = getInt(ifd, tag, defaultValue);
      if (!comp(value, requiredValue)) {
        throw FormatNotSupportedError(
          "Unsupported " + std::to_string(static_cast<int>(tag)) +
          " value: " + std::to_string(value) +
          ", expected: " + std::to_string(requiredValue));
      }
      return value;
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
        throw FormatNotSupportedError("Unsupported bits per sample");
      }
      return bitsPerSample;
    }

    template <typename Comp = std::equal_to<>>
    static int requireFillOrder(
      const TiffImage::IFD& ifd, int requiredValue, Comp&& comp = {})
    {
      return require(ifd, Tag::FillOrder, 1, requiredValue, comp);
    }

    template <typename Comp = std::equal_to<>>
    static int requirePlanarConfiguration(
      const TiffImage::IFD& ifd, int requiredValue, Comp&& comp = {})
    {
      return require(ifd, Tag::PlanarConfiguration, 1, requiredValue, comp);
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

      // Get the current value as a specific type
      template <typename T>
      T& getAs() const { return *reinterpret_cast<T*>(&(*iter2_)); }

      // Advance the iterator to the next element of type T
      template <typename T>
      Iterator next() { return (*this += sizeof(T)); }

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

      // Compound assignment
      Iterator operator+=(int n) {
        for (int i = 0; i < n; ++i) {
          ++(*this);
        }
        return *this;
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

      // Create an const iterator to the beginning
      static Iterator begin(const Container& container) {
        return begin(const_cast<Container&>(container));
      }

      // Create an const iterator to the end
      static Iterator end(const Container& container) {
        return end(const_cast<Container&>(container));
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

    static Iterator begin(const TiffImage::ImageData& container) {
      return Iterator::begin(container);
    }

    static Iterator end(const TiffImage::ImageData& container) {
      return Iterator::end(container);
    }

    // static void copy(
    //   const TiffImage::ImageData& imageData, std::vector<std::byte>& v)
    // {
    //   size_t copied = 0;
    //   auto it = v.begin();
    //   for (const auto& strip : imageData) {
    //     if (strip.empty()) continue; // Skip empty strips
    //     if (it + strip.size() > v.end()) {
    //       throw std::runtime_error("Not enough space in vector to copy image data");
    //     }
    //     std::copy(strip.begin(), strip.end(), it);
    //     it += strip.size();
    //     copied += strip.size();
    //   }
    //   if (copied != v.size()) {
    //     throw std::runtime_error("Not enough pixel data for the image size");
    //   }
    // }

    template <typename SrcType, typename DstType, typename UnaryOp>
    static void copyPixels(
      const TiffImage::ImageData& imageData,  // source pixel data
      size_t width,                           // source image width
      size_t height,                          // source image height
      size_t channels,                        // source image channels
      size_t bitsPerSample,                   // source bits per sample
      bool isPlanar,                          // source is planar
      bool equalsHostByteOrder,               // source data byte order
      UnaryOp&& op)
    {
      constexpr size_t bitsPerSrcPixel = 8 * sizeof(SrcType);
      auto src = begin(imageData);

      size_t countAvail = 0;
      SrcType bitsAvail = 0;

      auto loop_by_row_col_chan = [&](auto processor) {
        for (int row = 0; row < height; ++row) {
          for (int col = 0; col < width; ++col) {
            for (int chan = 0; chan < channels; ++chan) {
              DstType value = processor();
              std::invoke(op, value); // Apply the unary operation
            }
          }
          // flush partial words when the row is complete
          if (countAvail > 0) {
            countAvail = 0;
            bitsAvail = 0;
          }
        }
      };

      auto loop_by_chan_row_col = [&](auto processor) {
        for (int chan = 0; chan < channels; ++chan) {
          for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
              DstType value = processor();
              std::invoke(op, value); // Apply the unary operation
            }
            // flush partial words when the row is complete
            if (countAvail > 0) {
              countAvail = 0;
              bitsAvail = 0;
            }
          }
        }
      };

      auto process_word = [&]() -> DstType {
        if (src == end(imageData)) {
          throw std::runtime_error("No pixel data available");
        }
        DstType value = src.getAs<SrcType>();
        src.next<SrcType>();
        if (!equalsHostByteOrder) {
          value = swap(value);
        }
        return value;
      };

      auto process_bits = [&]() -> DstType {
        size_t count = 0;
        DstType value = 0;
        while (count < bitsPerSample) {
          // make sure we have some bits available
          if (countAvail == 0) {
            if (src == end(imageData)) {
              throw std::runtime_error("No pixel data available");
            }
            bitsAvail = src.getAs<SrcType>();
            src.next<SrcType>();
            if (!equalsHostByteOrder) {
              bitsAvail = swap(bitsAvail);
            }
            countAvail = bitsPerSrcPixel;
          }
          // consume available bits
          const size_t n = std::min(bitsPerSample - count, countAvail);
          value <<= n;
          value |= (bitsAvail >> (bitsPerSrcPixel - n));
          count += n;
          // update available bits
          countAvail -= n;
          bitsAvail <<= n;
        }
        assert(count == bitsPerSample);
        return value;
      };

      if (bitsPerSample == bitsPerSrcPixel) {
        // Fast path: bits per sample matches source pixel size
        if (isPlanar)
          loop_by_chan_row_col(process_word);
        else
          loop_by_row_col_chan(process_word);
      } else {
        // Slow path: bits per sample does not match source pixel size
        if (isPlanar)
          loop_by_chan_row_col(process_bits);
        else
          loop_by_row_col_chan(process_bits);
      }
      if (src != end(imageData)) {
        throw std::runtime_error("Not enough pixel data for the image size");
      }
    }

    template <typename SrcType, typename DstType, typename UnaryOp>
    void copyTiles(
      const TiffImage::ImageData& imageData,  // source pixel data
      size_t tileWidth,                       // tile image width
      size_t tileHeight,                      // tile image height
      size_t tileStride,                      // tile row stride in bytes
      size_t channels,                        // source image channels
      size_t planes,                          // source image planes
      size_t bitsPerSample,                   // source bits per sample
      bool equalsHostByteOrder,               // source data byte order
      UnaryOp&& op)
    {
      const int imageWidth = image_.width;
      const int imageHeight = image_.height;

      const int tilesAcross = (imageWidth + (tileWidth - 1)) / tileWidth;
      const int tilesDown = (imageHeight + (tileHeight - 1)) / tileHeight;

      const int tilesInImage = tilesAcross * tilesDown * planes;
      if (tilesInImage != imageData.size()) {
        throw std::runtime_error("Tile count mismatch");
      }

      for (int tileY = 0; tileY < tilesDown; ++tileY) {
        for (int tileX = 0; tileX < tilesAcross; ++tileX) {
          const auto& tile = imageData[tileY * tilesAcross + tileX];
          const int width = std::min(tileWidth, imageWidth - tileX * tileWidth);
          const int height = std::min(tileHeight, imageHeight - tileY * tileHeight);
          copyTile<SrcType, DstType, UnaryOp>(
            tile,                        // source pixel data
            width,                       // tile image width
            height,                      // tile image height
            tileStride,                  // tile row stride in bytes
            channels,                    // source image channels
            planes,                      // source image planes
            bitsPerSample,               // source bits per sample
            equalsHostByteOrder,         // source data byte order
            tileX * tileWidth,           // destination column
            tileY * tileHeight,          // destination row
            std::forward<UnaryOp>(op));
        }
      }

    }

    template <typename SrcType, typename DstType, typename UnaryOp>
    void copyTile(
      const std::vector<std::byte>& tile,     // source pixel data
      size_t tileWidth,                       // tile image width
      size_t tileHeight,                      // tile image height
      size_t tileStride,                      // tile row stride in bytes
      size_t channels,                        // source image channels
      size_t planes,                          // source image planes
      size_t bitsPerSample,                   // source bits per sample
      bool equalsHostByteOrder,               // source data byte order
      size_t dstX,                            // destination column
      size_t dstY,                            // destination row
      UnaryOp&& op)
    {
      constexpr size_t bitsPerSrcPixel = 8 * sizeof(SrcType);
      const auto* src = tile.data();

      size_t countAvail = 0;
      SrcType bitsAvail = 0;

      const size_t dstRowStride = image_.rowStride / sizeof(DstType);
      const size_t dstColStride = image_.colStride / sizeof(DstType);
      auto* dst = image_.dataPtr<DstType>()
        + dstY * dstRowStride + dstX * dstColStride;

      const auto* srcRow = src;
      auto* dstRow = dst;

      auto loop = [&](auto processor) {
        for (int plane = 0; plane < planes; ++plane) {
          for (int row = 0; row < tileHeight; ++row) {
            srcRow = src;
            dstRow = dst;
            for (int col = 0; col < tileWidth; ++col) {
              for (int chan = 0; chan < channels; ++chan) {
                DstType value = processor();
                *dstRow++ = std::invoke(op, value); // Apply the unary operation
              }
            }
            src += tileStride;
            dst += dstRowStride;
            // flush partial words when the row is complete
            if (countAvail > 0) {
              countAvail = 0;
              bitsAvail = 0;
            }
          }
        }
      };

      auto process_word = [&]() -> DstType {
        DstType value = *reinterpret_cast<const SrcType*>(&(*srcRow));
        srcRow += sizeof(SrcType);
        if (!equalsHostByteOrder) {
          value = swap(value);
        }
        return value;
      };

      auto process_bits = [&]() -> DstType {
        size_t count = 0;
        DstType value = 0;
        while (count < bitsPerSample) {
          // make sure we have some bits available
          if (countAvail == 0) {
            bitsAvail = *reinterpret_cast<const SrcType*>(&(*srcRow));
            srcRow += sizeof(SrcType);
            if (!equalsHostByteOrder) {
              bitsAvail = swap(bitsAvail);
            }
            countAvail = bitsPerSrcPixel;
          }
          // consume available bits
          const size_t n = std::min(bitsPerSample - count, countAvail);
          value <<= n;
          value |= (bitsAvail >> (bitsPerSrcPixel - n));
          count += n;
          // update available bits
          countAvail -= n;
          bitsAvail <<= n;
        }
        assert(count == bitsPerSample);
        return value;
      };

      if (bitsPerSample == bitsPerSrcPixel) {
        // Fast path: bits per sample matches source pixel size
        loop(process_word);
      } else {
        // Slow path: bits per sample does not match source pixel size
        loop(process_bits);
      }
    }

    void invertColors() {
      uint8_t* data = image_.dataPtr<uint8_t>();
      for (size_t i = 0; i < image_.data.size(); ++i) {
        data[i] = ~data[i];
      }
    }
  };

  // TiffExporter implementation for Grayscale images ------------------------
  template <typename DstType, typename SrcType = DstType>
  class TiffExporterGray : public TiffExporter
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
      const int fillOrder = requireFillOrder(ifd, 1);

      const int bitsPerSample = getInt(ifd, Tag::BitsPerSample, 1);

      constexpr size_t maxDstValue = std::numeric_limits<DstType>::max();
      const size_t maxSrcValue = bitsPerSample < sizeof(size_t) * 8
        ? (size_t(1) << bitsPerSample) - 1 : std::numeric_limits<size_t>::max();

      const int imageWidth = getWidth(ifd);
      const int imageHeight = getHeight(ifd);

      const int rowsPerStrip = getInt(ifd, Tag::RowsPerStrip, imageHeight);

      const int tileWidth = getInt(ifd, Tag::TileWidth, imageWidth);
      const int tileHeight = getInt(ifd, Tag::TileLength, rowsPerStrip);
      const int tileStride = (tileWidth * bitsPerSample + 7) / 8;

      // create the image and copy the pixel data
      image_ = Image::make<DstType, 1>(imageWidth, imageHeight);

      #if 1
      using Op = std::function<DstType(DstType)>;
      copyTiles<SrcType,DstType,Op>(
        imageData, // source pixel data
        tileWidth, // tile width
        tileHeight, // tile height
        tileStride, // tile row stride in bytes
        1, // source image channels
        1, // source image planes
        bitsPerSample, // source bits per sample
        header.equalsHostByteOrder(), // source data byte order
        [&](DstType value) -> DstType {
          return (value * maxDstValue) / maxSrcValue;
        }
      );

      #else

      auto* dst = image_.dataPtr<DstType>();
      using Op = std::function<void(DstType)>;
      copyPixels<SrcType,DstType,Op>(
        imageData, // source pixel data
        image_.width, // source image width
        image_.height, // source image height
        1, // source image channels
        bitsPerSample, // source bits per sample
        false, // source is planar
        header.equalsHostByteOrder(), // source data byte order
        [&](DstType value) {
          *dst++ = (value * maxDstValue) / maxSrcValue;
        }
      );

      #endif

      if (photometricInterpretation == 0) { // WhiteIsZero
        invertColors();
      }
    }
  };

  // TiffExporter implementation for Palette-color images --------------------
  template <typename DstType, typename SrcType = DstType>
  class TiffExporterPalette : public TiffExporter
  {
  public:
    // Callback for TiffCraft::load() function
    void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) override
    {
      const int samplesPerPixel = requireSamplesPerPixel(ifd, 1);
      const int photometricInterpretation = requirePhotometricInterpretation(ifd, 3);
      const int compression = requireCompression(ifd, 1);
      const int fillOrder = requireFillOrder(ifd, 1);

      const int bitsPerSample = getInt(ifd, Tag::BitsPerSample, 1);

      const auto numColors = (1 << bitsPerSample);
      const auto colorMap = getIntVec(ifd, Tag::ColorMap);
      if (3 * numColors > colorMap.size()) {
        throw std::runtime_error("Color map size does not match bits per sample");
      }
      const int red = numColors * 0;
      const int green = numColors * 1;
      const int blue = numColors * 2;

      constexpr int bitsPerDstPixel = 8 * sizeof(DstType);

      // create the image and copy the pixel data
      image_ = Image::make<DstType, 3>(getWidth(ifd), getHeight(ifd));
      auto* dst = image_.dataPtr<Rgb<DstType>>();
      using UnaryOp = std::function<void(DstType)>;
      copyPixels<SrcType,DstType,UnaryOp>(
        imageData, // source pixel data
        image_.width, // source image width
        image_.height, // source image height
        1, // source image channels
        bitsPerSample, // source bits per sample
        false, // source is planar
        header.equalsHostByteOrder(), // source data byte order
        [&](DstType value) {
          dst->r = static_cast<DstType>(colorMap[red + value] >> (16 - bitsPerDstPixel));
          dst->g = static_cast<DstType>(colorMap[green + value] >> (16 - bitsPerDstPixel));
          dst->b = static_cast<DstType>(colorMap[blue + value] >> (16 - bitsPerDstPixel));
          ++dst;
        }
      );
    }
  };

  // TiffExporter implementation for RGB images ------------------------------
  template <typename DstType, typename SrcType = DstType>
  class TiffExporterRgb : public TiffExporter
  {
  public:
    // Callback for TiffCraft::load() function
    void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) override
    {
      const int samplesPerPixel = requireSamplesPerPixel(ifd, 3);
      const int photometricInterpretation = requirePhotometricInterpretation(ifd, 2);
      const int compression = requireCompression(ifd, 1);

      const int planarConfiguration = requirePlanarConfiguration(ifd, -1,
        [](int value, int requiredValue) {
          return value == 1 || value == 2; // 1 for Contiguous, 2 for Planar
        });
      const bool isPlanar = (planarConfiguration == 2);

      const auto bitsPerSampleVec = getIntVec(ifd, Tag::BitsPerSample);
      if (bitsPerSampleVec.size() != 3) {
        throw std::runtime_error("Expected 3 bits per sample for RGB image");
      }

      const int bitsPerSample = bitsPerSampleVec.front();
      for (auto bits : bitsPerSampleVec) {
        if (bits != bitsPerSample) {
          throw std::runtime_error("Unsupported bits per sample for RGB image");
        }
      }

      constexpr size_t maxDstValue = std::numeric_limits<DstType>::max();
      const size_t maxSrcValue = bitsPerSample < sizeof(size_t) * 8
        ? (size_t(1) << bitsPerSample) - 1 : std::numeric_limits<size_t>::max();

      // create the image and copy the pixel data
      image_ = Image::make<DstType, 3>(getWidth(ifd), getHeight(ifd), isPlanar);
      auto* dst = image_.dataPtr<DstType>();
      using UnaryOp = std::function<void(DstType)>;
      copyPixels<SrcType,DstType,UnaryOp>(
        imageData, // source pixel data
        image_.width, // source image width
        image_.height, // source image height
        3, // source image channels
        bitsPerSample, // source bits per sample
        isPlanar, // source is planar
        header.equalsHostByteOrder(), // source data byte order
        [&](DstType value) {
          *dst++ = (value * maxDstValue) / maxSrcValue;
        }
      );
    }
  };
}
