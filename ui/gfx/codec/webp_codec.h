// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CODEC_WEBP_CODEC_H_
#define UI_GFX_CODEC_WEBP_CODEC_H_

#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
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
  struct Frame {
    // Bitmap of the frame.
    SkBitmap bitmap;
    // Duration of the frame in milliseconds.
    int duration;
  };

  WebpCodec(const WebpCodec&) = delete;
  WebpCodec& operator=(const WebpCodec&) = delete;

  // Encodes (lossy) the given raw 'input' pixmap, which includes a pointer to
  // pixels as well as information describing the pixel format. The encoded WebP
  // data will be written into the supplied vector and true will be returned on
  // success. On failure (false), the contents of the output buffer are
  // undefined.
  //
  // quality: an integer in the range 0-100, where 100 is the highest quality.
  //          Since this currently only supports lossy encoding, a higher
  //          quality means a higher visual quality.
  static bool Encode(const SkPixmap& input,
                     int quality,
                     std::vector<unsigned char>* output);

  // Encodes (lossy) the 'input' bitmap. The encoded WebP data will be written
  // into the supplied vector and true will be returned on success. On failure
  // (false), the contents of the output buffer are undefined.
  //
  // quality: an integer in the range 0-100, where 100 is the highest quality.
  //          Since this currently only supports lossy encoding, a higher
  //          quality means a higher visual quality.
  static bool Encode(const SkBitmap& input,
                     int quality,
                     std::vector<unsigned char>* output);

  // Encodes the pixmap 'frames' as an animated WebP image. Returns the encoded
  // data on success, or absl::nullopt on failure.
  static absl::optional<std::vector<uint8_t>> EncodeAnimated(
      const std::vector<SkEncoder::Frame>& frames,
      const SkWebpEncoder::Options& options);

  // Encodes the bitmap 'frames' as an animated WebP image. Returns the encoded
  // data on success, or absl::nullopt on failure.
  static absl::optional<std::vector<uint8_t>> EncodeAnimated(
      const std::vector<Frame>& frames,
      const SkWebpEncoder::Options& options);
};

}  // namespace gfx

#endif  // UI_GFX_CODEC_WEBP_CODEC_H_
