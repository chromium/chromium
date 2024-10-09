// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/jpeg_codec.h"

#include <climits>
#include <memory>
#include <optional>

#include "base/notreached.h"
#include "third_party/skia/include/codec/SkJpegDecoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "ui/gfx/codec/vector_wstream.h"

namespace gfx {

// Encoder ---------------------------------------------------------------------

std::optional<std::vector<uint8_t>> JPEGCodec::Encode(
    const SkPixmap& input,
    int quality,
    SkJpegEncoder::Downsample downsample,
    const SkData* xmp_metadata) {
  std::vector<uint8_t> output;
  VectorWStream dst(&output);

  SkJpegEncoder::Options options;
  options.fQuality = quality;
  options.fDownsample = downsample;
  if (xmp_metadata) {
    options.xmpMetadata = xmp_metadata;
  }

  if (!SkJpegEncoder::Encode(&dst, input, options)) {
    return std::nullopt;
  }

  return output;
}

// DEPRECATED
bool JPEGCodec::Encode(const SkPixmap& input,
                       int quality,
                       SkJpegEncoder::Downsample downsample,
                       std::vector<unsigned char>* output,
                       const SkData* xmp_metadata) {
  std::optional<std::vector<uint8_t>> result =
      Encode(input, quality, downsample, xmp_metadata);
  if (!result) {
    output->clear();
    return false;
  }

  *output = std::move(*result);
  return true;
}

std::optional<std::vector<uint8_t>> JPEGCodec::Encode(const SkPixmap& input,
                                                      int quality) {
  return Encode(input, quality, SkJpegEncoder::Downsample::k420);
}

// DEPRECATED
bool JPEGCodec::Encode(const SkPixmap& input,
                       int quality,
                       std::vector<unsigned char>* output) {
  return Encode(input, quality, SkJpegEncoder::Downsample::k420, output);
}

std::optional<std::vector<uint8_t>> JPEGCodec::Encode(const SkBitmap& src,
                                                      int quality) {
  SkPixmap pixmap;
  if (!src.peekPixels(&pixmap)) {
    return std::nullopt;
  }

  return JPEGCodec::Encode(pixmap, quality);
}

// DEPRECATED
bool JPEGCodec::Encode(const SkBitmap& src,
                       int quality,
                       std::vector<unsigned char>* output) {
  SkPixmap pixmap;
  if (!src.peekPixels(&pixmap)) {
    return false;
  }

  return JPEGCodec::Encode(pixmap, quality, output);
}

// Decoder --------------------------------------------------------------------

namespace {

struct PreparationOutput {
  std::unique_ptr<SkCodec> codec;
  SkImageInfo image_info;
};

std::optional<PreparationOutput> PrepareForJPEGDecode(
    base::span<const uint8_t> input,
    SkColorType color_type) {
  PreparationOutput output;

  // We only support 8-bit RGBA and BGRA color types.
  CHECK(color_type == kRGBA_8888_SkColorType ||
        color_type == kBGRA_8888_SkColorType)
      << "Invalid pixel format " << color_type;

  // Parse the input stream with the JPEG decoder, yielding a SkCodec.
  auto stream = std::make_unique<SkMemoryStream>(input.data(), input.size(),
                                                 /*copyData=*/false);
  SkCodec::Result result;
  output.codec = SkJpegDecoder::Decode(std::move(stream), &result);
  if (!output.codec || result != SkCodec::kSuccess) {
    return std::nullopt;
  }

  // Reject images that would exceed INT_MAX bytes.
  SkISize size = output.codec->dimensions();
  constexpr int kBytesPerPixel = 4;
  if (size.area() >= (INT_MAX / kBytesPerPixel)) {
    return std::nullopt;
  }

  // The fuzzer is able to make astronomically large bitmaps (30000x30000) from
  // very small inputs. Images this large can take several seconds to decode. In
  // a build instrumented for fuzzing, this time can balloon to over a minute.
  // To avoid timeouts, we limit the fuzzer to 16 million pixels. We don't
  // reject very wide or very tall images, as long as the image is reasonably
  // small on the other axis.
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  constexpr int kFuzzerPixelLimit = 4000 * 4000;
  if (size.area() >= kFuzzerPixelLimit) {
    return std::nullopt;
  }
#endif

  // Create an SkImageInfo matching our JPEG's dimensions and color type.
  output.image_info = SkImageInfo::Make(size, color_type, kOpaque_SkAlphaType);

  return output;
}

}  // namespace

bool JPEGCodec::Decode(const uint8_t* input,
                       size_t input_size,
                       SkColorType color_type,
                       std::vector<uint8_t>* output,
                       int* w,
                       int* h) {
  std::optional<PreparationOutput> preparation_output = PrepareForJPEGDecode(
      UNSAFE_BUFFERS(base::span(input, input_size)), color_type);
  if (!preparation_output) {
    return false;
  }

  *w = preparation_output->image_info.width();
  *h = preparation_output->image_info.height();

  // Decode the pixels into the `output` vector.
  output->resize(preparation_output->image_info.computeMinByteSize());
  SkCodec::Result result = preparation_output->codec->getPixels(
      preparation_output->image_info, &output->front(),
      preparation_output->image_info.minRowBytes());
  return result == SkCodec::kSuccess;
}

// static
SkBitmap JPEGCodec::Decode(base::span<const uint8_t> input) {
  constexpr SkColorType kFormat =  // Parens around (0) solve dead-code warning.
      (SK_R32_SHIFT == (0))   ? kRGBA_8888_SkColorType
      : (SK_B32_SHIFT == (0)) ? kBGRA_8888_SkColorType
                              : kUnknown_SkColorType;

  std::optional<PreparationOutput> preparation_output =
      PrepareForJPEGDecode(input, kFormat);
  if (!preparation_output) {
    return SkBitmap();
  }

  // Allocate pixel storage for the decoded JPEG.
  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(preparation_output->image_info.width(),
                                preparation_output->image_info.height())) {
    return SkBitmap();
  }

  // Decode the image pixels directly onto an SkBitmap.
  SkCodec::Result result =
      preparation_output->codec->getPixels(bitmap.pixmap());
  if (result == SkCodec::kSuccess) {
    return bitmap;
  } else {
    return SkBitmap();
  }
}

// DEPRECATED
std::unique_ptr<SkBitmap> JPEGCodec::Decode(const unsigned char* input,
                                            size_t input_size) {
  std::optional<SkBitmap> result =
      Decode(UNSAFE_TODO(base::span(input, input_size)));
  if (!result) {
    return std::make_unique<SkBitmap>();
  }
  return std::make_unique<SkBitmap>(std::move(result.value()));
}

}  // namespace gfx
