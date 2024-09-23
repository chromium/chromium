// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/png_codec.h"

#include <stdint.h>

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

bool PrepareForPNGDecode(const unsigned char* input,
                         size_t input_size,
                         PNGCodec::ColorFormat format,
                         std::unique_ptr<SkCodec>* codec,
                         SkImageInfo* image_info) {
  // Parse the input stream with the PNG decoder, yielding a SkCodec.
  auto stream = std::make_unique<SkMemoryStream>(input, input_size,
                                                 /*copyData=*/false);
  SkCodec::Result result;
  *codec = CreatePngDecoder(std::move(stream), &result);
  if (!*codec || result != SkCodec::kSuccess) {
    return false;
  }

  // Create an SkImageInfo matching the PNG's, but with our desired format.
  SkImageInfo codec_info = (*codec)->getInfo();
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
      return false;
  }
  *image_info =
      SkImageInfo::Make(codec_info.width(), codec_info.height(), color_type,
                        alpha_type, codec_info.refColorSpace());

  // Reject images that would exceed INT_MAX bytes.
  constexpr int kBytesPerPixel = 4;
  return image_info->dimensions().area() < (INT_MAX / kBytesPerPixel);
}

}  // namespace

// static
bool PNGCodec::Decode(const unsigned char* input,
                      size_t input_size,
                      ColorFormat format,
                      std::vector<unsigned char>* output,
                      int* w,
                      int* h) {
  DCHECK(output);
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("ImageDecoder.Png.UiGfxIntoVector");

  std::unique_ptr<SkCodec> codec;
  SkImageInfo info;
  bool ok = PrepareForPNGDecode(input, input_size, format, &codec, &info);

  *w = info.width();
  *h = info.height();
  if (!ok) {
    return false;
  }

  // Always decode into vanilla sRGB, because the output array is a bag of RGBA
  // pixels and doesn't have an associated colorspace.
  SkImageInfo info_srgb = info.makeColorSpace(SkColorSpace::MakeSRGB());

  // Decode the pixels into the `output` vector.
  output->resize(info_srgb.computeMinByteSize());
  SkCodec::Result result =
      codec->getPixels(info_srgb, &output->front(), info.minRowBytes());
  return result == SkCodec::kSuccess;
}

// static
bool PNGCodec::Decode(const unsigned char* input,
                      size_t input_size,
                      SkBitmap* bitmap) {
  DCHECK(bitmap);
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("ImageDecoder.Png.UiGfxIntoSkBitmap");

  std::unique_ptr<SkCodec> codec;
  SkImageInfo info;
  if (!PrepareForPNGDecode(input, input_size, FORMAT_SkBitmap, &codec, &info)) {
    return false;
  }

  // The image alpha type is likely to be "unpremultiplied," as this is set by
  // the PNG standard. However, Skia prefers premultiplied bitmaps. We update
  // the image-info struct to specify premultiplication; the SkCodec will
  // automatically fix up the pixels as it runs.
  SkAlphaType alpha = info.alphaType();
  if (alpha == kUnpremul_SkAlphaType) {
    info = info.makeAlphaType(kPremul_SkAlphaType);
  }

  // Decode the image pixels directly onto the SkBitmap. Alpha premultiplication
  // will be performed by `getPixels`. No colorspace conversion will occur,
  // because the bitmap uses the same colorspace as the original PNG.
  if (!bitmap->tryAllocPixels(info)) {
    return false;
  }

  return codec->getPixels(bitmap->pixmap()) == SkCodec::kSuccess;
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

bool EncodeSkPixmap(const SkPixmap& src,
                    const std::vector<PNGCodec::Comment>& comments,
                    std::vector<unsigned char>* output,
                    int zlib_level,
                    bool disable_filters) {
  output->clear();
  VectorWStream dst(output);

  SkPngEncoder::Options options;
  AddComments(options, comments);
  options.fZLibLevel = zlib_level;
  if (disable_filters)
    options.fFilterFlags = SkPngEncoder::FilterFlag::kNone;
  return SkPngEncoder::Encode(&dst, src, options);
}

bool EncodeSkPixmap(const SkPixmap& src,
                    bool discard_transparency,
                    const std::vector<PNGCodec::Comment>& comments,
                    std::vector<unsigned char>* output,
                    int zlib_level,
                    bool disable_filters) {
  if (discard_transparency) {
    SkImageInfo opaque_info = src.info().makeAlphaType(kOpaque_SkAlphaType);
    SkBitmap copy;
    if (!copy.tryAllocPixels(opaque_info)) {
      return false;
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
    return EncodeSkPixmap(opaque_pixmap, comments, output, zlib_level,
                          disable_filters);
  }

  // If the image's pixels are all opaque, encode the PNG as opaque, regardless
  // of the pixmap's alphaType.
  if (src.info().alphaType() != kOpaque_SkAlphaType && src.computeIsOpaque()) {
    SkPixmap opaque_pixmap{src.info().makeAlphaType(kOpaque_SkAlphaType),
                           src.addr(), src.rowBytes()};
    return EncodeSkPixmap(opaque_pixmap, comments, output, zlib_level,
                          disable_filters);
  }

  // Encode the PNG without any conversions.
  return EncodeSkPixmap(src, comments, output, zlib_level, disable_filters);
}

bool EncodeSkBitmap(const SkBitmap& input,
                    bool discard_transparency,
                    std::vector<unsigned char>* output,
                    int zlib_level,
                    bool disable_filters) {
  SkPixmap src;
  if (!input.peekPixels(&src)) {
    return false;
  }
  return EncodeSkPixmap(src, discard_transparency,
                        std::vector<PNGCodec::Comment>(), output, zlib_level,
                        disable_filters);
}

}  // namespace

// static
bool PNGCodec::Encode(const unsigned char* input,
                      ColorFormat format,
                      const Size& size,
                      int row_byte_width,
                      bool discard_transparency,
                      const std::vector<Comment>& comments,
                      std::vector<unsigned char>* output) {
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
  return EncodeSkPixmap(src, discard_transparency, comments, output,
                        DEFAULT_ZLIB_COMPRESSION, /* disable_filters= */ false);
}

// static
bool PNGCodec::EncodeBGRASkBitmap(const SkBitmap& input,
                                  bool discard_transparency,
                                  std::vector<unsigned char>* output) {
  return EncodeSkBitmap(input, discard_transparency, output,
                        DEFAULT_ZLIB_COMPRESSION, /* disable_filters= */ false);
}

// static
bool PNGCodec::EncodeA8SkBitmap(const SkBitmap& input,
                                std::vector<unsigned char>* output) {
  DCHECK_EQ(input.colorType(), kAlpha_8_SkColorType);
  auto info = input.info()
                  .makeColorType(kGray_8_SkColorType)
                  .makeAlphaType(kOpaque_SkAlphaType);
  SkPixmap src(info, input.getAddr(0, 0), input.rowBytes());
  return EncodeSkPixmap(src, std::vector<PNGCodec::Comment>(), output,
                        DEFAULT_ZLIB_COMPRESSION, /* disable_filters= */ false);
}

// static
bool PNGCodec::FastEncodeBGRASkBitmap(const SkBitmap& input,
                                      bool discard_transparency,
                                      std::vector<unsigned char>* output) {
  return EncodeSkBitmap(input, discard_transparency, output, Z_BEST_SPEED,
                        /* disable_filters= */ true);
}

PNGCodec::Comment::Comment(const std::string& k, const std::string& t)
    : key(k), text(t) {}

PNGCodec::Comment::~Comment() = default;

}  // namespace gfx
