// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/webp_codec.h"

#include "third_party/skia/include/encode/SkWebpEncoder.h"
#include "ui/gfx/codec/vector_wstream.h"

namespace gfx {

// Encoder ---------------------------------------------------------------------

bool WebpCodec::Encode(const SkPixmap& input,
                       int quality,
                       std::vector<unsigned char>* output) {
  output->clear();
  VectorWStream dst(output);

  SkWebpEncoder::Options options;
  options.fQuality = quality;
  return SkWebpEncoder::Encode(&dst, input, options);
}

bool WebpCodec::Encode(const SkBitmap& src,
                       int quality,
                       std::vector<unsigned char>* output) {
  SkPixmap pixmap;
  if (!src.peekPixels(&pixmap)) {
    return false;
  }

  return WebpCodec::Encode(pixmap, quality, output);
}

}  // namespace gfx
