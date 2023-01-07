// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/webp_codec.h"

#include <vector>

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

absl::optional<std::vector<uint8_t>> WebpCodec::EncodeAnimated(
    const std::vector<SkEncoder::Frame>& frames,
    const SkWebpEncoder::Options& options) {
  std::vector<uint8_t> output;
  VectorWStream dst(&output);

  if (!SkWebpEncoder::EncodeAnimated(&dst, frames, options)) {
    return absl::nullopt;
  }

  return output;
}

absl::optional<std::vector<uint8_t>> WebpCodec::EncodeAnimated(
    const std::vector<Frame>& frames,
    const SkWebpEncoder::Options& options) {
  std::vector<SkEncoder::Frame> pixmap_frames;
  for (const auto& frame : frames) {
    SkEncoder::Frame pixmap_frame;
    if (!frame.bitmap.peekPixels(&pixmap_frame.pixmap)) {
      return absl::nullopt;
    }
    pixmap_frame.duration = frame.duration;
    pixmap_frames.push_back(pixmap_frame);
  }

  return WebpCodec::EncodeAnimated(pixmap_frames, options);
}

}  // namespace gfx
