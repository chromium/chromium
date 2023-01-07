// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CODEC_JPEG_CODEC_H_
#define UI_GFX_CODEC_JPEG_CODEC_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "ui/gfx/codec/codec_export.h"

class SkBitmap;

namespace gfx {

// Interface for encoding/decoding JPEG data. This is a wrapper around libjpeg,
// which has an inconvenient interface for callers. This is only used for UI
// elements, WebKit has its own more complicated JPEG decoder which handles,
// among other things, partially downloaded data.
class CODEC_EXPORT JPEGCodec {
 public:
  enum ColorFormat {
    // 4 bytes per pixel, in RGBA order in mem regardless of endianness.
    FORMAT_RGBA,

    // 4 bytes per pixel, in BGRA order in mem regardless of endianness.
    // This is the default Windows DIB order.
    FORMAT_BGRA,

    // 4 bytes per pixel, it can be either RGBA or BGRA. It depends on the bit
    // order in kARGB_8888_Config skia bitmap.
    FORMAT_SkBitmap
  };

  // Encodes the given raw 'input' pixmap, which includes a pointer to pixels
  // as well as information describing the pixel format. The encoded JPEG data
  // will be written into the supplied vector and true will be returned on
  // success. On failure (false), the contents of the output buffer are
  // undefined.
  //
  // downsample: specifies how pixels will be sampled in the encoded JPEG image,
  //             can be either k420, k422 or k444.
  // quality: an integer in the range 0-100, where 100 is the highest quality.
  static bool Encode(const SkPixmap& input,
                     int quality,
                     SkJpegEncoder::Downsample downsample,
                     std::vector<unsigned char>* output);

  // Encodes the given raw 'input' pixmap, which includes a pointer to pixels
  // as well as information describing the pixel format. The encoded JPEG data
  // will be written into the supplied vector and true will be returned on
  // success. On failure (false), the contents of the output buffer are
  // undefined.
  //
  // quality: an integer in the range 0-100, where 100 is the highest quality.
  static bool Encode(const SkPixmap& input,
                     int quality,
                     std::vector<unsigned char>* output);

  // Encodes the 'input' bitmap.  The encoded JPEG data will be written into
  // the supplied vector and true will be returned on success. On failure
  // (false), the contents of the output buffer are undefined.
  //
  // quality: an integer in the range 0-100, where 100 is the highest quality.
  static bool Encode(const SkBitmap& input,
                     int quality,
                     std::vector<unsigned char>* output);

  // Decodes the JPEG data contained in input of length input_size. The
  // decoded data will be placed in *output with the dimensions in *w and *h
  // on success (returns true). This data will be written in the'format'
  // format. On failure, the values of these output variables is undefined.
  static bool Decode(const unsigned char* input, size_t input_size,
                     ColorFormat format, std::vector<unsigned char>* output,
                     int* w, int* h);

  // Decodes the JPEG data contained in input of length input_size. If
  // successful, a SkBitmap is created and returned.
  static std::unique_ptr<SkBitmap> Decode(const unsigned char* input,
                                          size_t input_size);
};

}  // namespace gfx

#endif  // UI_GFX_CODEC_JPEG_CODEC_H_
