// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/jpeg_codec.h"

#include <climits>
#include <memory>

#include "base/notreached.h"
#include "third_party/skia/include/codec/SkJpegDecoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "ui/gfx/codec/vector_wstream.h"

namespace gfx {

// Encoder ---------------------------------------------------------------------

bool JPEGCodec::Encode(const SkPixmap& input,
                       int quality,
                       SkJpegEncoder::Downsample downsample,
                       std::vector<unsigned char>* output,
                       const SkData* xmp_metadata) {
  output->clear();
  VectorWStream dst(output);

  SkJpegEncoder::Options options;
  options.fQuality = quality;
  options.fDownsample = downsample;
  if (xmp_metadata) {
    options.xmpMetadata = xmp_metadata;
  }
  return SkJpegEncoder::Encode(&dst, input, options);
}

bool JPEGCodec::Encode(const SkPixmap& input,
                       int quality,
                       std::vector<unsigned char>* output) {
  return Encode(input, quality, SkJpegEncoder::Downsample::k420, output);
}

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

static bool PrepareForJPEGDecode(const unsigned char* input,
                                 size_t input_size,
                                 SkColorType color_type,
                                 std::unique_ptr<SkCodec>* codec,
                                 SkImageInfo* image_info) {
  // We only support 8-bit RGBA and BGRA color types.
  CHECK(color_type == kRGBA_8888_SkColorType ||
        color_type == kBGRA_8888_SkColorType)
      << "Invalid pixel format " << color_type;

  // Parse the input stream with the JPEG decoder, yielding a SkCodec.
  auto stream = std::make_unique<SkMemoryStream>(input, input_size,
                                                 /*copyData=*/false);
  SkCodec::Result result;
  *codec = SkJpegDecoder::Decode(std::move(stream), &result);
  if (!*codec || result != SkCodec::kSuccess) {
    return false;
  }

  // Create an SkImageInfo matching our JPEG's dimensions and color type.
  SkISize size = (*codec)->dimensions();
  *image_info = SkImageInfo::Make(size, color_type, kOpaque_SkAlphaType);

  // Reject images that would exceed INT_MAX bytes.
  constexpr int kBytesPerPixel = 4;
  return size.area() < (INT_MAX / kBytesPerPixel);
}

bool JPEGCodec::Decode(const unsigned char* input,
                       size_t input_size,
                       SkColorType color_type,
                       std::vector<unsigned char>* output,
                       int* w,
                       int* h) {
  // Prepare a codec and image info for this JPEG. Populate `w` and `h` even if
  // the image is too large to decode.
  std::unique_ptr<SkCodec> codec;
  SkImageInfo info;
  bool ok = PrepareForJPEGDecode(input, input_size, color_type, &codec, &info);

  *w = info.width();
  *h = info.height();
  if (!ok) {
    return false;
  }

  // Decode the pixels into the `output` vector.
  output->resize(info.computeMinByteSize());
  SkCodec::Result result =
      codec->getPixels(info, &output->front(), info.minRowBytes());
  return result == SkCodec::kSuccess;
}

// static
std::unique_ptr<SkBitmap> JPEGCodec::Decode(const unsigned char* input,
                                            size_t input_size) {
  std::unique_ptr<SkCodec> codec;
  SkImageInfo info;
  constexpr SkColorType kFormat =  // Parens around (0) solve dead-code warning.
      (SK_R32_SHIFT == (0))   ? kRGBA_8888_SkColorType
      : (SK_B32_SHIFT == (0)) ? kBGRA_8888_SkColorType
                              : kUnknown_SkColorType;

  if (!PrepareForJPEGDecode(input, input_size, kFormat, &codec, &info)) {
    return nullptr;
  }

  // Allocate pixel storage for the decoded JPEG.
  auto bitmap = std::make_unique<SkBitmap>();
  if (!bitmap->tryAllocN32Pixels(info.width(), info.height())) {
    return nullptr;
  }

  // Decode the image pixels directly onto an SkBitmap.
  SkCodec::Result result = codec->getPixels(bitmap->pixmap());
  return (result == SkCodec::kSuccess) ? std::move(bitmap) : nullptr;
}

}  // namespace gfx
