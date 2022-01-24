// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CODEC_WEBP_CODEC_H_
#define UI_GFX_CODEC_WEBP_CODEC_H_

#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/codec/codec_export.h"

class SkBitmap;

namespace gfx {

class Size;

// Interface for encoding WebP data. This is currently only used
// in the devtools protocol to encode screenshots, so currently only minimally
// supports lossy encoding.
class CODEC_EXPORT WebpCodec {
 public:
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
};

}  // namespace gfx

#endif  // UI_GFX_CODEC_WEBP_CODEC_H_
