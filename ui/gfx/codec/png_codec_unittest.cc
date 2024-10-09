// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/png_codec.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <iomanip>
#include <optional>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "skia/buildflags.h"
#include "skia/rusty_png_feature.h"
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

std::vector<uint8_t> MakeRGBImage(int width, int height) {
  std::vector<uint8_t> data;
  data.resize(width * height * 3);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      size_t base = (y * width + x) * 3;
      data[base] = x * 3;          // r
      data[base + 1] = x * 3 + 1;  // g
      data[base + 2] = x * 3 + 2;  // b
    }
  }

  return data;
}

// Set use_transparency to write data into the alpha channel, otherwise it will
// be filled with 0xff. With the alpha channel stripped, this should yield the
// same image as MakeRGBImage above, so the code below can make reference
// images for conversion testing.
std::vector<uint8_t> MakeRGBAImage(int width,
                                   int height,
                                   bool use_transparency) {
  std::vector<uint8_t> result;

  result.resize(width * height * 4);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      size_t base = (y * width + x) * 4;
      result[base] = x * 3;          // r
      result[base + 1] = x * 3 + 1;  // g
      result[base + 2] = x * 3 + 2;  // b
      if (use_transparency) {
        result[base + 3] = x * 3 + 3;  // a
      } else {
        result[base + 3] = 0xFF;  // a (opaque)
      }
    }
  }

  return result;
}

// Creates a palette-based image.
struct PaletteImage {
  std::vector<uint8_t> data;
  std::vector<png_color> palette;
  std::vector<uint8_t> trans_chunk;
};
PaletteImage MakePaletteImage(int width, int height) {
  PaletteImage image;

  image.data.resize(width * height);
  image.palette.resize(width);
  for (int i = 0; i < width; ++i) {
    png_color& color = image.palette[i];
    color.red = i * 3;
    color.green = color.red + 1;
    color.blue = color.red + 2;
  }
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      image.data[y * width + x] = x;  // palette index
    }
  }
  image.trans_chunk.resize(image.palette.size());
  for (std::size_t i = 0; i < image.trans_chunk.size(); ++i) {
    image.trans_chunk[i] = i % 256;
  }

  return image;
}

// Creates a grayscale image without an alpha channel.
std::vector<uint8_t> MakeGrayscaleImage(int width, int height) {
  std::vector<uint8_t> data;
  data.resize(width * height);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      data[y * width + x] = x;  // gray value
    }
  }
  return data;
}

// Creates a grayscale image with an alpha channel.
std::vector<uint8_t> MakeGrayscaleAlphaImage(int width, int height) {
  std::vector<uint8_t> data;
  data.resize(width * height * 2);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      size_t base = (y * width + x) * 2;
      data[base] = x;      // gray value
      data[base + 1] = y;  // alpha
    }
  }
  return data;
}

// User write function (to be passed to libpng by EncodeImage) which writes
// into a buffer instead of to a file.
void WriteImageData(png_structp png_ptr, png_bytep data, png_size_t length) {
  std::vector<uint8_t>& v =
      *static_cast<std::vector<uint8_t>*>(png_get_io_ptr(png_ptr));
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
  ImageSpec(int width,
            int height,
            const std::vector<uint8_t>& bytes,
            ColorType type)
      : width(width), height(height), bytes(bytes), type(type) {}
  ImageSpec(int width,
            int height,
            const std::vector<uint8_t>& bytes,
            ColorType type,
            const std::vector<png_color>& palette,
            const std::vector<uint8_t>& trans)
      : width(width),
        height(height),
        bytes(bytes),
        type(type),
        palette(palette),
        trans(trans) {}

  int width;
  int height;
  std::vector<uint8_t> bytes;
  ColorType type;
  std::vector<png_color> palette;
  std::vector<uint8_t> trans;

  SkColor ReadPixel(int x, int y) const;
};

SkColor ImageSpec::ReadPixel(int x, int y) const {
  size_t base = (y * width + x) * PixelBytesForColorType(type);
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
  if (a.width != b.width || a.height != b.height) {
    return false;
  }
  for (int x = 0; x < a.width; ++x) {
    for (int y = 0; y < a.height; ++y) {
      if (a.ReadPixel(x, y) != b.ReadPixel(x, y)) {
        return false;
      }
    }
  }
  return true;
}

bool ImageExactlyEqualsSkBitmap(const ImageSpec& a, const SkBitmap& b) {
  if (a.width != b.width() || a.height != b.height()) {
    return false;
  }
  for (int x = 0; x < a.width; ++x) {
    for (int y = 0; y < a.height; ++y) {
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
std::optional<std::vector<uint8_t>> EncodeImage(
    base::span<const uint8_t> input,
    const int width,
    const int height,
    ColorType output_color_type,
    const int interlace_type = PNG_INTERLACE_NONE,
    std::optional<base::span<const png_color>> palette = std::nullopt,
    std::optional<base::span<const uint8_t>> palette_alpha = std::nullopt) {
  std::vector<uint8_t> output;

  int input_rowbytes = width * PixelBytesForColorType(output_color_type);
  int transforms = PNG_TRANSFORM_IDENTITY;

  if (output_color_type == COLOR_TYPE_PALETTE && !palette) {
    return std::nullopt;
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
      png_create_write_struct(PNG_LIBPNG_VER_STRING, /*error_ptr=*/nullptr,
                              /*error_fn=*/nullptr, /*warn_fn=*/nullptr);
  if (!png_ptr) {
    return std::nullopt;
  }
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, nullptr);
    return std::nullopt;
  }

  std::vector<png_bytep> row_pointers(height);
  for (int y = 0; y < height; ++y) {
    row_pointers[y] = const_cast<uint8_t*>(&input[y * input_rowbytes]);
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return std::nullopt;
  }

  png_set_error_fn(png_ptr, nullptr, LogLibPNGError, LogLibPNGWarning);
  png_set_rows(png_ptr, info_ptr, &row_pointers[0]);
  png_set_write_fn(png_ptr, &output, WriteImageData, FlushImageData);
  png_set_IHDR(png_ptr, info_ptr, width, height, 8, output_color_type,
               interlace_type, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  if (output_color_type == COLOR_TYPE_PALETTE) {
    png_set_PLTE(png_ptr, info_ptr, palette->data(), palette->size());
    if (palette_alpha) {
      png_set_tRNS(png_ptr, info_ptr, palette_alpha->data(),
                   palette_alpha->size(), /*trans_color=*/nullptr);
    }
  }

  png_write_png(png_ptr, info_ptr, transforms, nullptr);

  png_destroy_write_struct(&png_ptr, &info_ptr);
  return output;
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

void MakeTestBGRASkBitmap(int width, int height, SkBitmap* bmp) {
  bmp->allocN32Pixels(width, height);

  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      int i = y * width + x;
      *bmp->getAddr32(x, y) =
          SkPreMultiplyARGB(i % 255, i % 250, i % 245, i % 240);
    }
  }
}

void MakeTestA8SkBitmap(int width, int height, SkBitmap* bmp) {
  bmp->allocPixels(SkImageInfo::MakeA8(width, height));

  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      *bmp->getAddr8(x, y) = y * width + x;
    }
  }
}

enum class RustFeatureState { kRustEnabled, kRustDisabled };

class PNGCodecTest : public testing::TestWithParam<RustFeatureState> {
 public:
  PNGCodecTest() {
    switch (GetParam()) {
      case RustFeatureState::kRustEnabled:
        features_.InitAndEnableFeature(skia::kRustyPngFeature);
        break;
      case RustFeatureState::kRustDisabled:
        features_.InitAndDisableFeature(skia::kRustyPngFeature);
        break;
    }
  }

 protected:
  base::test::ScopedFeatureList features_;
};

TEST_P(PNGCodecTest, EncodeDecodeRGBA) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // Create an image with known values, a must be opaque because it will be
  // lost during encoding.
  std::vector<uint8_t> original =
      MakeRGBAImage(kWidth, kHeight, /*use_transparency=*/true);

  // encode
  std::optional<std::vector<uint8_t>> encoded = PNGCodec::Encode(
      original.data(), PNGCodec::FORMAT_RGBA, Size(kWidth, kHeight), kWidth * 4,
      /*discard_transparency=*/false, std::vector<PNGCodec::Comment>());
  ASSERT_TRUE(encoded);

  // decode
  std::optional<PNGCodec::DecodeOutput> output;
  {
    base::HistogramTester histograms;
    output = PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_RGBA);
    ASSERT_TRUE(output);
    std::vector<base::Bucket> buckets =
        histograms.GetAllSamples("ImageDecoder.Png.UiGfxIntoVector");
    ASSERT_EQ(buckets.size(), 1u);
    ASSERT_GE(buckets[0].min, 0);
  }

  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(kWidth, kHeight, original, COLOR_TYPE_RGBA),
                         ImageSpec(output->width, output->height,
                                   output->output, COLOR_TYPE_RGBA)));
}

TEST_P(PNGCodecTest, EncodeDecodeBGRA) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // Create an image with known values, alpha must be opaque because it will be
  // lost during encoding.
  std::vector<uint8_t> original =
      MakeRGBAImage(kWidth, kHeight, /*use_transparency=*/true);

  // Encode.
  std::optional<std::vector<uint8_t>> encoded = PNGCodec::Encode(
      original.data(), PNGCodec::FORMAT_BGRA, Size(kWidth, kHeight), kWidth * 4,
      /*discard_transparency=*/false, std::vector<PNGCodec::Comment>());
  ASSERT_TRUE(encoded);

  // Decode, it should have the same size as the original.
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_BGRA);
  ASSERT_TRUE(output);

  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(kWidth, kHeight, original, COLOR_TYPE_BGRA),
                         ImageSpec(output->width, output->height,
                                   output->output, COLOR_TYPE_BGRA)));
}

TEST_P(PNGCodecTest, DecodePalette) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // create an image with known values
  PaletteImage original = MakePaletteImage(kWidth, kHeight);

  // encode
  std::optional<std::vector<uint8_t>> encoded =
      EncodeImage(original.data, kWidth, kHeight, COLOR_TYPE_PALETTE,
                  PNG_INTERLACE_NONE, original.palette, original.trans_chunk);
  ASSERT_TRUE(encoded);

  // decode
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_RGBA);
  ASSERT_TRUE(output);

  EXPECT_TRUE(ImagesExactlyEqual(
      ImageSpec(kWidth, kHeight, original.data, COLOR_TYPE_PALETTE,
                original.palette, original.trans_chunk),
      ImageSpec(output->width, output->height, output->output,
                COLOR_TYPE_RGBA)));
}

TEST_P(PNGCodecTest, DecodeInterlacedPalette) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // create an image with known values
  PaletteImage original = MakePaletteImage(kWidth, kHeight);

  // encode
  std::optional<std::vector<uint8_t>> encoded =
      EncodeImage(original.data, kWidth, kHeight, COLOR_TYPE_PALETTE,
                  PNG_INTERLACE_ADAM7, original.palette, original.trans_chunk);
  ASSERT_TRUE(encoded);

  // decode
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_RGBA);
  ASSERT_TRUE(output);

  EXPECT_TRUE(ImagesExactlyEqual(
      ImageSpec(kWidth, kHeight, original.data, COLOR_TYPE_PALETTE,
                original.palette, original.trans_chunk),
      ImageSpec(output->width, output->height, output->output,
                COLOR_TYPE_RGBA)));
}

TEST_P(PNGCodecTest, DecodeGrayscale) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // create an image with known values
  std::vector<uint8_t> original = MakeGrayscaleImage(kWidth, kHeight);

  // encode
  std::optional<std::vector<uint8_t>> encoded =
      EncodeImage(original, kWidth, kHeight, COLOR_TYPE_GRAY);
  ASSERT_TRUE(encoded);

  // decode
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_RGBA);
  ASSERT_TRUE(output);

  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(kWidth, kHeight, original, COLOR_TYPE_GRAY),
                         ImageSpec(output->width, output->height,
                                   output->output, COLOR_TYPE_RGBA)));
}

TEST_P(PNGCodecTest, DecodeGrayscaleWithAlpha) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // create an image with known values
  std::vector<uint8_t> original = MakeGrayscaleAlphaImage(kWidth, kHeight);

  // encode
  std::optional<std::vector<uint8_t>> encoded =
      EncodeImage(original, kWidth, kHeight, COLOR_TYPE_GRAY_ALPHA);
  ASSERT_TRUE(encoded);

  // decode
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_RGBA);
  ASSERT_TRUE(output);

  EXPECT_TRUE(ImagesExactlyEqual(
      ImageSpec(kWidth, kHeight, original, COLOR_TYPE_GRAY_ALPHA),
      ImageSpec(output->width, output->height, output->output,
                COLOR_TYPE_RGBA)));
}

TEST_P(PNGCodecTest, DecodeInterlacedGrayscale) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // create an image with known values
  std::vector<uint8_t> original = MakeGrayscaleImage(kWidth, kHeight);

  // encode
  std::optional<std::vector<uint8_t>> encoded =
      EncodeImage(original, kWidth, kHeight, COLOR_TYPE_GRAY);
  ASSERT_TRUE(encoded);

  // decode
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_RGBA);
  ASSERT_TRUE(output);

  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(kWidth, kHeight, original, COLOR_TYPE_GRAY),
                         ImageSpec(output->width, output->height,
                                   output->output, COLOR_TYPE_RGBA)));
}

TEST_P(PNGCodecTest, DecodeInterlacedGrayscaleWithAlpha) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // create an image with known values
  std::vector<uint8_t> original = MakeGrayscaleAlphaImage(kWidth, kHeight);

  // encode
  std::optional<std::vector<uint8_t>> encoded = EncodeImage(
      original, kWidth, kHeight, COLOR_TYPE_GRAY_ALPHA, PNG_INTERLACE_ADAM7);
  ASSERT_TRUE(encoded);

  // decode
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_RGBA);
  ASSERT_TRUE(output);

  EXPECT_TRUE(ImagesExactlyEqual(
      ImageSpec(kWidth, kHeight, original, COLOR_TYPE_GRAY_ALPHA),
      ImageSpec(output->width, output->height, output->output,
                COLOR_TYPE_RGBA)));
}

TEST_P(PNGCodecTest, DecodeInterlacedRGBA) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // create an image with known values
  std::vector<uint8_t> original =
      MakeRGBAImage(kWidth, kHeight, /*use_transparency=*/false);

  // encode
  std::optional<std::vector<uint8_t>> encoded = EncodeImage(
      original, kWidth, kHeight, COLOR_TYPE_RGBA, PNG_INTERLACE_ADAM7);
  ASSERT_TRUE(encoded);

  // decode, it should have the same size as the original
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_RGBA);
  ASSERT_TRUE(output);

  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(kWidth, kHeight, original, COLOR_TYPE_RGBA),
                         ImageSpec(output->width, output->height,
                                   output->output, COLOR_TYPE_RGBA)));
}

TEST_P(PNGCodecTest, DecodeInterlacedBGR) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // create an image with known values
  std::vector<uint8_t> original = MakeRGBImage(kWidth, kHeight);

  // encode
  std::optional<std::vector<uint8_t>> encoded = EncodeImage(
      original, kWidth, kHeight, COLOR_TYPE_BGR, PNG_INTERLACE_ADAM7);
  ASSERT_TRUE(encoded);

  // decode, it should have the same size as the original
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_BGRA);
  ASSERT_TRUE(output);

  EXPECT_TRUE(
      ImagesExactlyEqual(ImageSpec(kWidth, kHeight, original, COLOR_TYPE_BGR),
                         ImageSpec(output->width, output->height,
                                   output->output, COLOR_TYPE_BGRA)));
}

TEST_P(PNGCodecTest, DecodeInterlacedBGRA) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // create an image with known values
  std::vector<uint8_t> original = MakeRGBAImage(kWidth, kHeight, false);

  // encode
  std::optional<std::vector<uint8_t>> encoded = EncodeImage(
      original, kWidth, kHeight, COLOR_TYPE_BGRA, PNG_INTERLACE_ADAM7);
  ASSERT_TRUE(encoded);

  // decode, it should have the same size as the original
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(encoded.value(), PNGCodec::FORMAT_BGRA);
  ASSERT_TRUE(output);

  EXPECT_TRUE(ImagesExactlyEqual(
      ImageSpec(kWidth, kHeight, original, COLOR_TYPE_BGRA),
      ImageSpec(output->width, output->height, original, COLOR_TYPE_BGRA)));
}

// Not encoding an interlaced PNG from SkBitmap because we don't do it
// anywhere, and the ability to do that requires more code changes.
TEST_P(PNGCodecTest, DecodeInterlacedRGBtoSkBitmap) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // create an image with known values
  std::vector<uint8_t> original = MakeRGBImage(kWidth, kHeight);

  // encode
  std::optional<std::vector<uint8_t>> encoded = EncodeImage(
      original, kWidth, kHeight, COLOR_TYPE_RGB, PNG_INTERLACE_ADAM7);
  ASSERT_TRUE(encoded);

  // Decode the encoded string.
  SkBitmap decoded_bitmap;
  {
    base::HistogramTester histograms;
    decoded_bitmap = PNGCodec::Decode(encoded.value());
    ASSERT_FALSE(decoded_bitmap.isNull());
    std::vector<base::Bucket> buckets =
        histograms.GetAllSamples("ImageDecoder.Png.UiGfxIntoSkBitmap");
    ASSERT_EQ(buckets.size(), 1u);
    ASSERT_GE(buckets[0].min, 0);
  }

  EXPECT_EQ(decoded_bitmap.alphaType(), kOpaque_SkAlphaType);
  EXPECT_TRUE(ImageExactlyEqualsSkBitmap(
      ImageSpec(kWidth, kHeight, original, COLOR_TYPE_RGB), decoded_bitmap));
}

void DecodeInterlacedRGBAtoSkBitmap(bool use_transparency) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  const ColorType color_type =
      use_transparency ? COLOR_TYPE_RGBA : COLOR_TYPE_RGBX;

  // create an image with known values
  std::vector<uint8_t> original =
      MakeRGBAImage(kWidth, kHeight, use_transparency);

  // encode
  std::optional<std::vector<uint8_t>> encoded =
      EncodeImage(original, kWidth, kHeight, color_type, PNG_INTERLACE_ADAM7);
  ASSERT_TRUE(encoded);

  // Decode the encoded string.
  SkBitmap decoded_bitmap = PNGCodec::Decode(encoded.value());
  ASSERT_FALSE(decoded_bitmap.isNull());
  EXPECT_EQ(decoded_bitmap.alphaType(),
            use_transparency ? kPremul_SkAlphaType : kOpaque_SkAlphaType);

  EXPECT_TRUE(ImageExactlyEqualsSkBitmap(
      ImageSpec(kWidth, kHeight, original, color_type), decoded_bitmap));
}

TEST_P(PNGCodecTest, DecodeInterlacedRGBAtoSkBitmap_Opaque) {
  DecodeInterlacedRGBAtoSkBitmap(/*use_transparency=*/false);
}

TEST_P(PNGCodecTest, DecodeInterlacedRGBAtoSkBitmap_Transparent) {
  DecodeInterlacedRGBAtoSkBitmap(/*use_transparency=*/true);
}

TEST(PNGCodec, EncoderSavesImagesWithAllOpaquePixelsAsOpaque) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // Create an RGBA image with all opaque pixels.
  std::vector<uint8_t> original =
      MakeRGBAImage(kWidth, kHeight, /*use_transparency=*/false);

  // Encode the image, without discarding transparency.
  std::optional<std::vector<uint8_t>> png_data = PNGCodec::Encode(
      original.data(), PNGCodec::FORMAT_RGBA, gfx::Size(kWidth, kHeight),
      kWidth * 4,
      /*discard_transparency=*/false, std::vector<PNGCodec::Comment>{});
  ASSERT_TRUE(png_data);

  // Decode the image into an SkBitmap.
  SkBitmap bitmap = PNGCodec::Decode(png_data.value());
  ASSERT_FALSE(bitmap.isNull());

  // Verify that the bitmap is opaque, despite coming from RGBA data.
  EXPECT_EQ(bitmap.info().alphaType(), kOpaque_SkAlphaType);
}

// Test that corrupted data decompression causes failures.
TEST_P(PNGCodecTest, DecodeCorrupted) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // Make some random data (an uncompressed image).
  std::vector<uint8_t> original = MakeRGBAImage(kWidth, kHeight, false);

  // It should fail when given non-PNG compressed data.
  std::optional<PNGCodec::DecodeOutput> output =
      PNGCodec::Decode(original, PNGCodec::FORMAT_RGBA);
  ASSERT_FALSE(output);

  // Make some compressed data.
  std::optional<std::vector<uint8_t>> compressed = PNGCodec::Encode(
      &original[0], PNGCodec::FORMAT_RGBA, Size(kWidth, kHeight), kWidth * 4,
      false, std::vector<PNGCodec::Comment>());
  ASSERT_TRUE(compressed);

  // Try decompressing a truncated version.
  output = PNGCodec::Decode(
      base::span(compressed.value()).subspan(0, compressed.value().size() / 2),
      PNGCodec::FORMAT_RGBA);
  ASSERT_FALSE(output);

  // Corrupt it and try decompressing that.
  for (int i = 10; i < 30; i++) {
    compressed.value()[i] = i;
  }
  output = PNGCodec::Decode(compressed.value(), PNGCodec::FORMAT_RGBA);
  ASSERT_FALSE(output);
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
TEST_P(PNGCodecTest, DecodeGamma) {
  base::FilePath root_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_dir));
  base::FilePath data_dir = root_dir.AppendASCII("ui")
                                .AppendASCII("gfx")
                                .AppendASCII("test")
                                .AppendASCII("data")
                                .AppendASCII("codec");

  struct SourceFile {
    double gamma;
    uint8_t corrected;
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

    std::optional<PNGCodec::DecodeOutput> output =
        PNGCodec::Decode(input, PNGCodec::FORMAT_RGBA);
    ASSERT_TRUE(output);
    ASSERT_GT(output->output.size(), 0u);

    EXPECT_EQ(output->output[0], sf.corrected) << "gamma: " << sf.gamma;
  }
}

TEST_P(PNGCodecTest, EncodeBGRASkBitmapStridePadded) {
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
  std::optional<std::vector<uint8_t>> encoded = PNGCodec::EncodeBGRASkBitmap(
      original_bitmap, /*discard_transparency=*/false);

  // Decode the encoded string.
  SkBitmap decoded_bitmap = PNGCodec::Decode(encoded.value());
  EXPECT_FALSE(decoded_bitmap.isNull());

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

TEST_P(PNGCodecTest, EncodeBGRASkBitmap) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  SkBitmap original_bitmap;
  MakeTestBGRASkBitmap(kWidth, kHeight, &original_bitmap);

  // Encode the bitmap.
  std::optional<std::vector<uint8_t>> encoded = PNGCodec::EncodeBGRASkBitmap(
      original_bitmap, /*discard_transparency=*/false);

  // Decode the encoded string.
  SkBitmap decoded_bitmap = PNGCodec::Decode(encoded.value());
  EXPECT_FALSE(decoded_bitmap.isNull());

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

TEST_P(PNGCodecTest, EncodeBGRASkBitmapDiscardTransparency) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  SkBitmap original_bitmap;
  MakeTestBGRASkBitmap(kWidth, kHeight, &original_bitmap);

  // Encode the bitmap.
  std::optional<std::vector<uint8_t>> encoded = PNGCodec::EncodeBGRASkBitmap(
      original_bitmap, /*discard_transparency=*/true);

  // Decode the encoded string.
  SkBitmap decoded_bitmap = PNGCodec::Decode(encoded.value());
  EXPECT_FALSE(decoded_bitmap.isNull());

  // Compare the original bitmap and the output bitmap. We need to
  // unpremultiply original_pixel, as the decoded bitmap doesn't have an alpha
  // channel.
  for (int x = 0; x < kWidth; x++) {
    for (int y = 0; y < kHeight; y++) {
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

TEST_P(PNGCodecTest, EncodeWithComment) {
  constexpr int kWidth = 10;
  constexpr int kHeight = 10;

  std::vector<uint8_t> original =
      MakeRGBAImage(kWidth, kHeight, /*use_transparency=*/true);

  std::vector<PNGCodec::Comment> comments;
  comments.emplace_back("key", "text");
  comments.emplace_back("test", "something");
  comments.emplace_back("have some", "spaces in both");
  std::optional<std::vector<uint8_t>> encoded =
      PNGCodec::Encode(original.data(), PNGCodec::FORMAT_RGBA,
                       Size(kWidth, kHeight), kWidth * 4, false, comments);
  ASSERT_TRUE(encoded);

  // Each chunk is of the form length (4 bytes), chunk type (tEXt), data,
  // checksum (4 bytes).  Make sure we find all of them in the encoded
  // results.
  const uint8_t kExpected1[] =
      "\x00\x00\x00\x08tEXtkey\x00text\x9e\xe7\x66\x51";
  const uint8_t kExpected2[] =
      "\x00\x00\x00\x0etEXttest\x00something\x29\xba\xef\xac";
  const uint8_t kExpected3[] =
      "\x00\x00\x00\x18tEXthave some\x00spaces in both\x8d\x69\x34\x2d";

  EXPECT_NE(base::ranges::search(encoded.value(), kExpected1),
            encoded.value().end());
  EXPECT_NE(base::ranges::search(encoded.value(), kExpected2),
            encoded.value().end());
  EXPECT_NE(base::ranges::search(encoded.value(), kExpected3),
            encoded.value().end());
}

TEST_P(PNGCodecTest, EncodeDecodeWithVaryingCompressionLevels) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // Create an image with known values, a must be opaque because it will be
  // lost during encoding.
  SkBitmap original_bitmap;
  MakeTestBGRASkBitmap(kWidth, kHeight, &original_bitmap);

  // Encode.
  std::optional<std::vector<uint8_t>> encoded_normal =
      PNGCodec::EncodeBGRASkBitmap(original_bitmap,
                                   /*discard_transparency=*/false);
  ASSERT_TRUE(encoded_normal);

  std::optional<std::vector<uint8_t>> encoded_fast =
      PNGCodec::FastEncodeBGRASkBitmap(original_bitmap,
                                       /*discard_transparency=*/false);
  ASSERT_TRUE(encoded_fast);

  // Make sure the different compression settings actually do something; the
  // sizes should be different.
  EXPECT_NE(encoded_normal->size(), encoded_fast->size());

  // Decode, they should be identical to the original.
  SkBitmap decoded = PNGCodec::Decode(encoded_normal.value());
  EXPECT_FALSE(decoded.isNull());
  EXPECT_TRUE(BitmapsAreEqual(decoded, original_bitmap));

  decoded = PNGCodec::Decode(encoded_fast.value());
  EXPECT_FALSE(decoded.isNull());
  EXPECT_TRUE(BitmapsAreEqual(decoded, original_bitmap));
}

TEST_P(PNGCodecTest, DecodingTruncatedEXIFChunkIsSafe) {
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

  SkBitmap bitmap = PNGCodec::Decode(kPNGData);
  EXPECT_TRUE(bitmap.isNull());
}

#if BUILDFLAG(SKIA_BUILD_RUST_PNG)
INSTANTIATE_TEST_SUITE_P(RustEnabled,
                         PNGCodecTest,
                         ::testing::Values(RustFeatureState::kRustEnabled));
#endif

INSTANTIATE_TEST_SUITE_P(RustDisabled,
                         PNGCodecTest,
                         ::testing::Values(RustFeatureState::kRustDisabled));

}  // namespace gfx
