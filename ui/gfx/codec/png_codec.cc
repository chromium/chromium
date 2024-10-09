// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/png_codec.h"

#include <stdint.h>

#include <optional>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "skia/buildflags.h"
#include "skia/rusty_png_feature.h"
#include "third_party/skia/include/codec/SkPngDecoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "third_party/zlib/zlib.h"
#include "ui/gfx/codec/vector_wstream.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(SKIA_BUILD_RUST_PNG)
#include "third_party/skia/experimental/rust_png/SkPngRustDecoder.h"
#endif

namespace gfx {

PNGCodec::DecodeOutput::DecodeOutput() = default;
PNGCodec::DecodeOutput::~DecodeOutput() = default;
PNGCodec::DecodeOutput::DecodeOutput(const PNGCodec::DecodeOutput& other) =
    default;
PNGCodec::DecodeOutput& PNGCodec::DecodeOutput::operator=(
    const PNGCodec::DecodeOutput& other) = default;

// Decoder --------------------------------------------------------------------

namespace {

std::unique_ptr<SkCodec> CreatePngDecoder(std::unique_ptr<SkStream> stream,
                                          SkCodec::Result* result) {
  if (skia::IsRustyPngEnabled()) {
#if BUILDFLAG(SKIA_BUILD_RUST_PNG)
    return SkPngRustDecoder::Decode(std::move(stream), result);
#else
    // The `if` condition guarantees `SKIA_BUILD_RUST_PNG`.
    NOTREACHED_NORETURN();
#endif
  }

  return SkPngDecoder::Decode(std::move(stream), result);
}

struct PreparationOutput {
  std::unique_ptr<SkCodec> codec;
  SkImageInfo image_info;
};

std::optional<PreparationOutput> PrepareForPNGDecode(
    base::span<const uint8_t> input,
    PNGCodec::ColorFormat format) {
  PreparationOutput output;

  // Parse the input stream with the PNG decoder, yielding a SkCodec.
  auto stream = std::make_unique<SkMemoryStream>(input.data(), input.size(),
                                                 /*copyData=*/false);
  SkCodec::Result result;
  output.codec = CreatePngDecoder(std::move(stream), &result);
  if (!output.codec || result != SkCodec::kSuccess) {
    return std::nullopt;
  }

  // Reject images that would exceed INT_MAX bytes.
  SkISize size = output.codec->dimensions();
  constexpr int kBytesPerPixel = 4;
  if (size.area() >= (INT_MAX / kBytesPerPixel)) {
    return std::nullopt;
  }

  // Create an SkImageInfo matching the PNG's, but with our desired format.
  SkImageInfo codec_info = output.codec->getInfo();
  SkAlphaType alpha_type = codec_info.alphaType();
  SkColorType color_type;
  switch (format) {
    case PNGCodec::FORMAT_RGBA:
      color_type = kRGBA_8888_SkColorType;
      break;
    case PNGCodec::FORMAT_BGRA:
      color_type = kBGRA_8888_SkColorType;
      break;
    case PNGCodec::FORMAT_SkBitmap:
      color_type = kN32_SkColorType;
      if (alpha_type == kUnpremul_SkAlphaType) {
        alpha_type = kPremul_SkAlphaType;
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid color format " << format;
      return std::nullopt;
  }
  output.image_info =
      SkImageInfo::Make(codec_info.width(), codec_info.height(), color_type,
                        alpha_type, codec_info.refColorSpace());

  return output;
}

}  // namespace

// static
std::optional<PNGCodec::DecodeOutput> PNGCodec::Decode(
    base::span<const uint8_t> input,
    ColorFormat format) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("ImageDecoder.Png.UiGfxIntoVector");

  std::optional<PreparationOutput> preparation_output =
      PrepareForPNGDecode(input, format);
  if (!preparation_output) {
    return std::nullopt;
  }

  DecodeOutput output;
  output.width = preparation_output->image_info.width();
  output.height = preparation_output->image_info.height();

  // Always decode into vanilla sRGB, because the output array is a bag of RGBA
  // pixels and doesn't have an associated colorspace.
  SkImageInfo info_srgb =
      preparation_output->image_info.makeColorSpace(SkColorSpace::MakeSRGB());

  // Decode the pixels into the `output` vector.
  output.output.resize(info_srgb.computeMinByteSize());
  SkCodec::Result result = preparation_output->codec->getPixels(
      info_srgb, output.output.data(),
      preparation_output->image_info.minRowBytes());
  if (result == SkCodec::kSuccess) {
    return output;
  } else {
    return std::nullopt;
  }
}

SkBitmap PNGCodec::Decode(base::span<const uint8_t> input) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("ImageDecoder.Png.UiGfxIntoSkBitmap");

  std::optional<PreparationOutput> preparation_output =
      PrepareForPNGDecode(input, FORMAT_SkBitmap);
  if (!preparation_output) {
    return SkBitmap();
  }

  // The image alpha type is likely to be "unpremultiplied," as this is set by
  // the PNG standard. However, Skia prefers premultiplied bitmaps. We update
  // the image-info struct to specify premultiplication; the SkCodec will
  // automatically fix up the pixels as it runs.
  SkAlphaType alpha = preparation_output->image_info.alphaType();
  if (alpha == kUnpremul_SkAlphaType) {
    preparation_output->image_info =
        preparation_output->image_info.makeAlphaType(kPremul_SkAlphaType);
  }

  // Decode the image pixels directly into the SkBitmap. Alpha premultiplication
  // will be performed by `getPixels`. No colorspace conversion will occur,
  // because the bitmap uses the same colorspace as the original PNG.
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(preparation_output->image_info)) {
    return SkBitmap();
  }

  SkCodec::Result result =
      preparation_output->codec->getPixels(bitmap.pixmap());
  if (result == SkCodec::kSuccess) {
    return bitmap;
  } else {
    return SkBitmap();
  }
}

// DEPRECATED
bool PNGCodec::Decode(const unsigned char* input,
                      size_t input_size,
                      SkBitmap* bitmap) {
  SkBitmap result = Decode(UNSAFE_TODO(base::span(input, input_size)));
  if (result.isNull()) {
    return false;
  }

  *bitmap = std::move(result);
  return true;
}

// Encoder --------------------------------------------------------------------

namespace {

void AddComments(SkPngEncoder::Options& options,
                 const std::vector<PNGCodec::Comment>& comments) {
  std::vector<const char*> comment_pointers;
  std::vector<size_t> comment_sizes;
  for (const auto& comment : comments) {
    comment_pointers.push_back(comment.key.c_str());
    comment_pointers.push_back(comment.text.c_str());
    comment_sizes.push_back(comment.key.length() + 1);
    comment_sizes.push_back(comment.text.length() + 1);
  }
  options.fComments = SkDataTable::MakeCopyArrays(
      (void const* const*)comment_pointers.data(), comment_sizes.data(),
      static_cast<int>(comment_pointers.size()));
}

std::optional<std::vector<uint8_t>> EncodeSkPixmap(
    const SkPixmap& src,
    const std::vector<PNGCodec::Comment>& comments,
    int zlib_level,
    bool disable_filters) {
  std::vector<uint8_t> output;
  VectorWStream dst(&output);

  SkPngEncoder::Options options;
  AddComments(options, comments);
  options.fZLibLevel = zlib_level;
  if (disable_filters) {
    options.fFilterFlags = SkPngEncoder::FilterFlag::kNone;
  }

  if (!SkPngEncoder::Encode(&dst, src, options)) {
    return std::nullopt;
  }

  return output;
}

std::optional<std::vector<uint8_t>> EncodeSkPixmap(
    const SkPixmap& src,
    bool discard_transparency,
    const std::vector<PNGCodec::Comment>& comments,
    int zlib_level,
    bool disable_filters) {
  if (discard_transparency) {
    SkImageInfo opaque_info = src.info().makeAlphaType(kOpaque_SkAlphaType);
    SkBitmap copy;
    if (!copy.tryAllocPixels(opaque_info)) {
      return std::nullopt;
    }
    SkPixmap opaque_pixmap;
    bool success = copy.peekPixels(&opaque_pixmap);
    DCHECK(success);
    // The following step does the unpremul as we set the dst alpha type to be
    // kUnpremul_SkAlphaType. Later, because opaque_pixmap has
    // kOpaque_SkAlphaType, we'll discard the transparency as required.
    success =
        src.readPixels(opaque_info.makeAlphaType(kUnpremul_SkAlphaType),
                       opaque_pixmap.writable_addr(), opaque_pixmap.rowBytes());
    DCHECK(success);
    return EncodeSkPixmap(opaque_pixmap, comments, zlib_level, disable_filters);
  }

  // If the image's pixels are all opaque, encode the PNG as opaque, regardless
  // of the pixmap's alphaType.
  if (src.info().alphaType() != kOpaque_SkAlphaType && src.computeIsOpaque()) {
    SkPixmap opaque_pixmap{src.info().makeAlphaType(kOpaque_SkAlphaType),
                           src.addr(), src.rowBytes()};
    return EncodeSkPixmap(opaque_pixmap, comments, zlib_level, disable_filters);
  }

  // Encode the PNG without any conversions.
  return EncodeSkPixmap(src, comments, zlib_level, disable_filters);
}

std::optional<std::vector<uint8_t>> EncodeSkBitmap(const SkBitmap& input,
                                                   bool discard_transparency,
                                                   int zlib_level,
                                                   bool disable_filters) {
  SkPixmap src;
  if (!input.peekPixels(&src)) {
    return std::nullopt;
  }
  return EncodeSkPixmap(src, discard_transparency,
                        std::vector<PNGCodec::Comment>(), zlib_level,
                        disable_filters);
}

}  // namespace

std::optional<std::vector<uint8_t>> PNGCodec::Encode(
    const unsigned char* input,
    ColorFormat format,
    const Size& size,
    int row_byte_width,
    bool discard_transparency,
    const std::vector<Comment>& comments) {
  // Initialization required for Windows although the switch covers all cases.
  SkColorType colorType = kN32_SkColorType;
  switch (format) {
    case FORMAT_RGBA:
      colorType = kRGBA_8888_SkColorType;
      break;
    case FORMAT_BGRA:
      colorType = kBGRA_8888_SkColorType;
      break;
    case FORMAT_SkBitmap:
      colorType = kN32_SkColorType;
      break;
  }
  auto alphaType =
      format == FORMAT_SkBitmap ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;
  SkImageInfo info =
      SkImageInfo::Make(size.width(), size.height(), colorType, alphaType);
  SkPixmap src(info, input, row_byte_width);
  return EncodeSkPixmap(src, discard_transparency, comments,
                        DEFAULT_ZLIB_COMPRESSION, /*disable_filters=*/false);
}

// DEPRECATED
bool PNGCodec::Encode(const unsigned char* input,
                      ColorFormat format,
                      const Size& size,
                      int row_byte_width,
                      bool discard_transparency,
                      const std::vector<Comment>& comments,
                      std::vector<unsigned char>* output) {
  std::optional<std::vector<uint8_t>> result = Encode(
      input, format, size, row_byte_width, discard_transparency, comments);

  if (!result) {
    output->clear();
    return false;
  }

  *output = std::move(*result);
  return true;
}

std::optional<std::vector<uint8_t>> PNGCodec::EncodeBGRASkBitmap(
    const SkBitmap& input,
    bool discard_transparency) {
  return EncodeSkBitmap(input, discard_transparency, DEFAULT_ZLIB_COMPRESSION,
                        /*disable_filters=*/false);
}

// DEPRECATED
bool PNGCodec::EncodeBGRASkBitmap(const SkBitmap& input,
                                  bool discard_transparency,
                                  std::vector<unsigned char>* output) {
  std::optional<std::vector<uint8_t>> result =
      EncodeSkBitmap(input, discard_transparency, DEFAULT_ZLIB_COMPRESSION,
                     /*disable_filters=*/false);

  if (!result) {
    output->clear();
    return false;
  }

  *output = std::move(*result);
  return true;
}

std::optional<std::vector<uint8_t>> PNGCodec::FastEncodeBGRASkBitmap(
    const SkBitmap& input,
    bool discard_transparency) {
  return EncodeSkBitmap(input, discard_transparency, Z_BEST_SPEED,
                        /*disable_filters=*/true);
}

// DEPRECATED
bool PNGCodec::FastEncodeBGRASkBitmap(const SkBitmap& input,
                                      bool discard_transparency,
                                      std::vector<unsigned char>* output) {
  std::optional<std::vector<uint8_t>> result =
      EncodeSkBitmap(input, discard_transparency, Z_BEST_SPEED,
                     /*disable_filters=*/true);

  if (!result) {
    output->clear();
    return false;
  }

  *output = std::move(*result);
  return true;
}

PNGCodec::Comment::Comment(const std::string& k, const std::string& t)
    : key(k), text(t) {}

PNGCodec::Comment::~Comment() = default;

}  // namespace gfx
