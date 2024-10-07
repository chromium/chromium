// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CODEC_WEBP_CODEC_H_
#define UI_GFX_CODEC_WEBP_CODEC_H_

#include <optional>
#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/encode/SkEncoder.h"
#include "third_party/skia/include/encode/SkWebpEncoder.h"
#include "ui/gfx/codec/codec_export.h"

class SkBitmap;

namespace gfx {

class Size;

// Interface for encoding WebP data. This is currently only used
// in the devtools protocol to encode screenshots, so currently only minimally
// supports lossy encoding.
class CODEC_EXPORT WebpCodec {
 public:
  struct CODEC_EXPORT Frame {
    // Bitmap of the frame.
    SkBitmap bitmap;
    // Duration of the frame in milliseconds.
    int duration;
  };

  WebpCodec(const WebpCodec&) = delete;
  WebpCodec& operator=(const WebpCodec&) = delete;

  // Encodes (lossy) the `input` bitmap.
  //
  // Returns the encoded data on success, or std::nullopt on failure.
  //
  // quality: an integer in the range 0-100, where 100 is the highest quality.
  //          Since this currently only supports lossy encoding, a higher
  //          quality means a higher visual quality.
  static std::optional<std::vector<uint8_t>> Encode(const SkBitmap& input,
                                                    int quality);

  // DEPRECATED version of above.
  static bool Encode(const SkBitmap& input,
                     int quality,
                     std::vector<unsigned char>* output);

  // Encodes the pixmap 'frames' as an animated WebP image. Returns the encoded
  // data on success, or std::nullopt on failure.
  static std::optional<std::vector<uint8_t>> EncodeAnimated(
      const std::vector<SkEncoder::Frame>& frames,
      const SkWebpEncoder::Options& options);

  // Encodes the bitmap 'frames' as an animated WebP image. Returns the encoded
  // data on success, or std::nullopt on failure.
  static std::optional<std::vector<uint8_t>> EncodeAnimated(
      const std::vector<Frame>& frames,
      const SkWebpEncoder::Options& options);
};

}  // namespace gfx

#endif  // UI_GFX_CODEC_WEBP_CODEC_H_
