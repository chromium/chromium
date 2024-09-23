// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/webp_codec.h"

#include <vector>

#include "base/feature_list.h"
#include "ui/gfx/codec/vector_wstream.h"

BASE_FEATURE(kUseLosslessWebPCompression,
             "UseLosslessWebPCompression",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace gfx {

// Encoder ---------------------------------------------------------------------

bool WebpCodec::Encode(const SkPixmap& input,
                       int quality,
                       std::vector<unsigned char>* output) {
  output->clear();
  VectorWStream dst(output);

  SkWebpEncoder::Options options;
  options.fQuality = quality;
  bool use_lossless_webp = quality >= 100 && base::FeatureList::IsEnabled(
                                                 kUseLosslessWebPCompression);
  options.fCompression = use_lossless_webp
                             ? SkWebpEncoder::Compression::kLossless
                             : SkWebpEncoder::Compression::kLossy;
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

std::optional<std::vector<uint8_t>> WebpCodec::EncodeAnimated(
    const std::vector<SkEncoder::Frame>& frames,
    const SkWebpEncoder::Options& options) {
  std::vector<uint8_t> output;
  VectorWStream dst(&output);

  if (!SkWebpEncoder::EncodeAnimated(&dst, frames, options)) {
    return std::nullopt;
  }

  return output;
}

std::optional<std::vector<uint8_t>> WebpCodec::EncodeAnimated(
    const std::vector<Frame>& frames,
    const SkWebpEncoder::Options& options) {
  std::vector<SkEncoder::Frame> pixmap_frames;
  for (const auto& frame : frames) {
    SkEncoder::Frame pixmap_frame;
    if (!frame.bitmap.peekPixels(&pixmap_frame.pixmap)) {
      return std::nullopt;
    }
    pixmap_frame.duration = frame.duration;
    pixmap_frames.push_back(pixmap_frame);
  }

  return WebpCodec::EncodeAnimated(pixmap_frames, options);
}

}  // namespace gfx
