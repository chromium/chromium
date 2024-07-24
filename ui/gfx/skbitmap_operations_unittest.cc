// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/skbitmap_operations.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"

namespace {

// Returns true if each channel of the given two colors are "close." This is
// used for comparing colors where rounding errors may cause off-by-one.
inline bool ColorsClose(uint32_t a, uint32_t b) {
  return abs(static_cast<int>(SkColorGetB(a) - SkColorGetB(b))) <= 2 &&
         abs(static_cast<int>(SkColorGetG(a) - SkColorGetG(b))) <= 2 &&
         abs(static_cast<int>(SkColorGetR(a) - SkColorGetR(b))) <= 2 &&
         abs(static_cast<int>(SkColorGetA(a) - SkColorGetA(b))) <= 2;
}

inline bool MultipliedColorsClose(uint32_t a, uint32_t b) {
  return ColorsClose(SkUnPreMultiply::PMColorToColor(a),
                     SkUnPreMultiply::PMColorToColor(b));
}

bool BitmapsClose(const SkBitmap& a, const SkBitmap& b) {
  for (int y = 0; y < a.height(); y++) {
    for (int x = 0; x < a.width(); x++) {
      SkColor a_pixel = *a.getAddr32(x, y);
      SkColor b_pixel = *b.getAddr32(x, y);
      if (!ColorsClose(a_pixel, b_pixel))
        return false;
    }
  }
  return true;
}

void FillDataToBitmap(int w, int h, SkBitmap* bmp) {
  bmp->allocN32Pixels(w, h);

  unsigned char* src_data =
      reinterpret_cast<unsigned char*>(bmp->getAddr32(0, 0));
  for (int i = 0; i < w * h; i++) {
    const int alpha = i % 256;
    src_data[i * 4 + 0] = static_cast<unsigned char>(alpha);
    src_data[i * 4 + 1] = static_cast<unsigned char>((i + 16) % (alpha + 1));
    src_data[i * 4 + 2] = static_cast<unsigned char>((i + 32) % (alpha + 1));
    src_data[i * 4 + 3] = static_cast<unsigned char>((i + 64) % (alpha + 1));
  }
}

// The reference (i.e., old) implementation of |CreateHSLShiftedBitmap()|.
SkBitmap ReferenceCreateHSLShiftedBitmap(
    const SkBitmap& bitmap,
    color_utils::HSL hsl_shift) {
  SkBitmap shifted;
  shifted.allocN32Pixels(bitmap.width(), bitmap.height());
  shifted.eraseARGB(0, 0, 0, 0);

  // Loop through the pixels of the original bitmap.
  for (int y = 0; y < bitmap.height(); ++y) {
    SkPMColor* pixels = bitmap.getAddr32(0, y);
    SkPMColor* tinted_pixels = shifted.getAddr32(0, y);

    for (int x = 0; x < bitmap.width(); ++x) {
      tinted_pixels[x] = SkPreMultiplyColor(color_utils::HSLShift(
          SkUnPreMultiply::PMColorToColor(pixels[x]), hsl_shift));
    }
  }

  return shifted;
}

}  // namespace

// Invert bitmap and verify the each pixel is inverted and the alpha value is
// not changed.
TEST(SkBitmapOperationsTest, CreateInvertedBitmap) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  src.allocN32Pixels(src_w, src_h);

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      int i = y * src_w + x;
      *src.getAddr32(x, y) =
          SkColorSetARGB((255 - i) % 255, i % 255, i * 4 % 255, 0);
    }
  }

  SkBitmap inverted = SkBitmapOperations::CreateInvertedBitmap(src);

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      int i = y * src_w + x;
      EXPECT_EQ(static_cast<unsigned int>((255 - i) % 255),
                SkColorGetA(*inverted.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(255 - (i % 255)),
                SkColorGetR(*inverted.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(255 - (i * 4 % 255)),
                SkColorGetG(*inverted.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(255),
                SkColorGetB(*inverted.getAddr32(x, y)));
    }
  }
}

// Blend two bitmaps together at 50% alpha and verify that the result
// is the middle-blend of the two.
TEST(SkBitmapOperationsTest, CreateBlendedBitmap) {
  int src_w = 16, src_h = 16;
  SkBitmap src_a;
  src_a.allocN32Pixels(src_w, src_h);

  SkBitmap src_b;
  src_b.allocN32Pixels(src_w, src_h);

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src_a.getAddr32(x, y) = SkColorSetARGB(255, 0, i * 2 % 255, i % 255);
      *src_b.getAddr32(x, y) =
          SkColorSetARGB((255 - i) % 255, i % 255, i * 4 % 255, 0);
      i++;
    }
  }

  // Shift to red.
  SkBitmap blended = SkBitmapOperations::CreateBlendedBitmap(
    src_a, src_b, 0.5);

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      int i = y * src_w + x;
      EXPECT_EQ(static_cast<unsigned int>((255 + ((255 - i) % 255)) / 2),
                SkColorGetA(*blended.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(i % 255 / 2),
                SkColorGetR(*blended.getAddr32(x, y)));
      EXPECT_EQ((static_cast<unsigned int>((i * 2) % 255 + (i * 4) % 255) / 2),
                SkColorGetG(*blended.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(i % 255 / 2),
                SkColorGetB(*blended.getAddr32(x, y)));
    }
  }
}

// Test our masking functions.
TEST(SkBitmapOperationsTest, CreateMaskedBitmap) {
  const int src_w = 16, src_h = 16;

  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  SkBitmap alpha;
  alpha.allocN32Pixels(src_w, src_h);
  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *alpha.getAddr32(x, y) = SkPackARGB32(i % 256, 0, 0, 0);
      i++;
    }
  }

  SkBitmap masked = SkBitmapOperations::CreateMaskedBitmap(src, alpha);

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      int alpha_pixel = *alpha.getAddr32(x, y);
      int src_pixel = *src.getAddr32(x, y);
      int masked_pixel = *masked.getAddr32(x, y);

      int scale = SkAlpha255To256(SkGetPackedA32(alpha_pixel));

      int src_a = (src_pixel >> SK_A32_SHIFT) & 0xFF;
      int src_r = (src_pixel >> SK_R32_SHIFT) & 0xFF;
      int src_g = (src_pixel >> SK_G32_SHIFT) & 0xFF;
      int src_b = (src_pixel >> SK_B32_SHIFT) & 0xFF;

      int masked_a = (masked_pixel >> SK_A32_SHIFT) & 0xFF;
      int masked_r = (masked_pixel >> SK_R32_SHIFT) & 0xFF;
      int masked_g = (masked_pixel >> SK_G32_SHIFT) & 0xFF;
      int masked_b = (masked_pixel >> SK_B32_SHIFT) & 0xFF;

      EXPECT_EQ((src_a * scale) >> 8, masked_a);
      EXPECT_EQ((src_r * scale) >> 8, masked_r);
      EXPECT_EQ((src_g * scale) >> 8, masked_g);
      EXPECT_EQ((src_b * scale) >> 8, masked_b);
    }
  }
}

// Make sure that when shifting a bitmap without any shift parameters,
// the end result is close enough to the original (rounding errors
// notwithstanding).
TEST(SkBitmapOperationsTest, CreateHSLShiftedBitmapToSame) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  src.allocN32Pixels(src_w, src_h);

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src.getAddr32(x, y) = SkPreMultiplyColor(SkColorSetARGB((i + 128) % 255,
          (i + 128) % 255, (i + 64) % 255, (i + 0) % 255));
      i++;
    }
  }

  color_utils::HSL hsl = { -1, -1, -1 };
  SkBitmap shifted = ReferenceCreateHSLShiftedBitmap(src, hsl);

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      SkColor src_pixel = *src.getAddr32(x, y);
      SkColor shifted_pixel = *shifted.getAddr32(x, y);
      EXPECT_TRUE(MultipliedColorsClose(src_pixel, shifted_pixel)) <<
          "source: (a,r,g,b) = (" << SkColorGetA(src_pixel) << "," <<
                                     SkColorGetR(src_pixel) << "," <<
                                     SkColorGetG(src_pixel) << "," <<
                                     SkColorGetB(src_pixel) << "); " <<
          "shifted: (a,r,g,b) = (" << SkColorGetA(shifted_pixel) << "," <<
                                     SkColorGetR(shifted_pixel) << "," <<
                                     SkColorGetG(shifted_pixel) << "," <<
                                     SkColorGetB(shifted_pixel) << ")";
    }
  }
}

// Shift a blue bitmap to red.
TEST(SkBitmapOperationsTest, CreateHSLShiftedBitmapHueOnly) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  src.allocN32Pixels(src_w, src_h);

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src.getAddr32(x, y) = SkColorSetARGB(255, 0, 0, i % 255);
      i++;
    }
  }

  // Shift to red.
  color_utils::HSL hsl = { 0, -1, -1 };

  SkBitmap shifted = SkBitmapOperations::CreateHSLShiftedBitmap(src, hsl);

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      EXPECT_TRUE(ColorsClose(shifted.getColor(x, y),
                              SkColorSetARGB(255, i % 255, 0, 0)));
      i++;
    }
  }
}

// Validate HSL shift.
TEST(SkBitmapOperationsTest, ValidateHSLShift) {
  // Note: 255/51 = 5 (exactly) => 6 including 0!
  const int inc = 51;
  const int dim = 255 / inc + 1;
  SkBitmap src;
  src.allocN32Pixels(dim*dim, dim*dim);

  for (int a = 0, y = 0; a <= 255; a += inc) {
    for (int r = 0; r <= 255; r += inc, y++) {
      for (int g = 0, x = 0; g <= 255; g += inc) {
        for (int b = 0; b <= 255; b+= inc, x++) {
          *src.getAddr32(x, y) =
              SkPreMultiplyColor(SkColorSetARGB(a, r, g, b));
        }
      }
    }
  }

  // Shhhh. The spec says I should set things to -1 for "no change", but
  // actually -0.1 will do. Don't tell anyone I did this.
  for (double h = -0.1; h <= 1.0001; h += 0.1) {
    for (double s = -0.1; s <= 1.0001; s += 0.1) {
      for (double l = -0.1; l <= 1.0001; l += 0.1) {
        color_utils::HSL hsl = { h, s, l };
        SkBitmap ref_shifted = ReferenceCreateHSLShiftedBitmap(src, hsl);
        SkBitmap shifted = SkBitmapOperations::CreateHSLShiftedBitmap(src, hsl);
        EXPECT_TRUE(BitmapsClose(ref_shifted, shifted))
            << "h = " << h << ", s = " << s << ", l = " << l;
      }
    }
  }
}

// Test our cropping.
TEST(SkBitmapOperationsTest, CreateCroppedBitmap) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(src, 4, 4,
                                                              8, 8);
  ASSERT_EQ(8, cropped.width());
  ASSERT_EQ(8, cropped.height());

  for (int y = 4; y < 12; y++) {
    for (int x = 4; x < 12; x++) {
      EXPECT_EQ(*src.getAddr32(x, y),
                *cropped.getAddr32(x - 4, y - 4));
    }
  }
}

// Test whether our cropping correctly wraps across image boundaries.
TEST(SkBitmapOperationsTest, CreateCroppedBitmapWrapping) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(
      src, src_w / 2, src_h / 2, src_w, src_h);
  ASSERT_EQ(src_w, cropped.width());
  ASSERT_EQ(src_h, cropped.height());

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      EXPECT_EQ(*src.getAddr32(x, y),
                *cropped.getAddr32((x + src_w / 2) % src_w,
                                   (y + src_h / 2) % src_h));
    }
  }
}

TEST(SkBitmapOperationsTest, DownsampleByTwo) {
  // Use an odd-sized bitmap to make sure the edge cases where there isn't a
  // 2x2 block of pixels is handled correctly.
  // Here's the ARGB example
  //
  //    50% transparent green             opaque 50% blue           white
  //        80008000                         FF000080              FFFFFFFF
  //
  //    50% transparent red               opaque 50% gray           black
  //        80800000                         80808080              FF000000
  //
  //         black                            white                50% gray
  //        FF000000                         FFFFFFFF              FF808080
  //
  // The result of this computation should be:
  //        A0404040  FF808080
  //        FF808080  FF808080
  SkBitmap input;
  input.allocN32Pixels(3, 3);

  // The color order may be different, but we don't care (the channels are
  // trated the same).
  *input.getAddr32(0, 0) = 0x80008000;
  *input.getAddr32(1, 0) = 0xFF000080;
  *input.getAddr32(2, 0) = 0xFFFFFFFF;
  *input.getAddr32(0, 1) = 0x80800000;
  *input.getAddr32(1, 1) = 0x80808080;
  *input.getAddr32(2, 1) = 0xFF000000;
  *input.getAddr32(0, 2) = 0xFF000000;
  *input.getAddr32(1, 2) = 0xFFFFFFFF;
  *input.getAddr32(2, 2) = 0xFF808080;

  SkBitmap result = SkBitmapOperations::DownsampleByTwo(input);
  EXPECT_EQ(2, result.width());
  EXPECT_EQ(2, result.height());

  // Some of the values are off-by-one due to rounding.
  EXPECT_EQ(0x9f404040, *result.getAddr32(0, 0));
  EXPECT_EQ(0xFF7f7f7f, *result.getAddr32(1, 0));
  EXPECT_EQ(0xFF7f7f7f, *result.getAddr32(0, 1));
  EXPECT_EQ(0xFF808080, *result.getAddr32(1, 1));
}

// Test edge cases for DownsampleByTwo.
TEST(SkBitmapOperationsTest, DownsampleByTwoSmall) {
  SkPMColor reference = 0xFF4080FF;

  // Test a 1x1 bitmap.
  SkBitmap one_by_one;
  one_by_one.allocN32Pixels(1, 1);
  *one_by_one.getAddr32(0, 0) = reference;
  SkBitmap result = SkBitmapOperations::DownsampleByTwo(one_by_one);
  EXPECT_EQ(1, result.width());
  EXPECT_EQ(1, result.height());
  EXPECT_EQ(reference, *result.getAddr32(0, 0));

  // Test an n by 1 bitmap.
  SkBitmap one_by_n;
  one_by_n.allocN32Pixels(300, 1);
  result = SkBitmapOperations::DownsampleByTwo(one_by_n);
  EXPECT_EQ(300, result.width());
  EXPECT_EQ(1, result.height());

  // Test a 1 by n bitmap.
  SkBitmap n_by_one;
  n_by_one.allocN32Pixels(1, 300);
  result = SkBitmapOperations::DownsampleByTwo(n_by_one);
  EXPECT_EQ(1, result.width());
  EXPECT_EQ(300, result.height());

  // Test an empty bitmap
  SkBitmap empty;
  result = SkBitmapOperations::DownsampleByTwo(empty);
  EXPECT_TRUE(result.isNull());
  EXPECT_EQ(0, result.width());
  EXPECT_EQ(0, result.height());
}

// Here we assume DownsampleByTwo works correctly (it's tested above) and
// just make sure that the wrapper function does the right thing.
TEST(SkBitmapOperationsTest, DownsampleByTwoUntilSize) {
  // First make sure a "too small" bitmap doesn't get modified at all.
  SkBitmap too_small;
  too_small.allocN32Pixels(10, 10);
  SkBitmap result = SkBitmapOperations::DownsampleByTwoUntilSize(
      too_small, 16, 16);
  EXPECT_EQ(10, result.width());
  EXPECT_EQ(10, result.height());

  // Now make sure giving it a 0x0 target returns something reasonable.
  result = SkBitmapOperations::DownsampleByTwoUntilSize(too_small, 0, 0);
  EXPECT_EQ(1, result.width());
  EXPECT_EQ(1, result.height());

  // Test multiple steps of downsampling.
  SkBitmap large;
  large.allocN32Pixels(100, 43);
  result = SkBitmapOperations::DownsampleByTwoUntilSize(large, 6, 6);

  // The result should be divided in half 100x43 -> 50x22 -> 25x11
  EXPECT_EQ(25, result.width());
  EXPECT_EQ(11, result.height());
}

TEST(SkBitmapOperationsTest, UnPreMultiply) {
  SkBitmap input;
  input.allocN32Pixels(2, 2);
  EXPECT_EQ(input.alphaType(), kPremul_SkAlphaType);

  // Set PMColors into the bitmap
  *input.getAddr32(0, 0) = SkPackARGB32(0x80, 0x00, 0x00, 0x00);
  *input.getAddr32(1, 0) = SkPackARGB32(0x80, 0x80, 0x80, 0x80);
  *input.getAddr32(0, 1) = SkPackARGB32(0xFF, 0x00, 0xCC, 0x88);
  *input.getAddr32(1, 1) = SkPackARGB32(0x00, 0x00, 0xCC, 0x88);

  SkBitmap result = SkBitmapOperations::UnPreMultiply(input);
  EXPECT_EQ(result.alphaType(), kUnpremul_SkAlphaType);
  EXPECT_EQ(2, result.width());
  EXPECT_EQ(2, result.height());
  EXPECT_NE(result.getPixels(), input.getPixels());

  EXPECT_EQ(0x80000000, *result.getAddr32(0, 0));
  EXPECT_EQ(0x80FFFFFF, *result.getAddr32(1, 0));
  EXPECT_EQ(0xFF00CC88, *result.getAddr32(0, 1));
  EXPECT_EQ(0x00000000u, *result.getAddr32(1, 1));  // "Division by zero".
}

TEST(SkBitmapOperationsTest, UnPreMultiplyOpaque) {
  SkBitmap input;
  input.allocN32Pixels(2, 2, true);
  EXPECT_EQ(input.alphaType(), kOpaque_SkAlphaType);

  SkBitmap result = SkBitmapOperations::UnPreMultiply(input);
  EXPECT_EQ(result.alphaType(), kOpaque_SkAlphaType);
  EXPECT_EQ(result.getPixels(), input.getPixels());
}

TEST(SkBitmapOperationsTest, UnPreMultiplyAlreadyUnPreMultiplied) {
  SkBitmap input;
  input.allocN32Pixels(2, 2);
  input.setAlphaType(kUnpremul_SkAlphaType);
  EXPECT_EQ(input.alphaType(), kUnpremul_SkAlphaType);

  SkBitmap result = SkBitmapOperations::UnPreMultiply(input);
  EXPECT_EQ(result.alphaType(), kUnpremul_SkAlphaType);
  EXPECT_EQ(result.getPixels(), input.getPixels());
}

TEST(SkBitmapOperationsTest, CreateTransposedBitmap) {
  SkBitmap input;
  input.allocN32Pixels(2, 3);

  for (int x = 0; x < input.width(); ++x) {
    for (int y = 0; y < input.height(); ++y) {
      *input.getAddr32(x, y) = x * input.width() + y;
    }
  }

  SkBitmap result = SkBitmapOperations::CreateTransposedBitmap(input);
  EXPECT_EQ(3, result.width());
  EXPECT_EQ(2, result.height());

  for (int x = 0; x < input.width(); ++x) {
    for (int y = 0; y < input.height(); ++y) {
      EXPECT_EQ(*input.getAddr32(x, y), *result.getAddr32(y, x));
    }
  }
}

void DrawRectWithColor(SkCanvas* canvas,
                       int left,
                       int top,
                       int right,
                       int bottom,
                       SkColor color) {
  SkPaint paint;
  paint.setColor(color);
  paint.setBlendMode(SkBlendMode::kSrc);
  canvas->drawRect(
      SkRect::MakeLTRB(SkIntToScalar(left), SkIntToScalar(top),
                       SkIntToScalar(right), SkIntToScalar(bottom)),
      paint);
}

// Check that Rotate provides the desired results
TEST(SkBitmapOperationsTest, RotateImage) {
  const int src_w = 6, src_h = 4;
  SkBitmap src;
  // Create a simple 4 color bitmap:
  // RRRBBB
  // RRRBBB
  // GGGYYY
  // GGGYYY
  src.allocN32Pixels(src_w, src_h);

  SkCanvas canvas(src, SkSurfaceProps{});
  src.eraseARGB(0, 0, 0, 0);

  // This region is a semi-transparent red to test non-opaque pixels.
  DrawRectWithColor(&canvas, 0, 0, src_w / 2, src_h / 2, 0x1FFF0000);
  DrawRectWithColor(&canvas, src_w / 2, 0, src_w, src_h / 2, SK_ColorBLUE);
  DrawRectWithColor(&canvas, 0, src_h / 2, src_w / 2, src_h, SK_ColorGREEN);
  DrawRectWithColor(&canvas, src_w / 2, src_h / 2, src_w, src_h,
                    SK_ColorYELLOW);

  SkBitmap rotate90, rotate180, rotate270;
  rotate90 = SkBitmapOperations::Rotate(src,
                                        SkBitmapOperations::ROTATION_90_CW);
  rotate180 = SkBitmapOperations::Rotate(src,
                                         SkBitmapOperations::ROTATION_180_CW);
  rotate270 = SkBitmapOperations::Rotate(src,
                                         SkBitmapOperations::ROTATION_270_CW);

  ASSERT_EQ(rotate90.width(), src.height());
  ASSERT_EQ(rotate90.height(), src.width());
  ASSERT_EQ(rotate180.width(), src.width());
  ASSERT_EQ(rotate180.height(), src.height());
  ASSERT_EQ(rotate270.width(), src.height());
  ASSERT_EQ(rotate270.height(), src.width());

  for (int x=0; x < src_w; ++x) {
    for (int y=0; y < src_h; ++y) {
      ASSERT_EQ(*src.getAddr32(x,y), *rotate90.getAddr32(src_h - (y+1),x));
      ASSERT_EQ(*src.getAddr32(x,y), *rotate270.getAddr32(y, src_w - (x+1)));
      ASSERT_EQ(*src.getAddr32(x,y),
                *rotate180.getAddr32(src_w - (x+1), src_h - (y+1)));
    }
  }
}
