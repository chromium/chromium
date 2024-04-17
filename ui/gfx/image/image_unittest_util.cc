// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Because the unit tests for gfx::Image are spread across multiple
// implementation files, this header contains the reusable components.

#include "ui/gfx/image/image_unittest_util.h"

#include <stddef.h>

#include <cmath>
#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/test/sk_color_eq.h"

#if BUILDFLAG(IS_IOS)
#include "base/apple/scoped_cftyperef.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/resource/resource_scale_factor.h"
#elif BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace gfx {
namespace test {

namespace {

// The maximum color shift in the red, green, and blue components caused by
// converting a gfx::Image between colorspaces. Color shifts occur when
// converting between NSImage & UIImage to ImageSkia. Determined by trial and
// error.
constexpr int kMaxColorSpaceConversionColorShift = 40;

}  // namespace

const SkBitmap CreateBitmap(int size, SkColor color) {
  return CreateBitmap(size, size, color);
}

const SkBitmap CreateBitmap(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return bitmap;
}

gfx::ImageSkia CreateImageSkia(int size, SkColor color) {
  return CreateImageSkia(size, size, color);
}

gfx::ImageSkia CreateImageSkia(int width, int height, SkColor color) {
  return gfx::ImageSkia::CreateFrom1xBitmap(CreateBitmap(width, height, color));
}

scoped_refptr<base::RefCountedMemory> CreatePNGBytes(int edge_size,
                                                     SkColor color) {
  SkBitmap bitmap = CreateBitmap(edge_size, edge_size, color);
  scoped_refptr<base::RefCountedBytes> bytes(new base::RefCountedBytes());
  PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bytes->as_vector());
  return bytes;
}

gfx::Image CreateImage(int size, SkColor color) {
  return CreateImage(size, size, color);
}

gfx::Image CreateImage(int width, int height, SkColor color) {
  return gfx::Image::CreateFrom1xBitmap(CreateBitmap(width, height, color));
}

bool AreImagesEqual(const gfx::Image& img1, const gfx::Image& img2) {
  return AreImagesClose(img1, img2, 0);
}

bool AreImagesClose(const gfx::Image& img1,
                    const gfx::Image& img2,
                    int max_deviation) {
  img1.AsImageSkia().EnsureRepsForSupportedScales();
  img2.AsImageSkia().EnsureRepsForSupportedScales();
  std::vector<gfx::ImageSkiaRep> img1_reps = img1.AsImageSkia().image_reps();
  gfx::ImageSkia image_skia2 = img2.AsImageSkia();
  if (image_skia2.image_reps().size() != img1_reps.size())
    return false;

  for (size_t i = 0; i < img1_reps.size(); ++i) {
    float scale = img1_reps[i].scale();
    const gfx::ImageSkiaRep& image_rep2 = image_skia2.GetRepresentation(scale);
    if (image_rep2.scale() != scale ||
        !AreBitmapsClose(img1_reps[i].GetBitmap(), image_rep2.GetBitmap(),
                         max_deviation)) {
      return false;
    }
  }
  return true;
}

bool AreBitmapsEqual(const SkBitmap& bmp1, const SkBitmap& bmp2) {
  return AreBitmapsClose(bmp1, bmp2, 0);
}

bool AreBitmapsClose(const SkBitmap& bmp1,
                     const SkBitmap& bmp2,
                     int max_deviation) {
  if (bmp1.isNull() && bmp2.isNull())
    return true;

  if (bmp1.width() != bmp2.width() ||
      bmp1.height() != bmp2.height() ||
      bmp1.colorType() != kN32_SkColorType ||
      bmp2.colorType() != kN32_SkColorType) {
    return false;
  }

  if (!bmp1.getPixels() || !bmp2.getPixels())
    return false;

  for (int y = 0; y < bmp1.height(); ++y) {
    for (int x = 0; x < bmp1.width(); ++x) {
      if (!ColorsClose(bmp1.getColor(x,y), bmp2.getColor(x,y), max_deviation))
        return false;
    }
  }

  return true;
}

bool ArePNGBytesCloseToBitmap(base::span<const uint8_t> bytes,
                              const SkBitmap& bitmap,
                              int max_deviation) {
  SkBitmap decoded;
  if (!PNGCodec::Decode(bytes.data(), bytes.size(), &decoded))
    return bitmap.isNull();

  return AreBitmapsClose(bitmap, decoded, max_deviation);
}

int MaxColorSpaceConversionColorShift() {
  return kMaxColorSpaceConversionColorShift;
}

void CheckImageIndicatesPNGDecodeFailure(const gfx::Image& image) {
  SkBitmap bitmap = image.AsBitmap();
  EXPECT_FALSE(bitmap.isNull());
  EXPECT_LE(16, bitmap.width());
  EXPECT_LE(16, bitmap.height());
  EXPECT_SKCOLOR_CLOSE(bitmap.getColor(10, 10), SK_ColorRED,
                       MaxColorSpaceConversionColorShift());
}

bool ImageSkiaStructureMatches(
    const gfx::ImageSkia& image_skia,
    int width,
    int height,
    const std::vector<float>& scales) {
  if (image_skia.isNull() ||
      image_skia.width() != width ||
      image_skia.height() != height ||
      image_skia.image_reps().size() != scales.size()) {
    return false;
  }

  for (size_t i = 0; i < scales.size(); ++i) {
    gfx::ImageSkiaRep image_rep =
        image_skia.GetRepresentation(scales[i]);
    if (image_rep.is_null() || image_rep.scale() != scales[i])
      return false;

    if (image_rep.pixel_width() != static_cast<int>(width * scales[i]) ||
        image_rep.pixel_height() != static_cast<int>(height * scales[i])) {
      return false;
    }
  }
  return true;
}

bool IsEmpty(const gfx::Image& image) {
  const SkBitmap& bmp = *image.ToSkBitmap();
  return bmp.isNull() ||
         (bmp.width() == 0 && bmp.height() == 0);
}

PlatformImage CreatePlatformImage() {
  SkBitmap bitmap(CreateBitmap(25, 25));
#if BUILDFLAG(IS_IOS)
  const float scale = ui::GetScaleForMaxSupportedResourceScaleFactor();

  if (scale > 1.0) {
    // Always create a 25pt x 25pt image.
    const int size = static_cast<int>(25 * scale);
    bitmap = CreateBitmap(size, size);
  }

  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  UIImage* image =
      skia::SkBitmapToUIImageWithColorSpace(bitmap, scale, color_space.get());
  return image;
#elif BUILDFLAG(IS_MAC)
  NSImage* image = skia::SkBitmapToNSImage(bitmap);
  return image;
#else
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
#endif
}

gfx::Image::RepresentationType GetPlatformRepresentationType() {
#if BUILDFLAG(IS_IOS)
  return gfx::Image::kImageRepCocoaTouch;
#elif BUILDFLAG(IS_MAC)
  return gfx::Image::kImageRepCocoa;
#else
  return gfx::Image::kImageRepSkia;
#endif
}

PlatformImage ToPlatformType(const gfx::Image& image) {
#if BUILDFLAG(IS_IOS)
  return image.ToUIImage();
#elif BUILDFLAG(IS_MAC)
  return image.ToNSImage();
#else
  return image.AsImageSkia();
#endif
}

gfx::Image CopyViaPlatformType(const gfx::Image& image) {
#if BUILDFLAG(IS_IOS)
  return gfx::Image(image.ToUIImage());
#elif BUILDFLAG(IS_MAC)
  return gfx::Image(image.ToNSImage());
#else
  return gfx::Image(image.AsImageSkia());
#endif
}

#if BUILDFLAG(IS_APPLE)
// Defined in image_unittest_util_apple.mm.
#else
SkColor GetPlatformImageColor(PlatformImage image, int x, int y) {
  return image.bitmap()->getColor(x, y);
}
#endif

void CheckColors(SkColor color1, SkColor color2) {
  // Be tolerant of floating point rounding and lossy color space conversions.
  EXPECT_SKCOLOR_CLOSE(color1, color2, MaxColorSpaceConversionColorShift());
}

void CheckIsTransparent(SkColor color) {
  EXPECT_LT(SkColorGetA(color) / 255.0, 0.05);
}

bool IsPlatformImageValid(PlatformImage image) {
#if BUILDFLAG(IS_APPLE)
  return image != NULL;
#else
  return !image.isNull();
#endif
}

bool PlatformImagesEqual(PlatformImage image1, PlatformImage image2) {
#if BUILDFLAG(IS_APPLE)
  return image1 == image2;
#else
  return image1.BackedBySameObjectAs(image2);
#endif
}

}  // namespace test
}  // namespace gfx
