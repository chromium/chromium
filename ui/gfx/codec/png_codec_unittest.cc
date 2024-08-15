// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/png_codec.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <iomanip>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libpng/png.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "third_party/zlib/zlib.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

namespace gfx {

namespace {

void MakeRGBImage(int w, int h, std::vector<unsigned char>* data) {
  data->resize(w * h * 3);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      size_t base = (y * w + x) * 3;
      (*data)[base] = x * 3;          // r
      (*data)[base + 1] = x * 3 + 1;  // g
      (*data)[base + 2] = x * 3 + 2;  // b
    }
  }
}

// Set use_transparency to write data into the alpha channel, otherwise it will
// be filled with 0xff. With the alpha channel stripped, this should yield the
// same image as MakeRGBImage above, so the code below can make reference
// images for conversion testing.
void MakeRGBAImage(int w,
                   int h,
                   bool use_transparency,
                   std::vector<unsigned char>* data) {
  data->resize(w * h * 4);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      size_t base = (y * w + x) * 4;
      (*data)[base] = x * 3;          // r
      (*data)[base + 1] = x * 3 + 1;  // g
      (*data)[base + 2] = x * 3 + 2;  // b
      if (use_transparency)
        (*data)[base + 3] = x * 3 + 3;  // a
      else
        (*data)[base + 3] = 0xFF;  // a (opaque)
    }
  }
}

// Creates a palette-based image.
void MakePaletteImage(int w,
                      int h,
                      std::vector<unsigned char>* data,
                      std::vector<png_color>* palette,
                      std::vector<unsigned char>* trans_chunk = 0) {
  data->resize(w * h);
  palette->resize(w);
  for (int i = 0; i < w; ++i) {
    png_color& color = (*palette)[i];
    color.red = i * 3;
    color.green = color.red + 1;
    color.blue = color.red + 2;
  }
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      (*data)[y * w + x] = x;  // palette index
    }
  }
  if (trans_chunk) {
    trans_chunk->resize(palette->size());
    for (std::size_t i = 0; i < trans_chunk->size(); ++i) {
      (*trans_chunk)[i] = i % 256;
    }
  }
}

// Creates a grayscale image without an alpha channel.
void MakeGrayscaleImage(int w, int h, std::vector<unsigned char>* data) {
  data->resize(w * h);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      (*data)[y * w + x] = x;  // gray value
    }
  }
}

// Creates a grayscale image with an alpha channel.
void MakeGrayscaleAlphaImage(int w, int h, std::vector<unsigned char>* data) {
  data->resize(w * h * 2);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      size_t base = (y * w + x) * 2;
      (*data)[base] = x;      // gray value
      (*data)[base + 1] = x;  // alpha
    }
  }
}

// User write function (to be passed to libpng by EncodeImage) which writes
// into a buffer instead of to a file.
void WriteImageData(png_structp png_ptr, png_bytep data, png_size_t length) {
  std::vector<unsigned char>& v =
      *static_cast<std::vector<unsigned char>*>(png_get_io_ptr(png_ptr));
  v.resize(v.size() + length);
  memcpy(&v[v.size() - length], data, length);
}

// User flush function; goes with WriteImageData, above.
void FlushImageData(png_structp /*png_ptr*/) {}

// Libpng user error function which allows us to print libpng errors using
// Chrome's logging facilities instead of stderr.
void LogLibPNGError(png_structp png_ptr, png_const_charp error_msg) {
  DLOG(ERROR) << "libpng encode error: " << error_msg;
  longjmp(png_jmpbuf(png_ptr), 1);
}

// Goes with LogLibPNGError, above.
void LogLibPNGWarning(png_structp png_ptr, png_const_charp warning_msg) {
  DLOG(ERROR) << "libpng encode warning: " << warning_msg;
}

// Color types supported by EncodeImage. Required because neither libpng nor
// PNGCodec::Encode supports all of the required values.
enum ColorType {
  COLOR_TYPE_GRAY = PNG_COLOR_TYPE_GRAY,
  COLOR_TYPE_GRAY_ALPHA = PNG_COLOR_TYPE_GRAY_ALPHA,
  COLOR_TYPE_PALETTE = PNG_COLOR_TYPE_PALETTE,
  COLOR_TYPE_RGB = PNG_COLOR_TYPE_RGB,
  COLOR_TYPE_RGBA = PNG_COLOR_TYPE_RGBA,
  COLOR_TYPE_BGR,
  COLOR_TYPE_BGRA,
  COLOR_TYPE_RGBX
};

constexpr size_t PixelBytesForColorType(ColorType color_type) {
  switch (color_type) {
    case COLOR_TYPE_GRAY:
      return 1;
    case COLOR_TYPE_GRAY_ALPHA:
      return 2;
    case COLOR_TYPE_PALETTE:
      return 1;
    case COLOR_TYPE_RGB:
      return 3;
    case COLOR_TYPE_RGBA:
      return 4;
    case COLOR_TYPE_BGR:
      return 3;
    case COLOR_TYPE_BGRA:
      return 4;
    case COLOR_TYPE_RGBX:
      return 4;
  }
  NOTREACHED();
}

std::tuple<uint8_t, uint8_t, uint8_t> Read3(const std::vector<uint8_t>& pixels,
                                            size_t base) {
  return std::tie(pixels[base], pixels[base + 1], pixels[base + 2]);
}

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> Read4(
    const std::vector<uint8_t>& pixels,
    size_t base) {
  return std::tie(pixels[base], pixels[base + 1], pixels[base + 2],
                  pixels[base + 3]);
}

struct ImageSpec {
  ImageSpec(int w, int h, const std::vector<uint8_t>& bytes, ColorType type)
      : w(w), h(h), bytes(bytes), type(type) {}
  ImageSpec(int w,
            int h,
            const std::vector<uint8_t>& bytes,
            ColorType type,
            const std::vector<png_color>& palette,
            const std::vector<uint8_t>& trans)
      : w(w), h(h), bytes(bytes), type(type), palette(palette), trans(trans) {}

  int w;
  int h;
  std::vector<uint8_t> bytes;
  ColorType type;
  std::vector<png_color> palette;
  std::vector<uint8_t> trans;

  SkColor ReadPixel(int x, int y) const;
};

SkColor ImageSpec::ReadPixel(int x, int y) const {
  size_t base = (y * w + x) * PixelBytesForColorType(type);
  uint8_t red = 0, green = 0, blue = 0, alpha = SK_AlphaOPAQUE;

  switch (type) {
    case COLOR_TYPE_GRAY:
      red = green = blue = bytes[base];
      break;
    case COLOR_TYPE_GRAY_ALPHA:
      red = green = blue = bytes[base];
      alpha = bytes[base + 1];
      break;
    case COLOR_TYPE_PALETTE:
      red = palette[bytes[base]].red;
      green = palette[bytes[base]].green;
      blue = palette[bytes[base]].blue;
      alpha = trans[bytes[base]];
      break;
    case COLOR_TYPE_RGB:
    case COLOR_TYPE_RGBX:
      std::tie(red, green, blue) = Read3(bytes, base);
      break;
    case COLOR_TYPE_RGBA:
      std::tie(red, green, blue, alpha) = Read4(bytes, base);
      break;
    case COLOR_TYPE_BGR:
      std::tie(blue, green, red) = Read3(bytes, base);
      break;
    case COLOR_TYPE_BGRA:
      std::tie(blue, green, red, alpha) = Read4(bytes, base);
      break;
    default:
      NOTREACHED();
  }

  return SkColorSetARGB(alpha, red, green, blue);
}

bool ImagesExactlyEqual(const ImageSpec& a, const ImageSpec& b) {
  if (a.w != b.w || a.h != b.h) {
    return false;
  }
  for (int x = 0; x < a.w; ++x) {
    for (int y = 0; y < a.h; ++y) {
      if (a.ReadPixel(x, y) != b.ReadPixel(x, y)) {
        return false;
      }
    }
  }
  return true;
}

bool ImageExactlyEqualsSkBitmap(const ImageSpec& a, const SkBitmap& b) {
  if (a.w != b.width() || a.h != b.height()) {
    return false;
  }
  for (int x = 0; x < a.w; ++x) {
    for (int y = 0; y < a.h; ++y) {
      SkColor ca = a.ReadPixel(x, y);
      uint32_t cb = *b.getAddr32(x, y);
      if (SkPreMultiplyColor(ca) != cb) {
        return false;
      }
    }
  }

  return true;
}

// PNG encoder used for testing. Required because PNGCodec::Encode doesn't do
// interlaced, palette-based, or grayscale images, but PNGCodec::Decode is
// actually asked to decode these types of images by Chrome.
bool EncodeImage(const std::vector<unsigned char>& input,
                 const int width,
                 const int height,
                 ColorType output_color_type,
                 std::vector<unsigned char>* output,
                 const int interlace_type = PNG_INTERLACE_NONE,
                 std::vector<png_color>* palette = 0,
                 std::vector<unsigned char>* palette_alpha = 0) {
  DCHECK(output);

  int input_rowbytes = width * PixelBytesForColorType(output_color_type);
  int transforms = PNG_TRANSFORM_IDENTITY;

  if (output_color_type == COLOR_TYPE_PALETTE && !palette) {
    return false;
  }

  if (output_color_type == COLOR_TYPE_RGBX) {
    output_color_type = COLOR_TYPE_RGB;
    transforms |= PNG_TRANSFORM_STRIP_FILLER_AFTER;
  } else if (output_color_type == COLOR_TYPE_BGR) {
    output_color_type = COLOR_TYPE_RGB;
    transforms |= PNG_TRANSFORM_BGR;
  } else if (output_color_type == COLOR_TYPE_BGRA) {
    output_color_type = COLOR_TYPE_RGBA;
    transforms |= PNG_TRANSFORM_BGR;
  }

  png_struct* png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr)
    return false;
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, nullptr);
    return false;
  }

  std::vector<png_bytep> row_pointers(height);
  for (int y = 0; y < height; ++y) {
    row_pointers[y] = const_cast<unsigned char*>(&input[y * input_rowbytes]);
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return false;
  }

  png_set_error_fn(png_ptr, nullptr, LogLibPNGError, LogLibPNGWarning);
  png_set_rows(png_ptr, info_ptr, &row_pointers[0]);
  png_set_write_fn(png_ptr, output, WriteImageData, FlushImageData);
  png_set_IHDR(png_ptr, info_ptr, width, height, 8, output_color_type,
               interlace_type, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  if (output_color_type == COLOR_TYPE_PALETTE) {
    png_set_PLTE(png_ptr, info_ptr, &palette->front(), palette->size());
    if (palette_alpha) {
      unsigned char* alpha_data = &palette_alpha->front();
      size_t alpha_size = palette_alpha->size();
      png_set_tRNS(png_ptr, info_ptr, alpha_data, alpha_size, nullptr);
    }
  }

  png_write_png(png_ptr, info_ptr, transforms, nullptr);

  png_destroy_write_struct(&png_ptr, &info_ptr);
  return true;
}

}  // namespace

// Returns true if each channel of the given two colors are "close." This is
// used for comparing colors where rounding errors may cause off-by-one.
bool ColorsClose(uint32_t a, uint32_t b) {
  return abs(static_cast<int>(SkColorGetB(a) - SkColorGetB(b))) < 2 &&
         abs(static_cast<int>(SkColorGetG(a) - SkColorGetG(b))) < 2 &&
         abs(static_cast<int>(SkColorGetR(a) - SkColorGetR(b))) < 2 &&
         abs(static_cast<int>(SkColorGetA(a) - SkColorGetA(b))) < 2;
}

// Returns true if the RGB components are "close."
bool NonAlphaColorsClose(uint32_t a, uint32_t b) {
  return abs(static_cast<int>(SkColorGetB(a) - SkColorGetB(b))) < 2 &&
         abs(static_cast<int>(SkColorGetG(a) - SkColorGetG(b))) < 2 &&
         abs(static_cast<int>(SkColorGetR(a) - SkColorGetR(b))) < 2;
}

// Returns true if the BGRA 32-bit SkColor specified by |a| is equivalent to the
// 8-bit Gray color specified by |b|.
bool BGRAGrayEqualsA8Gray(uint32_t a, uint8_t b) {
  return SkColorGetB(a) == b && SkColorGetG(a) == b && SkColorGetR(a) == b &&
         SkColorGetA(a) == 255;
}

void MakeTestBGRASkBitmap(int w, int h, SkBitmap* bmp) {
  bmp->allocN32Pixels(w, h);

  for (int x = 0; x < w; ++x) {
    for (int y = 0; y < h; ++y) {
      int i = y * w + x;
      *bmp->getAddr32(x, y) =
          SkPreMultiplyARGB(i % 255, i % 250, i % 245, i % 240);
    }
  }
}

void MakeTestA8SkBitmap(int w, int h, SkBitmap* bmp) {
  bmp->allocPixels(SkImageInfo::MakeA8(w, h));

  for (int x = 0; x < w; ++x) {
    for (int y = 0; y < h; ++y) {
      *bmp->getAddr8(x, y) = y * w + x;
    }
  }
}

TEST(PNGCodec, EncodeDecodeRGBA) {
  const int w = 20, h = 20;

  // create an image with known values, a must be opaque because it will be
  // lost during encoding
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, true, &original);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(PNGCodec::Encode(&original[0], PNGCodec::FORMAT_RGBA, Size(w, h),
                               w * 4, false, std::vector<PNGCodec::Comment>(),
                               &encoded));

  // decode
  std::vector<unsigned char> decoded;
  int outw, outh;
  {
    base::HistogramTester histograms;
    ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                                 PNGCodec::FORMAT_RGBA, &decoded, &outw,
                                 &outh));
    std::vector<base::Bucket> buckets =
        histograms.GetAllSamples("ImageDecoder.Png.UiGfxIntoVector");
    ASSERT_EQ(buckets.size(), 1u);
    ASSERT_GE(buckets[0].min, 0);
  }

  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_RGBA),
                         ImageSpec(outw, outh, decoded, COLOR_TYPE_RGBA)));
}

TEST(PNGCodec, EncodeDecodeBGRA) {
  const int w = 20, h = 20;

  // Create an image with known values, alpha must be opaque because it will be
  // lost during encoding.
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, true, &original);

  // Encode.
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(PNGCodec::Encode(&original[0], PNGCodec::FORMAT_BGRA, Size(w, h),
                               w * 4, false, std::vector<PNGCodec::Comment>(),
                               &encoded));

  // Decode, it should have the same size as the original.
  std::vector<unsigned char> decoded;
  int outw, outh;
  ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                               PNGCodec::FORMAT_BGRA, &decoded, &outw, &outh));

  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_BGRA),
                         ImageSpec(outw, outh, decoded, COLOR_TYPE_BGRA)));
}

TEST(PNGCodec, DecodePalette) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  std::vector<png_color> original_palette;
  std::vector<unsigned char> original_trans_chunk;
  MakePaletteImage(w, h, &original, &original_palette, &original_trans_chunk);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(EncodeImage(original, w, h, COLOR_TYPE_PALETTE, &encoded,
                          PNG_INTERLACE_NONE, &original_palette,
                          &original_trans_chunk));

  // decode
  std::vector<unsigned char> decoded;
  int outw, outh;
  ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                               PNGCodec::FORMAT_RGBA, &decoded, &outw, &outh));

  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_PALETTE,
                                   original_palette, original_trans_chunk),
                         ImageSpec(outw, outh, decoded, COLOR_TYPE_RGBA)));
}

TEST(PNGCodec, DecodeInterlacedPalette) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  std::vector<png_color> original_palette;
  std::vector<unsigned char> original_trans_chunk;
  MakePaletteImage(w, h, &original, &original_palette, &original_trans_chunk);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(EncodeImage(original, w, h, COLOR_TYPE_PALETTE, &encoded,
                          PNG_INTERLACE_ADAM7, &original_palette,
                          &original_trans_chunk));

  // decode
  std::vector<unsigned char> decoded;
  int outw, outh;
  ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                               PNGCodec::FORMAT_RGBA, &decoded, &outw, &outh));
  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_PALETTE,
                                   original_palette, original_trans_chunk),
                         ImageSpec(outw, outh, decoded, COLOR_TYPE_RGBA)));
}

TEST(PNGCodec, DecodeGrayscale) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeGrayscaleImage(w, h, &original);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(EncodeImage(original, w, h, COLOR_TYPE_GRAY, &encoded));

  // decode
  std::vector<unsigned char> decoded;
  int outw, outh;
  ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                               PNGCodec::FORMAT_RGBA, &decoded, &outw, &outh));
  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_GRAY),
                         ImageSpec(outw, outh, decoded, COLOR_TYPE_RGBA)));
}

TEST(PNGCodec, DecodeGrayscaleWithAlpha) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeGrayscaleAlphaImage(w, h, &original);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(EncodeImage(original, w, h, COLOR_TYPE_GRAY_ALPHA, &encoded));

  // decode
  std::vector<unsigned char> decoded;
  int outw, outh;
  ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                               PNGCodec::FORMAT_RGBA, &decoded, &outw, &outh));
  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_GRAY_ALPHA),
                         ImageSpec(outw, outh, decoded, COLOR_TYPE_RGBA)));
}

TEST(PNGCodec, DecodeInterlacedGrayscale) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeGrayscaleImage(w, h, &original);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(EncodeImage(original, w, h, COLOR_TYPE_GRAY, &encoded,
                          PNG_INTERLACE_ADAM7));

  // decode
  std::vector<unsigned char> decoded;
  int outw, outh;
  ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                               PNGCodec::FORMAT_RGBA, &decoded, &outw, &outh));
  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_GRAY),
                         ImageSpec(outw, outh, decoded, COLOR_TYPE_RGBA)));
}

TEST(PNGCodec, DecodeInterlacedGrayscaleWithAlpha) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeGrayscaleAlphaImage(w, h, &original);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(EncodeImage(original, w, h, COLOR_TYPE_GRAY_ALPHA, &encoded,
                          PNG_INTERLACE_ADAM7));

  // decode
  std::vector<unsigned char> decoded;
  int outw, outh;
  ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                               PNGCodec::FORMAT_RGBA, &decoded, &outw, &outh));
  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_GRAY_ALPHA),
                         ImageSpec(outw, outh, decoded, COLOR_TYPE_RGBA)));
}

TEST(PNGCodec, DecodeInterlacedRGBA) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, false, &original);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(EncodeImage(original, w, h, COLOR_TYPE_RGBA, &encoded,
                          PNG_INTERLACE_ADAM7));

  // decode, it should have the same size as the original
  std::vector<unsigned char> decoded;
  int outw, outh;
  ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                               PNGCodec::FORMAT_RGBA, &decoded, &outw, &outh));
  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_RGBA),
                         ImageSpec(outw, outh, decoded, COLOR_TYPE_RGBA)));
}

TEST(PNGCodec, DecodeInterlacedBGR) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeRGBImage(w, h, &original);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(EncodeImage(original, w, h, COLOR_TYPE_BGR, &encoded,
                          PNG_INTERLACE_ADAM7));

  // decode, it should have the same size as the original
  std::vector<unsigned char> decoded;
  int outw, outh;
  ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                               PNGCodec::FORMAT_BGRA, &decoded, &outw, &outh));
  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_BGR),
                         ImageSpec(outw, outh, decoded, COLOR_TYPE_BGRA)));
}

TEST(PNGCodec, DecodeInterlacedBGRA) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, false, &original);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(EncodeImage(original, w, h, COLOR_TYPE_BGRA, &encoded,
                          PNG_INTERLACE_ADAM7));

  // decode, it should have the same size as the original
  std::vector<unsigned char> decoded;
  int outw, outh;
  ASSERT_TRUE(PNGCodec::Decode(&encoded[0], encoded.size(),
                               PNGCodec::FORMAT_BGRA, &decoded, &outw, &outh));
  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(w, h, original, COLOR_TYPE_BGRA),
                         ImageSpec(outw, outh, original, COLOR_TYPE_BGRA)));
}

// Not encoding an interlaced PNG from SkBitmap because we don't do it
// anywhere, and the ability to do that requires more code changes.
TEST(PNGCodec, DecodeInterlacedRGBtoSkBitmap) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeRGBImage(w, h, &original);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(EncodeImage(original, w, h, COLOR_TYPE_RGB, &encoded,
                          PNG_INTERLACE_ADAM7));

  // Decode the encoded string.
  SkBitmap decoded_bitmap;
  {
    base::HistogramTester histograms;
    ASSERT_TRUE(
        PNGCodec::Decode(&encoded.front(), encoded.size(), &decoded_bitmap));
    std::vector<base::Bucket> buckets =
        histograms.GetAllSamples("ImageDecoder.Png.UiGfxIntoSkBitmap");
    ASSERT_EQ(buckets.size(), 1u);
    ASSERT_GE(buckets[0].min, 0);
  }

  EXPECT_EQ(decoded_bitmap.alphaType(), kOpaque_SkAlphaType);
  EXPECT_TRUE(ImageExactlyEqualsSkBitmap(
      ImageSpec(w, h, original, COLOR_TYPE_RGB), decoded_bitmap));
}

void DecodeInterlacedRGBAtoSkBitmap(bool use_transparency) {
  const int w = 20, h = 20;
  const ColorType color_type =
      use_transparency ? COLOR_TYPE_RGBA : COLOR_TYPE_RGBX;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, use_transparency, &original);

  // encode
  std::vector<unsigned char> encoded;
  ASSERT_TRUE(
      EncodeImage(original, w, h, color_type, &encoded, PNG_INTERLACE_ADAM7));

  // Decode the encoded string.
  SkBitmap decoded_bitmap;
  ASSERT_TRUE(
      PNGCodec::Decode(&encoded.front(), encoded.size(), &decoded_bitmap));
  EXPECT_EQ(decoded_bitmap.alphaType(),
            use_transparency ? kPremul_SkAlphaType : kOpaque_SkAlphaType);

  EXPECT_TRUE(ImageExactlyEqualsSkBitmap(ImageSpec(w, h, original, color_type),
                                         decoded_bitmap));
}

TEST(PNGCodec, DecodeInterlacedRGBAtoSkBitmap_Opaque) {
  DecodeInterlacedRGBAtoSkBitmap(/*use_transparency=*/false);
}

TEST(PNGCodec, DecodeInterlacedRGBAtoSkBitmap_Transparent) {
  DecodeInterlacedRGBAtoSkBitmap(/*use_transparency=*/true);
}

TEST(PNGCodec, EncoderSavesImagesWithAllOpaquePixelsAsOpaque) {
  const int w = 20, h = 20;

  // Create an RGBA image with all opaque pixels.
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, /*use_transparency=*/false, &original);

  // Encode the image, without discarding transparency.
  std::vector<unsigned char> png_data;
  ASSERT_TRUE(PNGCodec::Encode(&original.front(), PNGCodec::FORMAT_RGBA,
                               gfx::Size(w, h), w * 4,
                               /*discard_transparency=*/false,
                               std::vector<PNGCodec::Comment>{}, &png_data));

  // Decode the image into an SkBitmap.
  SkBitmap bitmap;
  ASSERT_TRUE(PNGCodec::Decode(&png_data.front(), png_data.size(), &bitmap));

  // Verify that the bitmap is opaque, despite coming from RGBA data.
  EXPECT_EQ(bitmap.info().alphaType(), kOpaque_SkAlphaType);
}

// Test that corrupted data decompression causes failures.
TEST(PNGCodec, DecodeCorrupted) {
  int w = 20, h = 20;

  // Make some random data (an uncompressed image).
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, false, &original);

  // It should fail when given non-PNG compressed data.
  std::vector<unsigned char> output;
  int outw, outh;
  EXPECT_FALSE(PNGCodec::Decode(&original[0], original.size(),
                                PNGCodec::FORMAT_RGBA, &output, &outw, &outh));

  // Make some compressed data.
  std::vector<unsigned char> compressed;
  ASSERT_TRUE(PNGCodec::Encode(&original[0], PNGCodec::FORMAT_RGBA, Size(w, h),
                               w * 4, false, std::vector<PNGCodec::Comment>(),
                               &compressed));

  // Try decompressing a truncated version.
  EXPECT_FALSE(PNGCodec::Decode(&compressed[0], compressed.size() / 2,
                                PNGCodec::FORMAT_RGBA, &output, &outw, &outh));

  // Corrupt it and try decompressing that.
  for (int i = 10; i < 30; i++)
    compressed[i] = i;
  EXPECT_FALSE(PNGCodec::Decode(&compressed[0], compressed.size(),
                                PNGCodec::FORMAT_RGBA, &output, &outw, &outh));
}

// Test decoding three PNG images, identical except for different gAMA chunks
// (with gamma values of 1.0, 1.8 and 2.2). All images are 256 x 256 pixels and
// 8-bit grayscale. The left half of the image is a solid block of medium gray
// (128 out of 255). The right half of the image alternates between black (0
// out of 255) and white (255 out of 255) in a checkerboard pattern.
//
// For the first file (gamma 1.0, linear), if you squint, the 128/255 left half
// should look about as bright as the checkerboard right half. PNGCodec::Decode
// applies gamma correction (assuming a default display gamma of 2.2), so the
// top left pixel value should be corrected from 128 to 186.
//
// The second file (gamma 1.8)'s correction is not as strong: from 128 to 145.
//
// The third file (gamma 2.2) matches the default display gamma and so the 128
// nominal value is unchanged. If you squint, the 128/255 left half should look
// darker than the right half.
//
// The "as used by libpng" formula for calculating these expected 186, 145 or
// (unchanged) 128 values can be seen in the diff at
// https://crrev.com/c/5402327/13/ui/gfx/codec/png_codec_unittest.cc
// and the same formula is at
// https://www.w3.org/TR/2003/REC-PNG-20031110/#13Decoder-gamma-handling but
// note the spec's caveat: "Viewers capable of full colour management... will
// perform more sophisticated calculations than those described here."
//
// Being corrected to 186, 145 or 128 assumes that, like libpng, the PNG
// decoder honors the gAMA chunk in the checkerboard.gamma*.png files. Those
// files don't have an iCCP color profile chunk, but since the PNGCodec::Decode
// API fills in a bag of RGBA pixels (without an associated colorspace), the
// PNGCodec::Decode implementation nonetheless applies sRGB color correction
// (approximately exponential) instead of basic gamma correction (literally
// exponential). This produces slightly different numbers: 188, 146 or 129. The
// code review in https://crrev.com/c/5402327 gives a little more context.
//
// When viewing these images in a browser, make sure to apply the "img {
// image-rendering: pixelated }" CSS. Otherwise, browsers will often blur when
// up-scaling (e.g. on high DPI displays), trumping the "two halves should have
// roughly equal / different brightness" effect. You can view the images at
// https://nigeltao.github.io/blog/2022/gamma-aware-pixelated-images.html
TEST(PNGCodec, DecodeGamma) {
  base::FilePath root_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_dir));
  base::FilePath data_dir = root_dir.AppendASCII("ui")
                                .AppendASCII("gfx")
                                .AppendASCII("test")
                                .AppendASCII("data")
                                .AppendASCII("codec");

  struct SourceFile {
    double gamma;
    unsigned char corrected;
    std::string filename;
  };

  const SourceFile kSourceFiles[] = {
      {1.0, 188, "checkerboard.gamma1dot0.png"},
      {1.8, 146, "checkerboard.gamma1dot8.png"},
      {2.2, 129, "checkerboard.gamma2dot2.png"},
  };

  for (const auto& sf : kSourceFiles) {
    base::FilePath filename = data_dir.AppendASCII(sf.filename);
    std::optional<const std::vector<uint8_t>> opt_input =
        base::ReadFileToBytes(filename);
    ASSERT_TRUE(opt_input.has_value()) << "failed to load: " << filename;
    const std::vector<uint8_t>& input = opt_input.value();
    ASSERT_GT(input.size(), 0u);

    std::vector<unsigned char> output;
    int outw, outh;
    ASSERT_TRUE(PNGCodec::Decode(&input[0], input.size(), PNGCodec::FORMAT_RGBA,
                                 &output, &outw, &outh));
    ASSERT_GT(output.size(), 0u);

    EXPECT_EQ(output[0], sf.corrected) << "gamma: " << sf.gamma;
  }
}

TEST(PNGCodec, EncodeBGRASkBitmapStridePadded) {
  const int kWidth = 20;
  const int kHeight = 20;
  const int kPaddedWidth = 32;
  const int kBytesPerPixel = 4;
  const int kRowBytes = kPaddedWidth * kBytesPerPixel;

  std::vector<uint32_t> original_pixels(kHeight * kPaddedWidth);
  SkImageInfo info = SkImageInfo::MakeN32Premul(kWidth, kHeight);
  SkBitmap original_bitmap;
  original_bitmap.setInfo(info, kRowBytes);
  original_bitmap.setPixels(original_pixels.data());

  // Write data over the source bitmap.
  // We write on the pad area here too.
  // The encoder should ignore the pad area.
  for (size_t i = 0; i < original_pixels.size(); i++) {
    original_pixels[i] = SkPreMultiplyARGB(i % 255, i % 250, i % 245, i % 240);
  }

  // Encode the bitmap.
  std::vector<unsigned char> encoded;
  PNGCodec::EncodeBGRASkBitmap(original_bitmap, false, &encoded);

  // Decode the encoded string.
  SkBitmap decoded_bitmap;
  EXPECT_TRUE(
      PNGCodec::Decode(&encoded.front(), encoded.size(), &decoded_bitmap));

  // Compare the original bitmap and the output bitmap. We use ColorsClose
  // as SkBitmaps are considered to be pre-multiplied, the unpremultiplication
  // (in Encode) and repremultiplication (in Decode) can be lossy.
  for (int x = 0; x < kWidth; x++) {
    for (int y = 0; y < kHeight; y++) {
      uint32_t original_pixel = *original_bitmap.getAddr32(x, y);
      uint32_t decoded_pixel = *decoded_bitmap.getAddr32(x, y);
      ASSERT_TRUE(ColorsClose(original_pixel, decoded_pixel))
          << "; original_pixel = " << std::hex << std::setw(8) << original_pixel
          << "; decoded_pixel = " << std::hex << std::setw(8) << decoded_pixel;
    }
  }
}

TEST(PNGCodec, EncodeBGRASkBitmap) {
  const int w = 20, h = 20;

  SkBitmap original_bitmap;
  MakeTestBGRASkBitmap(w, h, &original_bitmap);

  // Encode the bitmap.
  std::vector<unsigned char> encoded;
  PNGCodec::EncodeBGRASkBitmap(original_bitmap, false, &encoded);

  // Decode the encoded string.
  SkBitmap decoded_bitmap;
  EXPECT_TRUE(
      PNGCodec::Decode(&encoded.front(), encoded.size(), &decoded_bitmap));

  // Compare the original bitmap and the output bitmap. We use ColorsClose
  // as SkBitmaps are considered to be pre-multiplied, the unpremultiplication
  // (in Encode) and repremultiplication (in Decode) can be lossy.
  for (int x = 0; x < w; x++) {
    for (int y = 0; y < h; y++) {
      uint32_t original_pixel = *original_bitmap.getAddr32(x, y);
      uint32_t decoded_pixel = *decoded_bitmap.getAddr32(x, y);
      ASSERT_TRUE(ColorsClose(original_pixel, decoded_pixel))
          << "; original_pixel = " << std::hex << std::setw(8) << original_pixel
          << "; decoded_pixel = " << std::hex << std::setw(8) << decoded_pixel;
    }
  }
}

TEST(PNGCodec, EncodeA8SkBitmap) {
  const int w = 20, h = 20;

  SkBitmap original_bitmap;
  MakeTestA8SkBitmap(w, h, &original_bitmap);

  // Encode the bitmap.
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(PNGCodec::EncodeA8SkBitmap(original_bitmap, &encoded));

  // Decode the encoded string.
  SkBitmap decoded_bitmap;
  EXPECT_TRUE(
      PNGCodec::Decode(&encoded.front(), encoded.size(), &decoded_bitmap));

  for (int x = 0; x < w; x++) {
    for (int y = 0; y < h; y++) {
      uint8_t original_pixel = *original_bitmap.getAddr8(x, y);
      uint32_t decoded_pixel = *decoded_bitmap.getAddr32(x, y);
      EXPECT_TRUE(BGRAGrayEqualsA8Gray(decoded_pixel, original_pixel));
    }
  }
}

TEST(PNGCodec, EncodeBGRASkBitmapDiscardTransparency) {
  const int w = 20, h = 20;

  SkBitmap original_bitmap;
  MakeTestBGRASkBitmap(w, h, &original_bitmap);

  // Encode the bitmap.
  std::vector<unsigned char> encoded;
  PNGCodec::EncodeBGRASkBitmap(original_bitmap, true, &encoded);

  // Decode the encoded string.
  SkBitmap decoded_bitmap;
  EXPECT_TRUE(
      PNGCodec::Decode(&encoded.front(), encoded.size(), &decoded_bitmap));

  // Compare the original bitmap and the output bitmap. We need to
  // unpremultiply original_pixel, as the decoded bitmap doesn't have an alpha
  // channel.
  for (int x = 0; x < w; x++) {
    for (int y = 0; y < h; y++) {
      uint32_t original_pixel = *original_bitmap.getAddr32(x, y);
      uint32_t unpremultiplied =
          SkUnPreMultiply::PMColorToColor(original_pixel);
      uint32_t decoded_pixel = *decoded_bitmap.getAddr32(x, y);
      uint32_t unpremultiplied_decoded =
          SkUnPreMultiply::PMColorToColor(decoded_pixel);

      EXPECT_TRUE(NonAlphaColorsClose(unpremultiplied, unpremultiplied_decoded))
          << "Original_pixel: (" << SkColorGetR(unpremultiplied) << ", "
          << SkColorGetG(unpremultiplied) << ", "
          << SkColorGetB(unpremultiplied) << "), "
          << "Decoded pixel: (" << SkColorGetR(unpremultiplied_decoded) << ", "
          << SkColorGetG(unpremultiplied_decoded) << ", "
          << SkColorGetB(unpremultiplied_decoded) << ")";
    }
  }
}

TEST(PNGCodec, EncodeWithComment) {
  const int w = 10, h = 10;

  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, true, &original);

  std::vector<unsigned char> encoded;
  std::vector<PNGCodec::Comment> comments;
  comments.push_back(PNGCodec::Comment("key", "text"));
  comments.push_back(PNGCodec::Comment("test", "something"));
  comments.push_back(PNGCodec::Comment("have some", "spaces in both"));
  EXPECT_TRUE(PNGCodec::Encode(&original[0], PNGCodec::FORMAT_RGBA, Size(w, h),
                               w * 4, false, comments, &encoded));

  // Each chunk is of the form length (4 bytes), chunk type (tEXt), data,
  // checksum (4 bytes).  Make sure we find all of them in the encoded
  // results.
  const unsigned char kExpected1[] =
      "\x00\x00\x00\x08tEXtkey\x00text\x9e\xe7\x66\x51";
  const unsigned char kExpected2[] =
      "\x00\x00\x00\x0etEXttest\x00something\x29\xba\xef\xac";
  const unsigned char kExpected3[] =
      "\x00\x00\x00\x18tEXthave some\x00spaces in both\x8d\x69\x34\x2d";

  EXPECT_NE(base::ranges::search(encoded, kExpected1), encoded.end());
  EXPECT_NE(base::ranges::search(encoded, kExpected2), encoded.end());
  EXPECT_NE(base::ranges::search(encoded, kExpected3), encoded.end());
}

TEST(PNGCodec, EncodeDecodeWithVaryingCompressionLevels) {
  const int w = 20, h = 20;

  // create an image with known values, a must be opaque because it will be
  // lost during encoding
  SkBitmap original_bitmap;
  MakeTestBGRASkBitmap(w, h, &original_bitmap);

  // encode
  std::vector<unsigned char> encoded_normal;
  EXPECT_TRUE(
      PNGCodec::EncodeBGRASkBitmap(original_bitmap, false, &encoded_normal));

  std::vector<unsigned char> encoded_fast;
  EXPECT_TRUE(
      PNGCodec::FastEncodeBGRASkBitmap(original_bitmap, false, &encoded_fast));

  // Make sure the different compression settings actually do something; the
  // sizes should be different.
  EXPECT_NE(encoded_normal.size(), encoded_fast.size());

  // decode, they should be identical to the original.
  SkBitmap decoded;
  EXPECT_TRUE(
      PNGCodec::Decode(&encoded_normal[0], encoded_normal.size(), &decoded));
  EXPECT_TRUE(BitmapsAreEqual(decoded, original_bitmap));

  EXPECT_TRUE(
      PNGCodec::Decode(&encoded_fast[0], encoded_fast.size(), &decoded));
  EXPECT_TRUE(BitmapsAreEqual(decoded, original_bitmap));
}

TEST(PNGCodec, DecodingTruncatedEXIFChunkIsSafe) {
  // Libpng 1.6.37 had a bug which caused it to read two uninitialized bytes of
  // stack memory if a PNG contained an invalid EXIF chunk, when in progressive
  // reading mode. This would manifest as an MSAN error (crbug.com/332475837)
  // and was discovered by our fuzzer. The bug had been independently discovered
  // and fixed in Libpng by the time we found it; upgrading to 1.6.43 solved it.
  // See https://github.com/pnggroup/libpng/pull/552 for a deep-dive into this
  // issue with a libpng maintainer.
  static constexpr png_byte kPNGData[] = {
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
      0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0xf0,
      0x08, 0x06, 0x00, 0x00, 0x00, 0x3e, 0x55, 0xe9, 0x92, 0x00, 0x00, 0x00,
      0x95, 0x65, 0x58, 0x49, 0x66, 0x89, 0x47, 0x50, 0x4e, 0x0d, 0x0a, 0x1a,
      0x0a, 0x00, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61,
      0x61, 0x61, 0x61, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
      0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x26, 0x0b, 0x13, 0x01,
      0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45,
      0x07, 0x7d, 0x01, 0x1a, 0x16, 0x3b, 0x05, 0xc3, 0xff, 0x6f, 0x00, 0x00,
      0x00, 0x19, 0x74, 0x45, 0x58, 0x74, 0xb2, 0x43, 0x6f, 0x6d, 0x2d, 0x65,
      0xa0, 0x6e, 0x74, 0x00, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x00, 0x43,
      0x72, 0x65, 0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20,
      0x47, 0x49, 0x4d, 0xe2, 0x35, 0x87, 0xc3, 0xa1, 0x00, 0x00, 0x00, 0x49,
      0x45, 0x4e, 0x44, 0xef, 0x04, 0x3e, 0x00, 0xbf, 0x00, 0xae, 0x49, 0x44,
      0x41, 0x54, 0x68, 0x81, 0xed, 0xd5, 0x6b, 0x99, 0x25, 0x2e, 0xff, 0xff,
      0x00, 0xae, 0x79, 0x79, 0x79, 0x42, 0x60, 0x69, 0x82, 0x79, 0x79, 0x79,
      0xf0, 0x7e,
  };

  SkBitmap bitmap;
  EXPECT_FALSE(PNGCodec::Decode(kPNGData, sizeof(kPNGData), &bitmap));
}

}  // namespace gfx
