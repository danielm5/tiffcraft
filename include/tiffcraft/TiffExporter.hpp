// BSD 2-Clause License
// Copyright (c) 2025-2026 Daniel Moreno
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
// TiffExporter.hpp
// ================
// The classes in this file are used to read TIFF pixel data in various formats
// and copy them to a simple flat-buffer of 8, 16, or 32 bits per channel. This
// pre-processing step can be useful for converting TIFF images into a more
// manageable format for further processing or analysis.
//
// Code like the following could be used to read TIFF images and get a copy of
// their pixel data in a flat buffer:
//
//     #include <tiffcraft/TiffExporter.hpp>
//     #include <functional>
//
//     int main(int argc, char* argv[]) {
//       if (argc < 2) { return 1; }
//       TiffCraft::TiffExporterAny tiffExporter;
//       TiffCraft::load(argv[1], std::ref(tiffExporter));
//       auto image = tiffExporter.takeImage();
//
//       // your own image processing code
//
//       return 0;
//     }
//

#pragma once

#include "TiffImage.hpp"

#include <functional>
#include <algorithm>
#include <numeric>
#include <string>
#include <array>

namespace TiffCraft
{
  //! \brief A simple struct to hold the width and height of a rectangle.
  struct RectSize
  {
    int width = 0;
    int height = 0;
  };

  //! \brief A simple struct to hold 2-dimensional values.
  template <typename T>
  struct Vec2 { T x, y; };

  using Vec2i = Vec2<int>;

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

    template <typename T>
    static Image make(int width, int height, int channels = 1, bool isPlanar = false)
    {
      constexpr int sT = static_cast<int>(sizeof(T));
      if (isPlanar) {
        return Image{
          width, height, channels,              // width, height, channels
          width * sT,  sT, width * height * sT, // strides: row, column, channel
          sT * 8,                               // bit depth
          std::vector<std::byte>(width * height * channels * sT) };
      }
      return Image{
        width, height, channels,                   // width, height, channels
        width * channels * sT,  channels * sT, sT, // strides: row, column, channel
        sT * 8,                                    // bit depth
        std::vector<std::byte>(width * height * channels * sT) };
    }

    template <typename T>
    static Image make(const RectSize& rectSize, int channels = 1, bool isPlanar = false)
    {
      return make<T>(rectSize.width, rectSize.height, channels, isPlanar);
    }

    bool isPlanar() const
    {
      return channels > 1
        && rowStride / colStride == width
        && chanStride / colStride == width * height;
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

  struct YCbCr
  {
    uint8_t y, cb, cr;
  };
  static_assert(sizeof(YCbCr) == 3, "YCbCr size must be 3 bytes");


  //! \brief Round up the division of two integers.
  //! \tparam T An integer type.
  //! \param value The numerator.
  //! \param divisor The denominator.
  //! \return The result of the division rounded up to the nearest integer.
  template <typename T>
  constexpr inline T roundUpDiv(T value, T divisor) {
    return (value + divisor - 1) / divisor;
  }

  class FormatNotSupportedError : public std::runtime_error
  {
  public:
    explicit FormatNotSupportedError(const std::string& format)
      : std::runtime_error("Format not supported: " + format) {}
  };

  template <Tag valueTag, typename ContainerType = std::array<uint8_t, 1>>
  class TagValue
  {
    ContainerType values_;
  public:
    using Container = ContainerType;
    using Type = typename Container::value_type;
    constexpr static Tag tag = valueTag;
    TagValue(Container values) : values_{std::move(values)} {}
    void setRequiredSize(size_t size) {
      if constexpr (requires { values_.resize(size); }) {
        // Resize the container if it supports resizing (e.g., std::vector)
        values_.resize(size);
      }
    }
    size_t size() const { return values_.size(); }
    Type value(size_t index = 0) const { return values_[index]; }
    const Type* data() const { return values_.data(); }
    Type* data() { return values_.data(); }
    bool allEqual(Type v) const {
      return std::all_of(values_.begin(), values_.end(),
                         [v](Type x){ return x == v; });
    }
  };

  class SamplesPerPixel : public TagValue<Tag::SamplesPerPixel>
  {
  public:
    constexpr static Type defaultValue = 1;
    SamplesPerPixel(Type value = defaultValue) : TagValue{{value}}
    {
      if (value < 1) {
        throw std::invalid_argument{"Invalid SamplesPerPixel value"};
      }
    }
  };

  class BitsPerSample
    : public TagValue<Tag::BitsPerSample, std::vector<int>>
  {
  public:
    constexpr static Type defaultValue = 1;
    BitsPerSample(Container values = {{defaultValue}})
      : TagValue{std::move(values)}
    {
      for (const auto& v : values) {
        if (v < 1 || v > 32) {
          throw std::invalid_argument{"Invalid BitsPerSample value"};
        }
      }
    }
  };

  class PlanarConfiguration : public TagValue<Tag::PlanarConfiguration>
  {
  public:
    constexpr static Type Chunky = 1, Planar = 2, defaultValue = Chunky;
    PlanarConfiguration(Type value = defaultValue) : TagValue{{value}}
    {
      if (value != Chunky && value != Planar) {
        throw std::invalid_argument{"Invalid PlanarConfiguration value"};
      }
    }
    bool isChunky() const { return value() == Chunky; }
    bool isPlanar() const { return value() == Planar; }
  };

  class YCbCrPositioning : public TagValue<Tag::YCbCrPositioning>
  {
  public:
    constexpr static Type Centered = 1, Cosited = 2, defaultValue = Centered;
    YCbCrPositioning(Type value = defaultValue) : TagValue{{value}}
    {
      if (value != Centered && value != Cosited) {
        throw std::invalid_argument{"Invalid YCbCrPositioning value"};
      }
    }
    bool isCentered() const { return value() == Centered; }
    bool isCosited() const { return value() == Cosited; }
  };

  class YCbCrCoefficients
    : public TagValue<Tag::YCbCrCoefficients, std::array<float, 3>>
  {
  public:
    constexpr static Container defaultValue{ 0.299f, 0.587f, 0.114f };
    YCbCrCoefficients(Container values = defaultValue)
      : TagValue{std::move(values)}
    {
      for (const auto& v : values) {
        if (v < 0.0f || v > 1.0f) {
          throw std::invalid_argument{"Invalid YCbCrCoefficients value"};
        }
      }
      const Type sum = values[0] + values[1] + values[2];
      if (sum < 0.999f || sum > 1.001f) {
        throw std::invalid_argument{
          "Invalid YCbCrCoefficients value: coefficients must sum to 1"};
      }
    }
    Type red() const { return value(0); }
    Type green() const { return value(1); }
    Type blue() const { return value(2); }
  };

  class YCbCrSubSampling
    : public TagValue<Tag::YCbCrSubSampling, std::array<int, 2>>
  {
  public:
    constexpr static Container defaultValue{ 2, 2 };
    YCbCrSubSampling(Container values = defaultValue)
      : TagValue{std::move(values)}
    {
      for (const auto& v : values) {
        if (v != 1 && v != 2 && v != 4) {
          throw std::invalid_argument{"Invalid YCbCrSubSampling value"};
        }
      }
    }
    Type horizontal() const { return value(0); }
    Type vertical() const { return value(1); }
    bool isSubsampled() const { return horizontal() > 1 || vertical() > 1; }
  };

  template <float R1, float R2, float G1, float G2, float B1, float B2>
  class ReferenceBlackWhite
    : public TagValue<Tag::ReferenceBlackWhite, std::array<float, 6>>
  {
  public:
    constexpr static Container defaultValue{R1, R2, G1, G2, B1, B2 };
    ReferenceBlackWhite(Container values = defaultValue)
      : TagValue{std::move(values)}
    {
      for (size_t i = 0; i < size(); i += 2) {
        if (data()[i] >= data()[i + 1]) {
          throw std::invalid_argument{
            "Invalid ReferenceBlackWhite value: black must be less than white"};
        }
      }
    }
    template <size_t index, Type range> Type map(Type value) const {
      static_assert(index < 3, "Index must be 0, 1, or 2");
      const Type* v = data() + index * 2;
      return (value - v[0]) * range / (v[1] - v[0]);
    }
  };
  using ReferenceBlackWhiteYCbCr = ReferenceBlackWhite<0.0f, 255.0f, 128.0f, 255.0f, 128.0f, 255.0f>;


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
    [[nodiscard]] Image takeImage() { return std::move(image_); }

    // Callback for TiffCraft::load() function
    virtual void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) = 0;

    //! \brief Copies the values from a TIFF entry to a destination buffer of
    //!        the requested type.
    //! \tparam DstType The destination type to convert the values to.
    //! \param[in] entry The TIFF entry containing the values.
    //! \param[out] dst The destination buffer to copy the values to.
    //! \throw std::runtime_error if the source type is rational and any
    //!                           denominator is zero.
    //!
    //! This function is used to convert the values from a TIFF entry to a
    //! destination buffer of the requested type. Values are converted from the
    //! source type to the destination type, possibly loosing precision. The
    //! destination buffer must be large enough to hold the converted values.
    //!
    template <typename DstType>
    static void copy(const TiffImage::IFD::Entry& entry, DstType* dst)
    {
      return dispatchType(entry.type(), [&, entry]<Type type>() {
        using SrcType = TypeTraits_t<type>;
        const auto* values = reinterpret_cast<const SrcType*>(entry.values());
        for (size_t i = 0; i < entry.count(); ++i) {
          if constexpr (is_rational_v<SrcType>) {
            if (values[i].denominator == 0) {
              throw std::runtime_error{
                "Rational value with zero denominator in TIFF entry"};
            }
            const auto numerator = static_cast<DstType>(values[i].numerator);
            const auto denominator = static_cast<DstType>(values[i].denominator);
            dst[i] = numerator / denominator;
          }
          else {
            dst[i] = static_cast<DstType>(values[i]);
          }
        }
      });
    }

    //! \brief Gets a TIFF entry from an IFD by tag.
    //! \param[in] ifd The IFD containing the entry.
    //! \param[in] tag The tag to look for.
    //! \return An optional containing the TIFF entry if found, or std::nullopt
    //!         if not found.
    static std::optional<TiffImage::IFD::Entry> getEntry(
      const TiffImage::IFD& ifd, Tag tag)
    {
      auto it = ifd.entries().find(tag);
      if (it != ifd.entries().end()) {
        return it->second;
      }
      return std::nullopt;
    }

    //! \brief Gets a value from a TIFF entry as a specific type.
    //! \tparam Value The type of the value to get. Must be a TagValue
    //!               specialization.
    //! \param[in] ifd The IFD containing the entry.
    //! \return The value from the TIFF entry, or a default-constructed value if
    //!         the tag is not found.
    template <typename Value>
    static Value get(const TiffImage::IFD& ifd)
    {
      Value value; // create with default values
      auto entry = getEntry(ifd, Value::tag);
      if (entry.has_value()) {
        // Tag found, copy the values to the value object
        value.setRequiredSize(entry->count());
        if (entry->count() != value.size()) {
          throw std::runtime_error{
            "Expected " + std::to_string(value.size()) +
            " values for tag " + std::to_string(static_cast<int>(Value::tag))};
        }
        copy<typename Value::Type>(*entry, value.data());
      }
      return value;
    }

    //! \brief Copies the values from a TIFF entry to a vector of the requested
    //!        type.
    //! \param[in] ifd The IFD containing the entry.
    //! \param[in] tag The tag to look for.
    //! \param[in] defaultValue An optional default value to return if the tag
    //!                         is not found.
    //! \return A vector of integers containing the values from the TIFF entry.
    //! \throw std::runtime_error if the tag is not found and no default value
    //!        is provided, or if the source type is rational and any
    //!        denominator is zero.
    template <typename DstType>
    static std::vector<DstType> getVec(const TiffImage::IFD& ifd, Tag tag,
      std::optional<std::vector<DstType>> defaultValue = std::nullopt)
    {
      auto entry = getEntry(ifd, tag);
      if (!entry.has_value()) {
        // Tag not found, return default value if provided
        if (defaultValue.has_value()) {
          return defaultValue.value();
        }
        throw std::runtime_error{
          "Tag not found: " + std::to_string(static_cast<int>(tag))};
      }
      std::vector<DstType> v(entry->count());
      copy<DstType>(*entry, v.data());
      return v;
    }

    //! \brief Gets one value from a TIFF entry as an integer.
    //! \param[in] ifd The IFD containing the entry.
    //! \param[in] tag The tag to look for.
    //! \param[in] defaultValue An optional default value to return if the tag
    //!                         is not found.
    //! \return The integer value from the TIFF entry, or the default value if
    //!         not found.
    //! \throw std::runtime_error if the tag is not found and no default value
    //!        is provided, or if the entry contains more than one value.
    template <typename DstType>
    static DstType getValue(const TiffImage::IFD& ifd, Tag tag,
      std::optional<DstType> defaultValue = std::nullopt)
    {
      std::optional<std::vector<DstType>> defaultValueVec;
      if (defaultValue.has_value()) {
        defaultValueVec = std::vector<DstType>{ defaultValue.value() };
      }
      auto v = getVec<DstType>(ifd, tag, defaultValueVec);
      if (v.size() != 1) {
        throw std::runtime_error{"Expected a single value for integer tag"};
      }
      return v[0];
    }

    //! \brief Requires that a TIFF entry has a specific value, throwing an
    //!        exception if it does not.
    //! \tparam Comp A comparison functor to compare the actual and required
    //!              values. Defaults to std::equal_to<>.
    //! \param[in] ifd The IFD containing the entry.
    //! \param[in] tag The tag to look for.
    //! \param[in] defaultValue An optional default value to return if the tag
    //!                         is not found.
    //! \param[in] requiredValue The value that is required for the entry.
    //! \param[in] comp A comparison functor to compare the actual and required
    //!                 values. Defaults to std::equal_to<>.
    //! \return The actual value of the entry if it matches the required value.
    //! \throw FormatNotSupportedError if the actual value does not match the
    //!        required value.
    template <typename Comp = std::equal_to<>>
    static int require(
      const TiffImage::IFD& ifd, Tag tag, std::optional<int> defaultValue,
      int requiredValue, Comp&& comp = {})
    {
      const int value = getValue<int>(ifd, tag, defaultValue);
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
      auto bitsPerSample = getVec<int>(ifd, Tag::BitsPerSample);
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
      return getValue<int>(ifd, Tag::ImageWidth);;
    }

    static int getHeight(const TiffImage::IFD& ifd)
    {
      return getValue<int>(ifd, Tag::ImageLength);
    }

    static std::vector<int> getBitsPerSample(const TiffImage::IFD& ifd)
    {
      if (ifd.entries().contains(Tag::BitsPerSample)) {
        return getVec<int>(ifd, Tag::BitsPerSample);
      }
      return { 1 }; // default value if missing
    }

    static RectSize getRectSize(const TiffImage::IFD& ifd)
    {
      const int imageWidth = getWidth(ifd);
      const int imageHeight = getHeight(ifd);

      const int rowsPerStrip =
        std::min(getValue<int>(ifd, Tag::RowsPerStrip, imageHeight), imageHeight);

      const int tileWidth = getValue<int>(ifd, Tag::TileWidth, imageWidth);
      const int tileHeight = getValue<int>(ifd, Tag::TileLength, rowsPerStrip);

      return { tileWidth, tileHeight };
    }

    template <typename SrcType, typename DstType, typename UnaryOp = std::nullptr_t>
    static Image makeImage(
      const TiffImage::IFD& ifd,              // source IFD
      const TiffImage::ImageData& imageData,  // source pixel data
      int dstSamplesPerPixel,                 // destination image channels
      bool equalsHostByteOrder,               // source data byte order
      UnaryOp unaryOp = nullptr)
    {
      // Create a destination image with the information from the source IFD
      const int imageWidth = getWidth(ifd);
      const int imageHeight =  getHeight(ifd);
      const int samplesPerPixel = get<SamplesPerPixel>(ifd).value();
      const bool isPlanar = get<PlanarConfiguration>(ifd).isPlanar();
      Image image = Image::make<DstType>(imageWidth, imageHeight,
                                         dstSamplesPerPixel, isPlanar);

      // Get the rectangle size from the source IFD
      const auto rectSize = getRectSize(ifd);

      // Copy the image data
      std::vector<Image> images{ std::move(image) };
      copyImageData<SrcType, DstType, UnaryOp>(
        images, ifd, imageData, rectSize, equalsHostByteOrder, samplesPerPixel,
        std::move(unaryOp));

      // Return the filled image
      return std::move(images.front());
    }

    template <typename SrcType, typename DstType, typename UnaryOp = std::nullptr_t>
    static void copyImageData(
      std::vector<Image>& images,             // destination images
      const TiffImage::IFD& ifd,              // source IFD
      const TiffImage::ImageData& imageData,  // source pixel data
      const RectSize& rectSize,               // source rectangle size
      bool equalsHostByteOrder,               // source data byte order
      int srcSamplesPerPixel,                 // source image channels
      UnaryOp unaryOp = nullptr)
    {
      if (images.empty()) {
        throw std::runtime_error("No destination images provided");
      }

      // We take the first image as a reference for size calculations
      const Image& refImg = images.front();
      const int width = refImg.width;
      const int height = refImg.height;

      // Process as planar if there is more than one image or if the first image
      // is planar. The convention is that a single image is given, which it
      // could be planar or chunky, or if more than one image is given then each
      // image is a separate plane of a planar image.
      const bool isPlanar = images.size() > 1 || refImg.isPlanar();
      const int planes = isPlanar ? images.size() > 1
        ? static_cast<int>(images.size()) : refImg.channels : 1;

      // Determine the number of channels in the source image
      const int channels = isPlanar ? 1 : srcSamplesPerPixel;

      // Rectangle number and dimensions
      const int rectAcross = roundUpDiv(width, rectSize.width);
      const int rectDown = roundUpDiv(height, rectSize.height);
      const int rectsInPlane = rectAcross * rectDown;
      const int rectsInImage = rectAcross * rectDown * planes;
      if (rectsInImage != imageData.size()) {
        throw std::invalid_argument{
          "Mismatch between expected number of rectangles and image data"};
      }
      auto getRect = [&](int plane, int rectY, int rectX) {
        return imageData[plane * rectsInPlane + rectY * rectAcross + rectX];
      };

      // Calculate the maximum source and destination values
      const auto bitsPerSample = get<BitsPerSample>(ifd).value();
      constexpr size_t maxDstValue = std::numeric_limits<DstType>::max();
      const size_t maxSrcValue = bitsPerSample < sizeof(size_t) * 8
        ? (size_t(1) << bitsPerSample) - 1 : std::numeric_limits<size_t>::max();

      // TODO: if DstType is small create a lookup table for the conversion to
      //       avoid repeated division

      // Operator to map from the source value range to the destination value
      // range, or apply a user-provided unary
      auto op = [&](DstType value) {
        if constexpr (std::is_same_v<UnaryOp, std::nullptr_t>) {
          // Scale the value to the destination range
          return static_cast<DstType>((value * maxDstValue) / maxSrcValue);
        } else {
          // Apply the user-provided unary operation
          return std::invoke(unaryOp, value);
        }
      };
      using OpType = decltype(op);

      for (int plane = 0; plane < planes; ++plane) {
        Image& image = images.size() > 1 ? images[plane] : images.front();
        const int dstPlane = images.size() > 1 ? 0 : plane;

        const int subsampleX = roundUpDiv(width, image.width);
        const int subsampleY = roundUpDiv(height, image.height);
        const int planeWidth = roundUpDiv(rectSize.width, subsampleX);
        const int planeHeight = roundUpDiv(rectSize.height, subsampleY);
        const int planeStride = getRect(plane, 0, 0).size() / planeHeight;

        RectSize currRectSize;
        for (int rectY = 0; rectY < rectDown; ++rectY) {
          const int dstRow = rectY * planeHeight;
          currRectSize.height = std::min(planeHeight, image.height - dstRow);
          for (int rectX = 0; rectX < rectAcross; ++rectX) {
            const int dstCol = rectX * planeWidth;
            currRectSize.width = std::min(planeWidth, image.width - dstCol);
            const auto& rectData = getRect(plane, rectY, rectX);
            copyRectangle<SrcType, DstType, OpType>(
              rectData,                    // source pixel data
              currRectSize,                // source rectangle size
              channels,                    // source image channels
              planeStride,                 // source image stride
              bitsPerSample,               // source image bits per sample
              equalsHostByteOrder,         // source data byte order
              image,                       // destination image
              dstPlane,                    // destination plane
              dstCol,                      // destination column
              dstRow,                      // destination row
              std::forward<OpType>(op));
          }
        }
      }
    }

    template <typename SrcType, typename DstType, typename UnaryOp>
    static void copyRectangle(
      const std::vector<std::byte>& rectData, // source pixel data
      const RectSize& rectSize,               // source rectangle size
      size_t channels,                        // source image channels
      size_t stride,                          // source image stride
      size_t bitsPerSample,                   // source image bits per sample
      bool equalsHostByteOrder,               // source data byte order
      Image& image,                           // destination image
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

      const size_t dstRowStride = image.rowStride / sizeof(ValueType);
      const size_t dstColStride = image.colStride / sizeof(ValueType);
      const size_t dstChanStride = image.chanStride / sizeof(ValueType);
      const size_t dstPlaneStride = dstRowStride * image.height;
      auto* dst = image.dataPtr<ValueType>()
        + dstPlane * dstPlaneStride
        + dstY * dstRowStride
        + dstX * dstColStride;

      const auto* srcRow = src;
      auto* dstRow = dst;

      auto loop = [&](auto processor) {
        for (int row = 0; row < rectSize.height; ++row) {
          if (src >= srcEnd) {
            // We've reached the end of the source tile
            throw std::runtime_error("Unexpected end of source tile");
          }
          srcRow = src;
          dstRow = dst;
          for (int col = 0; col < rectSize.width; ++col) {
            auto* dstChan = dstRow;
            for (int chan = 0; chan < channels; ++chan) {
              DstType value = processor();
              *dstChan = std::invoke(op, value);
              dstChan += dstChanStride;
            }
            dstRow += dstColStride;
          }
          src += stride;
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
      requireSamplesPerPixel(ifd, 1);
      requireCompression(ifd, 1);
      requireFillOrder(ifd, 1);

      const int photometricInterpretation =
        requirePhotometricInterpretation(ifd, 1, std::less_equal<>());

      // create the image and copy the pixel data
      constexpr int dstSamplesPerPixel = 1;
      image_ = makeImage<SrcType,DstType>(
        ifd,                            // source IFD
        imageData,                      // source pixel data
        dstSamplesPerPixel,             // destination image channels
        header.equalsHostByteOrder()    // source data byte order
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
      requireSamplesPerPixel(ifd, 1);
      requirePhotometricInterpretation(ifd, 3);
      requireCompression(ifd, 1);
      requireFillOrder(ifd, 1);

      // Get the color map
      const int bitsPerSample = get<BitsPerSample>(ifd).value();
      const auto numColors = (1 << bitsPerSample);
      const auto colorMap = getVec<int>(ifd, Tag::ColorMap);
      if (3 * numColors > colorMap.size()) {
        throw std::runtime_error("Color map size does not match bits per sample");
      }
      const int chan[3] = { numColors * 0, numColors * 1, numColors * 2 };

      // create the image and copy the pixel data
      constexpr int dstSamplesPerPixel = 3;
      using UnaryOp = std::function<Rgb<DstType>(DstType)>;
      image_ = makeImage<SrcType,DstType, UnaryOp>(
        ifd,                            // source IFD
        imageData,                      // source pixel data
        dstSamplesPerPixel,             // destination image channels
        header.equalsHostByteOrder(),   // source data byte order
        [&](DstType value) -> Rgb<DstType> {
          constexpr int bitsPerDstPixel = 8 * sizeof(DstType);
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
      requireSamplesPerPixel(ifd, 3);
      requirePhotometricInterpretation(ifd, 2);
      requireCompression(ifd, 1);

      const auto bitsPerSample = get<BitsPerSample>(ifd);
      if (bitsPerSample.size() != 3
        || !bitsPerSample.allEqual(bitsPerSample.value(0))) {
        throw std::runtime_error{
          "Expected 3 bits per sample for RGB image with equal values"};
      }

      // create the image and copy the pixel data
      constexpr int dstSamplesPerPixel = 3;
      image_ = makeImage<SrcType,DstType>(
        ifd,                            // source IFD
        imageData,                      // source pixel data
        dstSamplesPerPixel,             // destination image channels
        header.equalsHostByteOrder()    // source data byte order
      );
    }
  };

  // TiffExporter implementation for YCbCr images ----------------------------
  template <typename DstType, typename SrcType = DstType>
  class TiffExporterYCbCr : public TiffExporter
  {
    static_assert(std::is_same_v<SrcType, uint8_t> && std::is_same_v<DstType, uint8_t>,
      "YCbCr images must use 8-bit source and destination type");
  public:
    // Callback for TiffCraft::load() function
    void operator()(
      const TiffImage::Header& header,
      const TiffImage::IFD& ifd,
      TiffImage::ImageData imageData) override
    {
      requireSamplesPerPixel(ifd, 3);
      requireBitsPerSample(ifd, { 8, 8, 8 });
      requireCompression(ifd, 1);
      requirePhotometricInterpretation(ifd, 6);

      const auto yCbCrCoefficients = get<YCbCrCoefficients>(ifd);
      const auto referenceBlackWhiteYCbCr = get<ReferenceBlackWhiteYCbCr>(ifd);

      const auto yCbCrSubsampling = get<YCbCrSubSampling>(ifd);
      const bool isPlanar = get<PlanarConfiguration>(ifd).isPlanar();

      // We will ignore the YCbCrPositioning tag
      // const bool isCentered = get<YCbCrPositioning>(ifd).isCentered();
      // if (!isCentered) {
      //   throw std::runtime_error{"YCbCrPositioning must be Centered"};
      // }

      const int subsampleX = yCbCrSubsampling.horizontal();
      const int subsampleY = yCbCrSubsampling.vertical();
      const RectSize imageRect{
        getWidth(ifd),
        getHeight(ifd)
      };
      const RectSize subsampledRect{
        roundUpDiv(imageRect.width, subsampleX),
        roundUpDiv(imageRect.height, subsampleY)
      };

      const RectSize rectSize = getRectSize(ifd);
      const RectSize subsampledRectSize{
        roundUpDiv(rectSize.width, subsampleX),
        roundUpDiv(rectSize.height, subsampleY)
      };

      int channels;
      RectSize copyRectSize;
      std::vector<Image> images;

      if (isPlanar)
      { // Planar images: use 3 separate images for Y, Cb, and Cr planes
        channels = 1;
        images = {
          Image::make<DstType>(imageRect),
          Image::make<DstType>(subsampledRect),
          Image::make<DstType>(subsampledRect),
        };
        copyRectSize = rectSize;
      }
      else
      { // Chunky images: use a subsampled image with YCbCr blocks as pixels
        channels = subsampleX * subsampleY + 2; // Y + Cb + Cr
        images = { Image::make<DstType>(subsampledRect, channels) };
        copyRectSize = subsampledRectSize;
      }

      // Copy the image data to the intermediate image
      copyImageData<SrcType, DstType>(
        images,                        // destination images
        ifd,                           // source IFD
        imageData,                     // source pixel data
        copyRectSize,                  // source rectangle size
        header.equalsHostByteOrder(),  // source data byte order
        channels                       // source image channels
      );

      // Convert YCbCr to RGB
      if (isPlanar)
      {
        auto makeYCbCr =
          [&](const std::byte** rows, const Vec2i* indices,
              [[maybe_unused]] int w, [[maybe_unused]] int h) -> YCbCr {
            return {
              *reinterpret_cast<const uint8_t*>(rows[0] + indices[0].x),
              *reinterpret_cast<const uint8_t*>(rows[1] + indices[1].x),
              *reinterpret_cast<const uint8_t*>(rows[2] + indices[2].x),
            };
          };
        image_ = Image::make<DstType>(imageRect, 3);
        convertYCbCrToRgb_(
          image_, images, yCbCrCoefficients, referenceBlackWhiteYCbCr, makeYCbCr);
      }
      else
      {
        Image* dstImage = images.data();

        if (yCbCrSubsampling.isSubsampled()) {
          image_ = Image::make<DstType>(imageRect, 3);
          dstImage = &image_;
        }

        const int blockSize = images.front().colStride;
        const int cbOffset = blockSize - 2;
        const int crOffset = blockSize - 1;
        auto makeYCbCr =
          [&](const std::byte** rows, const Vec2i* indices, int w, int h) -> YCbCr {
            const std::byte* block = rows[0] + indices[0].x * blockSize;
            const int row = h % subsampleY, col = w % subsampleX;
            return {
              *reinterpret_cast<const uint8_t*>(block + (row * subsampleX + col)),
              *reinterpret_cast<const uint8_t*>(block + cbOffset),
              *reinterpret_cast<const uint8_t*>(block + crOffset)
            };
          };

        convertYCbCrToRgb_(
          *dstImage, images, yCbCrCoefficients, referenceBlackWhiteYCbCr, makeYCbCr);

        if (&image_ != dstImage) {
          image_ = std::move(*dstImage);
        }
      }
    }
  private:
    struct LUT {
      std::vector<int32_t> yLUT, cbbLUT, crrLUT, cbgLUT, crgLUT;

      constexpr static int BitShift = 16;
      constexpr static int32_t toFixed(float x) {
        return static_cast<int32_t>(x * (1 << BitShift) + 0.5f);
      }
      constexpr static uint8_t fromFixed(int32_t x) {
        return static_cast<uint8_t>(std::clamp(x >> BitShift, 0, 255));
      };

      Rgb8 toRGB(YCbCr pixel) const {
        return Rgb8{
          fromFixed(yLUT[pixel.y] + crrLUT[pixel.cr]),                    // r
          fromFixed(yLUT[pixel.y] + cbgLUT[pixel.cb] + crgLUT[pixel.cr]), // g
          fromFixed(yLUT[pixel.y] + cbbLUT[pixel.cb])                     // b
        };
      }
    };

    static LUT computeConversionTables_(const YCbCrCoefficients& luma,
                                        const ReferenceBlackWhiteYCbCr& refBlackWhite)
    {
      // Conversion formulas
      // ===================
      // R = Cr * (2-2*Lr) + Y
      // B = Cb * (2-2*Lb) + Y
      // G = (Y - Lb*B - Lr*R)/Lg
      // Lr + Lg + Lb = 1
      //
      // Lr' = 2-2*Lr
      // Lb' = 2-2*Lb
      //
      // R = Y + Cr*Lr'
      // B = Y + Cb*Lb'
      // G = (Y - Lb*(Y + Cb*Lb') - Lr*(Y + Cr*Lr'))/Lg
      //
      // G = (Y - Lb*Y - Cb*Lb*Lb' - Lr*Y - Cr*Lr*Lr')/Lg
      // G = Y * (1-Lb-Lr)/Lg - Cb*Lb*Lb'/Lg  - Cr*Lr*Lr'/Lg
      //
      // Lg = 1-Lb-Lr
      // G = Y + (-Lb*Lb'/Lg)*Cb + (-Lr*Lr'/Lg)*Cr
      //
      // --------------------------------
      // R = yLUT[y] + crrLUT[cr]
      // B = yLUT[y] + cbbLUT[cb]
      // G = yLUT[y] + cbgLUT[cb] + crgLUT[cr]

      // Create a lookup table for faster YCbCr to RGB conversion
      const float Lr = luma.red();
      const float Lg = luma.green();
      const float Lb = luma.blue();
      const float Lrr = 2 - 2*Lr;
      const float Lbb = 2 - 2*Lb;
      const float Lbg = -Lb*Lbb/Lg;
      const float Lrg = -Lr*Lrr/Lg;

      std::vector<int32_t> yLUT(256), cbbLUT(256), crrLUT(256);
      std::vector<int32_t> cbgLUT(256), crgLUT(256);
      for (int i = 0; i < 256; ++i) {
        const uint8_t i8 = static_cast<uint8_t>(i & 0xff);
        const int32_t y = refBlackWhite.map<0, 255.0f>(i8);
        const int32_t cb = refBlackWhite.map<1, 127.0f>(i8);
        const int32_t cr = refBlackWhite.map<2, 127.0f>(i8);
        yLUT[i] = LUT::toFixed(y);
        cbbLUT[i] = LUT::toFixed(cb * Lbb);
        crrLUT[i] = LUT::toFixed(cr * Lrr);
        cbgLUT[i] = LUT::toFixed(cb * Lbg);
        crgLUT[i] = LUT::toFixed(cr * Lrg);
      }

      return LUT{std::move(yLUT), std::move(cbbLUT), std::move(crrLUT),
                 std::move(cbgLUT), std::move(crgLUT)};
    }

    template <typename MakeYCbCr>
    static void convertYCbCrToRgb_(
      Image& dstImage,
      const std::vector<Image>& srcImages,
      const YCbCrCoefficients& luma,
      const ReferenceBlackWhiteYCbCr& refBlackWhite,
      MakeYCbCr makeYCbCr)
    {
      if (srcImages.empty()) {
        throw std::invalid_argument{
          "YCbCr to RGB conversion requires at least 1 source image"};
      }

      auto channel = [&](std::byte* pixel, size_t index) -> uint8_t& {
        return *reinterpret_cast<uint8_t*>(pixel + index * dstImage.chanStride);
      };

      const LUT lut = computeConversionTables_(luma, refBlackWhite);

      std::vector<const std::byte*> srcPlanes( srcImages.size() );
      std::vector<Vec2i> subsample( srcImages.size() );
      for (size_t i = 0; i < srcImages.size(); ++i) {
        srcPlanes[i] = srcImages[i].dataPtr<std::byte>();
        subsample[i] = { roundUpDiv(dstImage.width, srcImages[i].width),
                         roundUpDiv(dstImage.height, srcImages[i].height) };
      }

      std::vector<const std::byte*> srcRows( srcPlanes.size() );
      std::vector<Vec2i> index( srcPlanes.size() );

      std::byte* pixels = dstImage.dataPtr<std::byte>();
      for (size_t h = 0; h < dstImage.height; ++h) {
        for (size_t i = 0; i < srcImages.size(); ++i) {
          const size_t hi = h / subsample[i].y;
          srcRows[i] = srcPlanes[i] + hi * srcImages[i].rowStride;
          index[i].y = hi;
        }
        std::byte* row = pixels + h * dstImage.rowStride;
        for (size_t w = 0; w < dstImage.width; ++w) {
          for (size_t i = 0; i < srcImages.size(); ++i) {
            index[i].x = w / subsample[i].x;
          }
          const Rgb8 rgb = lut.toRGB(makeYCbCr(srcRows.data(), index.data(), w, h));
          std::byte* pixelPtr = row + w * dstImage.colStride;
          channel(pixelPtr, 0) = rgb.r;
          channel(pixelPtr, 1) = rgb.g;
          channel(pixelPtr, 2) = rgb.b;
        }
      }
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
      exported_ = false;
      const int photometricInterpretation = getValue<int>(ifd, Tag::PhotometricInterpretation, 1);

      if (photometricInterpretation >= 0 && photometricInterpretation <= 1) {
        // grayscale image types
        const int bitsPerSample = getValue<int>(ifd, Tag::BitsPerSample, 1);
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
        std::vector<int> bitsPerSampleVec = getVec<int>(ifd, Tag::BitsPerSample);
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
        const int bitsPerSample = getValue<int>(ifd, Tag::BitsPerSample, 1);
        if (bitsPerSample <= 8) {
          tryToExport<TiffExporterPalette<uint8_t>>(header, ifd, imageData);
        } else if (bitsPerSample <= 16) {
          tryToExport<TiffExporterPalette<uint16_t>>(header, ifd, imageData);
        }
      }
      else if (photometricInterpretation == 6) {
        // YCbCr image types
        tryToExport<TiffExporterYCbCr<uint8_t>>(header, ifd, imageData);
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
