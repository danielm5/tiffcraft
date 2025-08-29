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
          std::vector<std::byte>(width * height * N * sT) };
      }
      return Image{
        width, height, N,             // width, height, channels
        width * N * sT,  N * sT, sT,  // strides: row, column, channel
        sT * 8,                       // bit depth
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

  template <typename PixelType>
  Rgb<PixelType> swap(const Rgb<PixelType>& rgb)
  {
    return { swap(rgb.b), swap(rgb.g), swap(rgb.r) };
  }

  struct RectInfo
  {
    int width = 0;
    int height = 0;
    int stride = 0; // Bytes per row
    int bitsPerSample = 0;
  };

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

    static std::vector<int> getBitsPerSample(const TiffImage::IFD& ifd)
    {
      if (ifd.entries().contains(Tag::BitsPerSample)) {
        return getIntVec(ifd, Tag::BitsPerSample);
      }
      return { 1 }; // default value if missing
    }

    static RectInfo getRectInfo(const TiffImage::IFD& ifd)
    {
      const std::vector<int> bitsPerSampleVec = getBitsPerSample(ifd);
      if (bitsPerSampleVec.empty()
        || std::any_of(bitsPerSampleVec.begin(), bitsPerSampleVec.end(),
          [&](int n) { return n != bitsPerSampleVec.front(); })) {
         throw FormatNotSupportedError("Unsupported bits per sample");
      }
      const int bitsPerSample = bitsPerSampleVec.front();
      const int samplesPerPixel = getInt(ifd, Tag::SamplesPerPixel, 1);
      const int planarConfiguration = getInt(ifd, Tag::PlanarConfiguration, 1);

      const int imageWidth = getWidth(ifd);
      const int imageHeight = getHeight(ifd);

      const int rowsPerStrip = getInt(ifd, Tag::RowsPerStrip, imageHeight);

      const int tileWidth = getInt(ifd, Tag::TileWidth, imageWidth);
      const int tileHeight = getInt(ifd, Tag::TileLength, rowsPerStrip);
      const int tileChannels = (planarConfiguration == 1 ? samplesPerPixel : 1);
      const int tileStride = (tileWidth * tileChannels * bitsPerSample + 7) / 8;

      return { tileWidth, tileHeight, tileStride, bitsPerSample };
    }

    template <typename SrcType, typename DstType, typename UnaryOp>
    void copyRectangles(
      const TiffImage::ImageData& imageData,  // source pixel data
      const RectInfo& rectInfo,               // source rectangle info
      size_t channels,                        // source image channels
      size_t planes,                          // source image planes
      bool equalsHostByteOrder,               // source data byte order
      UnaryOp&& op)
    {
      const int imageWidth = image_.width;
      const int imageHeight = image_.height;

      const int rectAcross = (imageWidth + (rectInfo.width - 1)) / rectInfo.width;
      const int rectDown = (imageHeight + (rectInfo.height - 1)) / rectInfo.height;
      const int rectsInPlane = rectAcross * rectDown;

      const int rectsInImage = rectAcross * rectDown * planes;
      if (rectsInImage != imageData.size()) {
        throw std::runtime_error("Rectangle count mismatch");
      }

      for (int plane = 0; plane < planes; ++plane) {
        for (int rectY = 0; rectY < rectDown; ++rectY) {
          for (int rectX = 0; rectX < rectAcross; ++rectX) {
            const auto& rectData = imageData[plane * rectsInPlane + rectY * rectAcross + rectX];
            auto currRectInfo = rectInfo;
            currRectInfo.width = std::min(rectInfo.width, imageWidth - rectX * rectInfo.width);
            currRectInfo.height = std::min(rectInfo.height, imageHeight - rectY * rectInfo.height);
            copyRectangle<SrcType, DstType, UnaryOp>(
              rectData,                    // source pixel data
              currRectInfo,                // source rectangle info
              channels,                    // source image channels
              equalsHostByteOrder,         // source data byte order
              plane,                       // destination plane
              rectX * rectInfo.width,      // destination column
              rectY * rectInfo.height,     // destination row
              std::forward<UnaryOp>(op));
          }
        }
      }
    }

    template <typename SrcType, typename DstType, typename UnaryOp>
    void copyRectangle(
      const std::vector<std::byte>& rectData, // source pixel data
      const RectInfo& rectInfo,               // source rectangle info
      size_t channels,                        // source image channels
      bool equalsHostByteOrder,               // source data byte order
      size_t dstPlane,                        // destination plane
      size_t dstX,                            // destination column
      size_t dstY,                            // destination row
      UnaryOp&& op)
    {
      constexpr size_t bitsPerSrcPixel = 8 * sizeof(SrcType);
      const auto* src = rectData.data();
      const auto* srcEnd = rectData.data() + rectData.size();

      size_t countAvail = 0;
      SrcType bitsAvail = 0;

      using ValueType = std::invoke_result_t<UnaryOp, DstType>;

      const size_t dstRowStride = image_.rowStride / sizeof(ValueType);
      const size_t dstColStride = image_.colStride / sizeof(ValueType);
      const size_t dstChanStride = image_.chanStride / sizeof(ValueType);
      const size_t dstPlaneStride = dstRowStride * image_.height;
      auto* dst = image_.dataPtr<ValueType>()
        + dstPlane * dstPlaneStride
        + dstY * dstRowStride
        + dstX * dstColStride;

      const auto* srcRow = src;
      auto* dstRow = dst;

      auto loop = [&](auto processor) {
        for (int row = 0; row < rectInfo.height; ++row) {
          if (src >= srcEnd) {
            // We've reached the end of the source tile
            throw std::runtime_error("Unexpected end of source tile");
          }
          srcRow = src;
          dstRow = dst;
          for (int col = 0; col < rectInfo.width; ++col) {
            auto* dstChan = dstRow;
            for (int chan = 0; chan < channels; ++chan) {
              DstType value = processor();
              *dstChan = std::invoke(op, value);
              dstChan += dstChanStride;
            }
            dstRow += dstColStride;
          }
          src += rectInfo.stride;
          dst += dstRowStride;
          // flush partial words when the row is complete
          if (countAvail > 0) {
            countAvail = 0;
            bitsAvail = 0;
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
        while (count < rectInfo.bitsPerSample) {
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
          const size_t n = std::min(rectInfo.bitsPerSample - count, countAvail);
          value <<= n;
          value |= (bitsAvail >> (bitsPerSrcPixel - n));
          count += n;
          // update available bits
          countAvail -= n;
          bitsAvail <<= n;
        }
        assert(count == rectInfo.bitsPerSample);
        return value;
      };

      if (rectInfo.bitsPerSample == bitsPerSrcPixel) {
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

      const auto rectInfo = getRectInfo(ifd);

      // create the image and copy the pixel data
      image_ = Image::make<DstType, 1>(getWidth(ifd), getHeight(ifd));
      using UnaryOp = std::function<DstType(DstType)>;
      copyRectangles<SrcType,DstType,UnaryOp>(
        imageData, // source pixel data
        rectInfo,  // rectangle info
        1, // source image channels
        1, // source image planes
        header.equalsHostByteOrder(), // source data byte order
        [&](DstType value) -> DstType {
          return (value * maxDstValue) / maxSrcValue;
        }
      );

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
      const int chan[3] = { numColors * 0, numColors * 1, numColors * 2 };

      constexpr int bitsPerDstPixel = 8 * sizeof(DstType);

      const auto rectInfo = getRectInfo(ifd);

      // create the image and copy the pixel data
      image_ = Image::make<DstType, 3>(getWidth(ifd), getHeight(ifd));
      using UnaryOp = std::function<Rgb<DstType>(DstType)>;
      copyRectangles<SrcType,DstType,UnaryOp>(
        imageData, // source pixel data
        rectInfo,  // rectangle info
        1, // source image channels
        1, // source image planes
        header.equalsHostByteOrder(), // source data byte order
        [&](DstType value) -> Rgb<DstType> {
          return {
            static_cast<DstType>(colorMap[chan[0] + value] >> (16 - bitsPerDstPixel)),
            static_cast<DstType>(colorMap[chan[1] + value] >> (16 - bitsPerDstPixel)),
            static_cast<DstType>(colorMap[chan[2] + value] >> (16 - bitsPerDstPixel))
          };
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

      const auto rectInfo = getRectInfo(ifd);

      // create the image and copy the pixel data
      image_ = Image::make<DstType, 3>(getWidth(ifd), getHeight(ifd), isPlanar);
      using UnaryOp = std::function<DstType(DstType)>;
      copyRectangles<SrcType,DstType,UnaryOp>(
        imageData, // source pixel data
        rectInfo,  // rectangle info
        isPlanar ? 1 : samplesPerPixel, // source image channels
        isPlanar ? samplesPerPixel : 1, // source image planes
        header.equalsHostByteOrder(),   // source data byte order
        [&](DstType value) -> DstType {
          return (value * maxDstValue) / maxSrcValue;
        }
      );
    }
  };

  // TiffExporter implementation for all supported image types ---------------
  class TiffExporterAny : public TiffExporter
  {
    bool exported_ = false;
  public:
    // Callback for TiffCraft::load() function
    void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) override
    {
      const int photometricInterpretation = getInt(ifd, Tag::PhotometricInterpretation, 1);

      if (photometricInterpretation >= 0 && photometricInterpretation <= 1) {
        // grayscale image types
        const int bitsPerSample = getInt(ifd, Tag::BitsPerSample, 1);
        if (bitsPerSample <= 8) {
          tryToExport<TiffExporterGray<uint8_t>>(header, ifd, imageData);
        } else if (bitsPerSample <= 15) {
          tryToExport<TiffExporterGray<uint16_t,uint8_t>>(header, ifd, imageData);
        } else if (bitsPerSample == 16) {
          tryToExport<TiffExporterGray<uint16_t>>(header, ifd, imageData);
        } else if (bitsPerSample <= 31) {
          tryToExport<TiffExporterGray<uint32_t,uint8_t>>(header, ifd, imageData);
        } else if (bitsPerSample == 32) {
          tryToExport<TiffExporterGray<uint32_t>>(header, ifd, imageData);
        }
      } else if (photometricInterpretation == 2) {
        // RGB image types
        std::vector<int> bitsPerSampleVec = getIntVec(ifd, Tag::BitsPerSample);
        if (bitsPerSampleVec.size() > 0
          && std::any_of(bitsPerSampleVec.begin(), bitsPerSampleVec.end(),
            [&](int n) { return n != bitsPerSampleVec.front(); })) {
          throw std::runtime_error("Unsupported bits per sample for RGB image");
        }
        const int bitsPerSample = bitsPerSampleVec.empty() ? 1 : bitsPerSampleVec.front();
        if (bitsPerSample <= 8) {
          tryToExport<TiffExporterRgb<uint8_t>>(header, ifd, imageData);
        } else if (bitsPerSample <= 15) {
          tryToExport<TiffExporterRgb<uint16_t,uint8_t>>(header, ifd, imageData);
        } else if (bitsPerSample == 16) {
          tryToExport<TiffExporterRgb<uint16_t>>(header, ifd, imageData);
        } else if (bitsPerSample <= 31) {
          tryToExport<TiffExporterRgb<uint32_t,uint8_t>>(header, ifd, imageData);
        } else if (bitsPerSample == 32) {
          tryToExport<TiffExporterRgb<uint32_t>>(header, ifd, imageData);
        }
      } else if (photometricInterpretation == 3) {
        // palette-color image types
        const int bitsPerSample = getInt(ifd, Tag::BitsPerSample, 1);
        if (bitsPerSample <= 8) {
          tryToExport<TiffExporterPalette<uint8_t>>(header, ifd, imageData);
        } else if (bitsPerSample <= 16) {
          tryToExport<TiffExporterPalette<uint16_t>>(header, ifd, imageData);
        }
      }

      if (!exported_ || image_.dataSize() == 0)
      { // image was not exported
        throw FormatNotSupportedError("No exporter can handle this image");
      }
    }

    template <typename Exporter>
    void tryToExport(const TiffImage::Header& header,
                     const TiffImage::IFD& ifd,
                     TiffImage::ImageData imageData)
    {
      if (!exported_) {
        // Image has not been exported yet
        try {
          // Try with the provided exporter
          Exporter exporter;
          exporter(header, ifd, imageData);
          image_ = exporter.takeImage();
          exported_ = true;
        } catch (const FormatNotSupportedError&) {
          // Image format not supported by the current exporter, ignore
        }
      }
    }
  };
}
