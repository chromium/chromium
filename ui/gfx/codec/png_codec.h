// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CODEC_PNG_CODEC_H_
#define UI_GFX_CODEC_PNG_CODEC_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "ui/gfx/codec/codec_export.h"

class SkBitmap;

namespace gfx {

class Size;

// Interface for encoding and decoding PNG data. This is a wrapper around
// libpng, which has an inconvenient interface for callers. This is currently
// designed for use in tests only (where we control the files), so the handling
// isn't as robust as would be required for a browser (see Decode() for more).
// WebKit has its own more complicated PNG decoder which handles, among other
// things, partially downloaded data.
class CODEC_EXPORT PNGCodec {
 public:
  static constexpr int DEFAULT_ZLIB_COMPRESSION = 6;

  enum ColorFormat {
    // 4 bytes per pixel, in RGBA order in memory regardless of endianness.
    // Alpha is unpremultiplied, the same as what PNG uses.
    FORMAT_RGBA,

    // 4 bytes per pixel, in BGRA order in memory regardless of endianness.
    // Alpha is unpremultiplied, the same as what PNG uses.
    //
    // This is the default Windows DIB order.
    FORMAT_BGRA,

    // SkBitmap format. For Encode() kN32_SkColorType (4 bytes per pixel) and
    // kAlpha_8_SkColorType (1 byte per pixel) formats are supported.
    // kAlpha_8_SkColorType gets encoded into a grayscale PNG treating alpha as
    // the color intensity. For Decode() kN32_SkColorType is always used.
    //
    // When encoding from a SkBitmap (which implicitly chooses the
    // FORMAT_SkBitmap ColorFormat), this respects the input's alpha type:
    // kOpaque_SkAlphaType, kPremul_SkAlphaType or kUnpremul_SkAlphaType. For
    // premul input, the Encode callee will convert to PNG's unpremul.
    //
    // When encoding from a std::vector, passing FORMAT_SkBitmap treats the
    // pixel data as kN32_SkColorType and kPremul_SkAlphaType. Again, the
    // Encode callee will convert to PNG's unpremul.
    //
    // When decoding with FORMAT_SkBitmap (implicitly if passing a SkBitmap* to
    // Decode), the output SkBitmap or pixel data is kN32_SkColorType and
    // either kOpaque_SkAlphaType or kPremul_SkAlphaType, depending on whether
    // the source PNG image is opaque.
    //
    // FORMAT_SkBitmap prefers premultiplied alpha even though the PNG file
    // format (and other ColorFormat enum values) work with unpremultiplied
    // alpha. Per SkAlphaType documentation, premultiplied color components
    // improve performance.
    //
    // This implies that, for a kUnpremul_SkAlphaType input SkBitmap, a round
    // trip (encoding to PNG and decoding back via this file's functions) can
    // be lossy, even though PNG is a lossless format (in unpremultiplied alpha
    // space). The input can distinguish completely transparent red from
    // completely transparent black but the output will not.
    FORMAT_SkBitmap
  };

  // Represents a comment in the tEXt ancillary chunk of the png.
  struct CODEC_EXPORT Comment {
    Comment(const std::string& k, const std::string& t);
    ~Comment();

    std::string key;
    std::string text;
  };

  PNGCodec(const PNGCodec&) = delete;
  PNGCodec& operator=(const PNGCodec&) = delete;

  // Encodes the given raw `input` data, with each pixel being represented as
  // given in `format`.
  //
  // Returns the encoded data on success, or std::nullopt on failure.
  //
  // size: dimensions of the image
  //
  // row_byte_width: the width in bytes of each row. This may be greater than `w
  // * bytes_per_pixel` if there is extra padding at the end of each row (often,
  // each row is padded to the next machine word).
  //
  // discard_transparency: when true, and when the input data format includes
  // alpha values, these alpha values will be discarded and only RGB will be
  // written to the resulting file. Otherwise, alpha values in the input will be
  // preserved.
  //
  // comments: comments to be written in the png's metadata.
  //
  // TODO(https://crbug.com/371929522): This is a dangerous call, as it takes an
  // unsafe raw pointer and cannot be spanified. Most callers are actually
  // calling this with an SkBitmap that doesn't qualify for the other two
  // SkBitmap calls below. This method (and the two SkBitmap methods below)
  // should be removed, and a more general SkBitmap encoding method should be
  // added.
  static std::optional<std::vector<uint8_t>> Encode(
      const unsigned char* input,
      ColorFormat format,
      const Size& size,
      int row_byte_width,
      bool discard_transparency,
      const std::vector<Comment>& comments);

  // DEPRECATED version of above.
  static bool Encode(const unsigned char* input,
                     ColorFormat format,
                     const Size& size,
                     int row_byte_width,
                     bool discard_transparency,
                     const std::vector<Comment>& comments,
                     std::vector<unsigned char>* output);

  // Call `PNGCodec::Encode` on the supplied SkBitmap `input`, which is assumed
  // to be `kN32_SkColorType`, 32 bits per pixel. The params
  // `discard_transparency` and `output` are passed directly to Encode(); refer
  // to Encode() for more information.
  static std::optional<std::vector<uint8_t>> EncodeBGRASkBitmap(
      const SkBitmap& input,
      bool discard_transparency);

  // DEPRECATED version of above.
  static bool EncodeBGRASkBitmap(const SkBitmap& input,
                                 bool discard_transparency,
                                 std::vector<unsigned char>* output);

  // Call `PNGCodec::Encode` on the supplied SkBitmap `input`. The difference
  // between this and the previous method is that this restricts compression to
  // zlib q1, which is just rle encoding.
  static std::optional<std::vector<uint8_t>> FastEncodeBGRASkBitmap(
      const SkBitmap& input,
      bool discard_transparency);

  // DEPRECATED version of above.
  static bool FastEncodeBGRASkBitmap(const SkBitmap& input,
                                     bool discard_transparency,
                                     std::vector<unsigned char>* output);

  // Decodes the PNG data contained in `input`.
  //
  // Returns the decoded data on success, or std::nullopt on failure. The output
  // data will be written in the format specified by the `format` parameter.
  //
  // This function may not support all PNG types, and it hasn't been tested
  // with a large number of images, so assume a new format may not work. It's
  // really designed to be able to read in something written by Encode() above.
  //
  // TODO(https://crbug.com/371926662): There are few callers; remove.
  struct CODEC_EXPORT DecodeOutput {
    DecodeOutput();
    ~DecodeOutput();
    DecodeOutput(const DecodeOutput& other);
    DecodeOutput& operator=(const DecodeOutput& other);

    std::vector<uint8_t> output;
    int width = 0;
    int height = 0;
  };
  static std::optional<DecodeOutput> Decode(base::span<const uint8_t> input,
                                            ColorFormat format);

  // Decodes the PNG data directly into an SkBitmap. This is significantly
  // faster than the vector<uint8_t> version of Decode() above when dealing with
  // PNG files that are >500K, which a lot of theme images are. (There are a lot
  // of themes that have a NTP image of about ~1 megabyte, and those require a
  // 7-10 megabyte side buffer.)
  //
  // Returns a valid SkBitmap if the data can be decoded as a PNG, and a null
  // SkBitmap otherwise.
  static SkBitmap Decode(base::span<const uint8_t> input);

  // DEPRECATED version of above.
  static bool Decode(const unsigned char* input,
                     size_t input_size,
                     SkBitmap* bitmap);
};

}  // namespace gfx

#endif  // UI_GFX_CODEC_PNG_CODEC_H_
