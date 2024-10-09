// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CODEC_JPEG_CODEC_H_
#define UI_GFX_CODEC_JPEG_CODEC_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "ui/gfx/codec/codec_export.h"

class SkBitmap;

namespace gfx {

// Interface for encoding/decoding JPEG data. This is a wrapper around Skia
// image codecs, with a simpler interface for callers. This is only used for UI
// elements; Blink has its own more complicated JPEG decoder which handles,
// among other things, partially downloaded data.
class CODEC_EXPORT JPEGCodec {
 public:
  // Encodes the given raw `input` pixmap, which includes a pointer to pixels as
  // well as information describing the pixel format.
  //
  // Returns the encoded data on success, or std::nullopt on failure.
  //
  // downsample: specifies how pixels will be sampled in the encoded JPEG image,
  // can be either k420, k422 or k444.
  //
  // quality: an integer in the range 0-100, where 100 is the highest quality.
  //
  // xmp_metadata: pointer to XMP metadata, or nullptr if there is no XMP
  // metadata.
  static std::optional<std::vector<uint8_t>> Encode(
      const SkPixmap& input,
      int quality,
      SkJpegEncoder::Downsample downsample,
      const SkData* xmp_metadata = nullptr);

  // DEPRECATED version of above.
  static bool Encode(const SkPixmap& input,
                     int quality,
                     SkJpegEncoder::Downsample downsample,
                     std::vector<unsigned char>* output,
                     const SkData* xmp_metadata = nullptr);

  // Encodes the given raw `input` pixmap, which includes a pointer to pixels
  // as well as information describing the pixel format.
  //
  // Returns the encoded data on success, or std::nullopt on failure.
  //
  // quality: an integer in the range 0-100, where 100 is the highest quality.
  static std::optional<std::vector<uint8_t>> Encode(const SkPixmap& input,
                                                    int quality);

  // DEPRECATED version of above.
  static bool Encode(const SkPixmap& input,
                     int quality,
                     std::vector<unsigned char>* output);

  // Encodes the `input` bitmap`
  //
  // Returns the encoded data on success, or std::nullopt on failure.
  //
  // quality: an integer in the range 0-100, where 100 is the highest quality.
  static std::optional<std::vector<uint8_t>> Encode(const SkBitmap& input,
                                                    int quality);

  // DEPRECATED version of above.
  static bool Encode(const SkBitmap& input,
                     int quality,
                     std::vector<unsigned char>* output);

  // Decodes the JPEG data contained in `input` of length `input_size`.
  // `color_type` must be RGBA_8888 or BGRA_8888. The decoded data will be
  // placed in *output, with the dimensions in *w and *h on success (returns
  // true). This data will be written in the `color_type` format. On failure,
  // the values of these output variables is undefined.
  //
  // TODO(https://crbug.com/371987005): Remove and replace, as this is untested
  // and unused other than in unit tests.
  [[nodiscard]] static bool Decode(const uint8_t* input,
                                   size_t input_size,
                                   SkColorType color_type,
                                   std::vector<uint8_t>* output,
                                   int* w,
                                   int* h);

  // Decodes the JPEG data contained in `input`.
  //
  // Returns a valid SkBitmap if the data can be decoded as a JPEG, and a null
  // SkBitmap otherwise.
  static SkBitmap Decode(base::span<const uint8_t> input);

  // DEPRECATED version of above.
  static std::unique_ptr<SkBitmap> Decode(const unsigned char* input,
                                          size_t input_size);
};

}  // namespace gfx

#endif  // UI_GFX_CODEC_JPEG_CODEC_H_
