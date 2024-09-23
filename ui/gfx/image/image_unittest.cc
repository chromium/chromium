// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>
#include <vector>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_IOS)
#include "base/apple/foundation_util.h"
#include "skia/ext/skia_utils_ios.h"
#elif BUILDFLAG(IS_MAC)
#include <CoreGraphics/CoreGraphics.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace {

#if BUILDFLAG(IS_APPLE)
constexpr bool kUsesSkiaNatively = false;
#else
constexpr bool kUsesSkiaNatively = true;
#endif

#if BUILDFLAG(IS_MAC)
bool IsSystemColorSpaceSRGB() {
  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGDisplayCopyColorSpace(CGMainDisplayID()));
  base::apple::ScopedCFTypeRef<CFStringRef> name(
      CGColorSpaceCopyName(color_space.get()));
  return name &&
         CFStringCompare(name.get(), kCGColorSpaceSRGB, 0) == kCFCompareEqualTo;
}
#endif  // BUILDFLAG(IS_MAC)

class ImageTest : public testing::Test {
 public:
  ImageTest() = default;
  ImageTest(const ImageTest&) = delete;
  ImageTest& operator=(const ImageTest&) = delete;
  ~ImageTest() override = default;

 private:
  ui::test::ScopedSetSupportedResourceScaleFactors
      scoped_set_supported_scale_factors_{{ui::k100Percent,
#if !BUILDFLAG(IS_IOS)
                                           ui::k200Percent
#endif
      }};
};

namespace gt = gfx::test;

TEST_F(ImageTest, EmptyImage) {
  // Test the default constructor.
  gfx::Image image;
  EXPECT_EQ(0U, image.RepresentationCount());
  EXPECT_TRUE(image.IsEmpty());
  EXPECT_EQ(0, image.Width());
  EXPECT_EQ(0, image.Height());
}

// Test constructing a gfx::Image from an empty PlatformImage.
TEST_F(ImageTest, EmptyImageFromEmptyPlatformImage) {
#if BUILDFLAG(IS_APPLE)
  gfx::Image image1(nullptr);
  EXPECT_TRUE(image1.IsEmpty());
  EXPECT_EQ(0, image1.Width());
  EXPECT_EQ(0, image1.Height());
  EXPECT_EQ(0U, image1.RepresentationCount());
#endif

  // gfx::ImageSkia and gfx::ImagePNGRep are available on all platforms.
  gfx::ImageSkia image_skia;
  EXPECT_TRUE(image_skia.isNull());
  gfx::Image image2(image_skia);
  EXPECT_TRUE(image2.IsEmpty());
  EXPECT_EQ(0, image2.Width());
  EXPECT_EQ(0, image2.Height());
  EXPECT_EQ(0U, image2.RepresentationCount());

  std::vector<gfx::ImagePNGRep> image_png_reps;
  gfx::Image image3(image_png_reps);
  EXPECT_TRUE(image3.IsEmpty());
  EXPECT_EQ(0, image3.Width());
  EXPECT_EQ(0, image3.Height());
  EXPECT_EQ(0U, image3.RepresentationCount());
}

// The resulting Image should be empty when it is created using obviously
// invalid data.
TEST_F(ImageTest, EmptyImageFromObviouslyInvalidPNGImage) {
  std::vector<gfx::ImagePNGRep> image_png_reps1;
  image_png_reps1.push_back(gfx::ImagePNGRep(nullptr, 1.0f));
  gfx::Image image1(image_png_reps1);
  EXPECT_TRUE(image1.IsEmpty());
  EXPECT_EQ(0U, image1.RepresentationCount());

  std::vector<gfx::ImagePNGRep> image_png_reps2;
  image_png_reps2.push_back(gfx::ImagePNGRep(
      new base::RefCountedBytes(), 1.0f));
  gfx::Image image2(image_png_reps2);
  EXPECT_TRUE(image2.IsEmpty());
  EXPECT_EQ(0U, image2.RepresentationCount());
}

// Test the Width, Height and Size of an empty and non-empty image.
TEST_F(ImageTest, ImageSize) {
  gfx::Image image;
  EXPECT_EQ(0, image.Width());
  EXPECT_EQ(0, image.Height());
  EXPECT_EQ(gfx::Size(0, 0), image.Size());

  gfx::Image image2(gt::CreateImageSkia(10, 25));
  EXPECT_EQ(10, image2.Width());
  EXPECT_EQ(25, image2.Height());
  EXPECT_EQ(gfx::Size(10, 25), image2.Size());
}

TEST_F(ImageTest, SkiaToSkia) {
  gfx::Image image(gt::CreateImageSkia(25, 25));
  EXPECT_EQ(25, image.Width());
  EXPECT_EQ(25, image.Height());

  // Test ToImageSkia().
  const gfx::ImageSkia* image_skia1 = image.ToImageSkia();
  EXPECT_TRUE(image_skia1);
  EXPECT_FALSE(image_skia1->isNull());
  EXPECT_EQ(1U, image.RepresentationCount());

  // Make sure double conversion doesn't happen.
  const gfx::ImageSkia* image_skia2 = image.ToImageSkia();
  EXPECT_EQ(1U, image.RepresentationCount());

  // ToImageSkia() should always return the same gfx::ImageSkia.
  EXPECT_EQ(image_skia1, image_skia2);

  // Test ToSkBitmap().
  const SkBitmap* bitmap1 = image.ToSkBitmap();
  const SkBitmap* bitmap2 = image.ToSkBitmap();
  EXPECT_TRUE(bitmap1);
  EXPECT_FALSE(bitmap1->isNull());
  EXPECT_EQ(bitmap1, bitmap2);

  EXPECT_EQ(1U, image.RepresentationCount());
  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepSkia));
  if (!kUsesSkiaNatively)
    EXPECT_FALSE(image.HasRepresentation(gt::GetPlatformRepresentationType()));
}

TEST_F(ImageTest, EmptyImageToPNG) {
  gfx::Image image;
  scoped_refptr<base::RefCountedMemory> png_bytes = image.As1xPNGBytes();
  EXPECT_TRUE(png_bytes.get());
  EXPECT_FALSE(png_bytes->size());
}

// Check that getting the 1x PNG bytes from images which do not have a 1x
// representation returns null.
TEST_F(ImageTest, ImageNo1xToPNG) {
  // Image with 2x only.
  constexpr int kSize2x = 50;
  gfx::ImageSkia image_skia;
  image_skia.AddRepresentation(gfx::ImageSkiaRep(gt::CreateBitmap(
      kSize2x, kSize2x), 2.0f));
  gfx::Image image1(image_skia);
  scoped_refptr<base::RefCountedMemory> png_bytes1 = image1.As1xPNGBytes();
  EXPECT_TRUE(png_bytes1.get());
  EXPECT_FALSE(png_bytes1->size());

  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(gfx::ImagePNGRep(
      gt::CreatePNGBytes(kSize2x), 2.0f));
  gfx::Image image2(image_png_reps);
  EXPECT_FALSE(image2.IsEmpty());
  EXPECT_EQ(0, image2.Width());
  EXPECT_EQ(0, image2.Height());
  scoped_refptr<base::RefCountedMemory> png_bytes2 = image2.As1xPNGBytes();
  EXPECT_TRUE(png_bytes2.get());
  EXPECT_FALSE(png_bytes2->size());
}

// Check that for an image initialized with multi resolution PNG data,
// As1xPNGBytes() returns the 1x bytes.
TEST_F(ImageTest, CreateExtractPNGBytes) {
  constexpr int kSize1x = 25;
  constexpr int kSize2x = 50;

  scoped_refptr<base::RefCountedMemory> bytes1x = gt::CreatePNGBytes(kSize1x);
  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.emplace_back(bytes1x, 1.0f);
  image_png_reps.emplace_back(gt::CreatePNGBytes(kSize2x), 2.0f);

  gfx::Image image(image_png_reps);
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(25, image.Width());
  EXPECT_EQ(25, image.Height());

  EXPECT_TRUE(std::ranges::equal(base::span(*bytes1x),
                                 base::span(*image.As1xPNGBytes())));
}

TEST_F(ImageTest, MultiResolutionImageSkiaToPNG) {
  constexpr int kSize1x = 25;
  constexpr int kSize2x = 50;

  SkBitmap bitmap_1x = gt::CreateBitmap(kSize1x, kSize1x);
  gfx::ImageSkia image_skia;
  image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap_1x,
                                                 1.0f));
  image_skia.AddRepresentation(gfx::ImageSkiaRep(gt::CreateBitmap(
      kSize2x, kSize2x), 2.0f));
  gfx::Image image(image_skia);

  EXPECT_TRUE(
      gt::ArePNGBytesCloseToBitmap(*image.As1xPNGBytes(), bitmap_1x,
                                   gt::MaxColorSpaceConversionColorShift()));
  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepPNG));
}

TEST_F(ImageTest, MultiResolutionPNGToImageSkia) {
  constexpr int kSize1x = 25;
  constexpr int kSize2x = 50;

  scoped_refptr<base::RefCountedMemory> bytes1x = gt::CreatePNGBytes(kSize1x);
  scoped_refptr<base::RefCountedMemory> bytes2x = gt::CreatePNGBytes(kSize2x);

  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(gfx::ImagePNGRep(bytes1x, 1.0f));
  image_png_reps.push_back(gfx::ImagePNGRep(bytes2x, 2.0f));
  gfx::Image image(image_png_reps);

  std::vector<float> scales;
  scales.push_back(1.0f);
  scales.push_back(2.0f);
  gfx::ImageSkia image_skia = image.AsImageSkia();
  EXPECT_TRUE(gt::ArePNGBytesCloseToBitmap(
      *bytes1x, image_skia.GetRepresentation(1.0f).GetBitmap(),
      gt::MaxColorSpaceConversionColorShift()));
  EXPECT_TRUE(gt::ArePNGBytesCloseToBitmap(
      *bytes2x, image_skia.GetRepresentation(2.0f).GetBitmap(),
      gt::MaxColorSpaceConversionColorShift()));
  EXPECT_TRUE(gt::ImageSkiaStructureMatches(image_skia, kSize1x, kSize1x,
                                            scales));
#if !BUILDFLAG(IS_IOS)
  // IOS does not support arbitrary scale factors.
  gfx::ImageSkiaRep rep_1_6x = image_skia.GetRepresentation(1.6f);
  ASSERT_FALSE(rep_1_6x.is_null());
  ASSERT_EQ(1.6f, rep_1_6x.scale());
  EXPECT_EQ("40x40", rep_1_6x.pixel_size().ToString());

  gfx::ImageSkiaRep rep_0_8x = image_skia.GetRepresentation(0.8f);
  ASSERT_FALSE(rep_0_8x.is_null());
  ASSERT_EQ(0.8f, rep_0_8x.scale());
  EXPECT_EQ("20x20", rep_0_8x.pixel_size().ToString());
#endif
}

#if !BUILDFLAG(IS_IOS)
// IOS does not support arbitrary scale factors.
TEST_F(ImageTest, PreferDownscaleToUpscale) {
  constexpr int kSize1x = 25;
  constexpr int kSize4x = 100;

  scoped_refptr<base::RefCountedMemory> bytes1x =
      gt::CreatePNGBytes(kSize1x, SK_ColorGREEN);
  scoped_refptr<base::RefCountedMemory> bytes4x =
      gt::CreatePNGBytes(kSize4x, SK_ColorBLUE);

  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.emplace_back(bytes1x, 1.0f);
  image_png_reps.emplace_back(bytes4x, 4.0f);
  gfx::Image image(image_png_reps);

  gfx::ImageSkia image_skia = image.AsImageSkia();

  // Make sure that the 1x, 4x representations are stored.
  image_skia.GetRepresentation(1.0f);
  image_skia.GetRepresentation(4.0f);

  // Make sure that a 1.6x representation is created by downscaling the 4x
  // representation, not by upscaling the 1x representation.
  gfx::ImageSkiaRep rep_1_6x = image_skia.GetRepresentation(1.6f);
  EXPECT_EQ(SK_ColorBLUE, rep_1_6x.GetBitmap().getColor(0, 0));

  // Make sure that a 0.8x representation is created by downscaling the 1x
  // representation.
  gfx::ImageSkiaRep rep_0_8x = image_skia.GetRepresentation(0.8f);
  EXPECT_EQ(SK_ColorGREEN, rep_0_8x.GetBitmap().getColor(0, 0));

  // Make sure that a 8x representation is created by upscaling the 4x
  // representation.
  gfx::ImageSkiaRep rep_8x = image_skia.GetRepresentation(8.0f);
  EXPECT_EQ(SK_ColorBLUE, rep_8x.GetBitmap().getColor(0, 0));
}
#endif

TEST_F(ImageTest, MultiResolutionPNGToPlatform) {
  constexpr int kSize1x = 25;
  constexpr int kSize2x = 50;

  scoped_refptr<base::RefCountedMemory> bytes1x = gt::CreatePNGBytes(kSize1x);
  scoped_refptr<base::RefCountedMemory> bytes2x = gt::CreatePNGBytes(kSize2x);
  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(gfx::ImagePNGRep(bytes1x, 1.0f));
  image_png_reps.push_back(gfx::ImagePNGRep(bytes2x, 2.0f));

  gfx::Image from_png(image_png_reps);
  gfx::Image from_platform(gt::CopyViaPlatformType(from_png));
#if BUILDFLAG(IS_IOS)
  // On iOS the platform type (UIImage) only supports one resolution.
  const std::vector<ui::ResourceScaleFactor>& scales =
      ui::GetSupportedResourceScaleFactors();
  EXPECT_EQ(scales.size(), 1U);
  if (scales[0] == ui::k100Percent) {
    EXPECT_TRUE(
        gt::ArePNGBytesCloseToBitmap(*bytes1x, from_platform.AsBitmap(),
                                     gt::MaxColorSpaceConversionColorShift()));
  } else if (scales[0] == ui::k200Percent) {
    EXPECT_TRUE(
        gt::ArePNGBytesCloseToBitmap(*bytes2x, from_platform.AsBitmap(),
                                     gt::MaxColorSpaceConversionColorShift()));
  } else {
    ADD_FAILURE() << "Unexpected platform scale factor.";
  }
#else
  EXPECT_TRUE(
      gt::ArePNGBytesCloseToBitmap(*bytes1x, from_platform.AsBitmap(),
                                   gt::MaxColorSpaceConversionColorShift()));
#endif  // BUILDFLAG(IS_IOS)
}


TEST_F(ImageTest, PlatformToPNGEncodeAndDecode) {
  gfx::Image image(gt::CreatePlatformImage());
  scoped_refptr<base::RefCountedMemory> png_data = image.As1xPNGBytes();
  EXPECT_TRUE(png_data.get());
  EXPECT_TRUE(png_data->size());
  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepPNG));

  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(gfx::ImagePNGRep(png_data, 1.0f));
  gfx::Image from_png(image_png_reps);

  EXPECT_TRUE(from_png.HasRepresentation(gfx::Image::kImageRepPNG));
  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(from_png)));
}

// The platform types use the platform provided encoding/decoding of PNGs. Make
// sure these work with the Skia Encode/Decode.
TEST_F(ImageTest, PNGEncodeFromSkiaDecodeToPlatform) {
  // Force the conversion sequence skia to png to platform_type.
  gfx::Image from_bitmap = gfx::Image::CreateFrom1xBitmap(
      gt::CreateBitmap(25, 25));
  scoped_refptr<base::RefCountedMemory> png_bytes =
      from_bitmap.As1xPNGBytes();

  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(gfx::ImagePNGRep(png_bytes, 1.0f));
  gfx::Image from_png(image_png_reps);

  gfx::Image from_platform(gt::CopyViaPlatformType(from_png));

  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(from_platform)));
  EXPECT_TRUE(
      gt::ArePNGBytesCloseToBitmap(*png_bytes, from_platform.AsBitmap(),
                                   gt::MaxColorSpaceConversionColorShift()));
}

TEST_F(ImageTest, PNGEncodeFromPlatformDecodeToSkia) {
  // Force the conversion sequence platform_type to png to skia.
  gfx::Image from_platform(gt::CreatePlatformImage());
  scoped_refptr<base::RefCountedMemory> png_bytes =
      from_platform.As1xPNGBytes();
  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(gfx::ImagePNGRep(png_bytes, 1.0f));
  gfx::Image from_png(image_png_reps);

  EXPECT_TRUE(gt::AreBitmapsClose(
      from_platform.AsBitmap(), from_png.AsBitmap(),
      gt::MaxColorSpaceConversionColorShift()));
}

TEST_F(ImageTest, PNGDecodeToSkiaFailure) {
  scoped_refptr<base::RefCountedBytes> invalid_bytes(
      new base::RefCountedBytes());
  invalid_bytes->as_vector().push_back('0');
  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(gfx::ImagePNGRep(
      invalid_bytes, 1.0f));
  gfx::Image image(image_png_reps);
  gt::CheckImageIndicatesPNGDecodeFailure(image);
}

TEST_F(ImageTest, PNGDecodeToPlatformFailure) {
  scoped_refptr<base::RefCountedBytes> invalid_bytes(
      new base::RefCountedBytes());
  invalid_bytes->as_vector().push_back('0');
  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(gfx::ImagePNGRep(
      invalid_bytes, 1.0f));
  gfx::Image from_png(image_png_reps);
  gfx::Image from_platform(gt::CopyViaPlatformType(from_png));
  gt::CheckImageIndicatesPNGDecodeFailure(from_platform);
}

TEST_F(ImageTest, SkiaToPlatform) {
  gfx::Image image(gt::CreateImageSkia(25, 25));
  EXPECT_EQ(25, image.Width());
  EXPECT_EQ(25, image.Height());
  constexpr size_t kRepCount = kUsesSkiaNatively ? 1U : 2U;

  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepSkia));
  if (!kUsesSkiaNatively)
    EXPECT_FALSE(image.HasRepresentation(gt::GetPlatformRepresentationType()));

  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image)));
  EXPECT_EQ(kRepCount, image.RepresentationCount());

  const SkBitmap* bitmap = image.ToSkBitmap();
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(kRepCount, image.RepresentationCount());

  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepSkia));
  EXPECT_TRUE(image.HasRepresentation(gt::GetPlatformRepresentationType()));
  EXPECT_EQ(25, image.Width());
  EXPECT_EQ(25, image.Height());
}

TEST_F(ImageTest, PlatformToSkia) {
  gfx::Image image(gt::CreatePlatformImage());
  EXPECT_EQ(25, image.Width());
  EXPECT_EQ(25, image.Height());
  constexpr size_t kRepCount = kUsesSkiaNatively ? 1U : 2U;

  EXPECT_TRUE(image.HasRepresentation(gt::GetPlatformRepresentationType()));
  if (!kUsesSkiaNatively)
    EXPECT_FALSE(image.HasRepresentation(gfx::Image::kImageRepSkia));

  const SkBitmap* bitmap = image.ToSkBitmap();
  EXPECT_TRUE(bitmap);
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(kRepCount, image.RepresentationCount());

  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image)));
  EXPECT_EQ(kRepCount, image.RepresentationCount());

  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepSkia));
  EXPECT_EQ(25, image.Width());
  EXPECT_EQ(25, image.Height());
}

TEST_F(ImageTest, PlatformToPlatform) {
  gfx::Image image(gt::CreatePlatformImage());
  EXPECT_EQ(25, image.Width());
  EXPECT_EQ(25, image.Height());
  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image)));
  EXPECT_EQ(1U, image.RepresentationCount());

  // Make sure double conversion doesn't happen.
  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image)));
  EXPECT_EQ(1U, image.RepresentationCount());

  EXPECT_TRUE(image.HasRepresentation(gt::GetPlatformRepresentationType()));
  if (!kUsesSkiaNatively)
    EXPECT_FALSE(image.HasRepresentation(gfx::Image::kImageRepSkia));
  EXPECT_EQ(25, image.Width());
  EXPECT_EQ(25, image.Height());
}

TEST_F(ImageTest, CheckSkiaColor) {
  gfx::Image image(gt::CreatePlatformImage());

  const SkBitmap* bitmap = image.ToSkBitmap();
  gt::CheckColors(bitmap->getColor(10, 10), SK_ColorGREEN);
}

TEST_F(ImageTest, SkBitmapConversionPreservesOrientation) {
#if BUILDFLAG(IS_MAC)
  LOG_IF(WARNING, !IsSystemColorSpaceSRGB())
      << "This test is designed to pass with the sRGB color space, which is "
         "not set for your main display currently. Thus, colors can be off by "
         "too big a margin, and the test can fail.";
#endif  // BUILDFLAG(IS_MAC)

  constexpr int width = 50;
  constexpr int height = 50;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseARGB(255, 0, 255, 0);

  // Paint the upper half of the image in red (lower half is in green).
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  SkPaint red;
  red.setColor(SK_ColorRED);
  canvas.drawRect(SkRect::MakeWH(width, height / 2), red);
  {
    SCOPED_TRACE("Checking color of the initial SkBitmap");
    gt::CheckColors(bitmap.getColor(10, 10), SK_ColorRED);
    gt::CheckColors(bitmap.getColor(10, 40), SK_ColorGREEN);
  }

  // Convert from SkBitmap to a platform representation, then check the upper
  // half of the platform image to make sure it is red, not green.
  gfx::Image from_skbitmap = gfx::Image::CreateFrom1xBitmap(bitmap);
  {
    SCOPED_TRACE("Checking color of the platform image");
    gt::CheckColors(
        gt::GetPlatformImageColor(gt::ToPlatformType(from_skbitmap), 10, 10),
        SK_ColorRED);
    gt::CheckColors(
        gt::GetPlatformImageColor(gt::ToPlatformType(from_skbitmap), 10, 40),
        SK_ColorGREEN);
  }

  // Force a conversion back to SkBitmap and check that the upper half is red.
  gfx::Image from_platform(gt::CopyViaPlatformType(from_skbitmap));
  const SkBitmap* bitmap2 = from_platform.ToSkBitmap();
  {
    SCOPED_TRACE("Checking color after conversion back to SkBitmap");
    gt::CheckColors(bitmap2->getColor(10, 10), SK_ColorRED);
    gt::CheckColors(bitmap2->getColor(10, 40), SK_ColorGREEN);
  }
}

TEST_F(ImageTest, SkBitmapConversionPreservesTransparency) {
  constexpr int width = 50;
  constexpr int height = 50;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseARGB(0, 0, 255, 0);

  // Paint the upper half of the image in red (lower half is transparent).
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  SkPaint red;
  red.setColor(SK_ColorRED);
  canvas.drawRect(SkRect::MakeWH(width, height / 2), red);
  {
    SCOPED_TRACE("Checking color of the initial SkBitmap");
    gt::CheckColors(bitmap.getColor(10, 10), SK_ColorRED);
    gt::CheckIsTransparent(bitmap.getColor(10, 40));
  }

  // Convert from SkBitmap to a platform representation, then check the upper
  // half of the platform image to make sure it is red, not green.
  gfx::Image from_skbitmap = gfx::Image::CreateFrom1xBitmap(bitmap);
  {
    SCOPED_TRACE("Checking color of the platform image");
    gt::CheckColors(
        gt::GetPlatformImageColor(gt::ToPlatformType(from_skbitmap), 10, 10),
        SK_ColorRED);
    gt::CheckIsTransparent(
        gt::GetPlatformImageColor(gt::ToPlatformType(from_skbitmap), 10, 40));
  }

  // Force a conversion back to SkBitmap and check that the upper half is red.
  gfx::Image from_platform(gt::CopyViaPlatformType(from_skbitmap));
  const SkBitmap* bitmap2 = from_platform.ToSkBitmap();
  {
    SCOPED_TRACE("Checking color after conversion back to SkBitmap");
    gt::CheckColors(bitmap2->getColor(10, 10), SK_ColorRED);
    gt::CheckIsTransparent(bitmap.getColor(10, 40));
  }
}

TEST_F(ImageTest, Copy) {
  constexpr size_t kRepCount = kUsesSkiaNatively ? 1U : 2U;

  gfx::Image image1(gt::CreateImageSkia(25, 25));
  EXPECT_EQ(25, image1.Width());
  EXPECT_EQ(25, image1.Height());
  gfx::Image image2(image1);
  EXPECT_EQ(25, image2.Width());
  EXPECT_EQ(25, image2.Height());

  EXPECT_EQ(1U, image1.RepresentationCount());
  EXPECT_EQ(1U, image2.RepresentationCount());
  EXPECT_EQ(image1.ToImageSkia(), image2.ToImageSkia());

  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image2)));
  EXPECT_EQ(kRepCount, image2.RepresentationCount());
  EXPECT_EQ(kRepCount, image1.RepresentationCount());
}

TEST_F(ImageTest, Assign) {
  gfx::Image image1(gt::CreatePlatformImage());
  EXPECT_EQ(25, image1.Width());
  EXPECT_EQ(25, image1.Height());
  // Assignment must be on a separate line to the declaration in order to test
  // assignment operator (instead of copy constructor).
  gfx::Image image2;
  image2 = image1;
  EXPECT_EQ(25, image2.Width());
  EXPECT_EQ(25, image2.Height());

  EXPECT_EQ(1U, image1.RepresentationCount());
  EXPECT_EQ(1U, image2.RepresentationCount());
  EXPECT_EQ(image1.ToSkBitmap(), image2.ToSkBitmap());
}

TEST_F(ImageTest, Move) {
  constexpr size_t kRepCount = kUsesSkiaNatively ? 1U : 2U;

  gfx::Image image1(gt::CreateImageSkia(25, 25));
  EXPECT_EQ(25, image1.Width());
  EXPECT_EQ(25, image1.Height());
  gfx::Image image2(std::move(image1));
  EXPECT_EQ(25, image2.Width());
  EXPECT_EQ(25, image2.Height());

  EXPECT_EQ(0U, image1.RepresentationCount());
  EXPECT_EQ(1U, image2.RepresentationCount());

  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image2)));
  EXPECT_EQ(0U, image1.RepresentationCount());
  EXPECT_EQ(kRepCount, image2.RepresentationCount());
}

TEST_F(ImageTest, MoveAssign) {
  gfx::Image image1(gt::CreatePlatformImage());
  EXPECT_EQ(25, image1.Width());
  EXPECT_EQ(25, image1.Height());
  // Assignment must be on a separate line to the declaration in order to test
  // move assignment operator (instead of move constructor).
  gfx::Image image2;
  image2 = std::move(image1);
  EXPECT_EQ(25, image2.Width());
  EXPECT_EQ(25, image2.Height());

  EXPECT_EQ(0U, image1.RepresentationCount());
  EXPECT_EQ(1U, image2.RepresentationCount());
}

TEST_F(ImageTest, Copy_PreservesRepresentation) {
  constexpr gfx::Size kSize1x(25, 25);
  constexpr gfx::Size kSize2x(50, 50);
  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(
      gfx::ImagePNGRep(gt::CreatePNGBytes(kSize1x.width()), 1.0f));
  image_png_reps.push_back(
      gfx::ImagePNGRep(gt::CreatePNGBytes(kSize2x.width()), 2.0f));
  gfx::Image image(image_png_reps);

  gfx::ImageSkia image_skia = image.AsImageSkia();
  EXPECT_EQ(kSize1x, image_skia.size());
  SkISize size = image_skia.GetRepresentation(2.0f).GetBitmap().dimensions();
  EXPECT_EQ(kSize2x, gfx::Size(size.fWidth, size.fHeight));

  gfx::Image image2(image);
  gfx::ImageSkia image_skia2 = image2.AsImageSkia();
  EXPECT_EQ(kSize1x, image_skia2.size());
  size = image_skia2.GetRepresentation(2.0f).GetBitmap().dimensions();
  EXPECT_EQ(kSize2x, gfx::Size(size.fWidth, size.fHeight));

  EXPECT_TRUE(image_skia.BackedBySameObjectAs(image_skia2));
}

TEST_F(ImageTest, Copy_PreventsDuplication) {
  constexpr gfx::Size kSize1x(25, 25);
  constexpr gfx::Size kSize2x(50, 50);
  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(
      gfx::ImagePNGRep(gt::CreatePNGBytes(kSize1x.width()), 1.0f));
  image_png_reps.push_back(
      gfx::ImagePNGRep(gt::CreatePNGBytes(kSize2x.width()), 2.0f));
  gfx::Image image(image_png_reps);

  gfx::Image image2(image);

  gfx::ImageSkia image_skia = image.AsImageSkia();
  EXPECT_EQ(kSize1x, image_skia.size());
  SkISize size = image_skia.GetRepresentation(2.0f).GetBitmap().dimensions();
  EXPECT_EQ(kSize2x, gfx::Size(size.fWidth, size.fHeight));

  gfx::ImageSkia image_skia2 = image2.AsImageSkia();
  EXPECT_EQ(kSize1x, image_skia2.size());
  size = image_skia2.GetRepresentation(2.0f).GetBitmap().dimensions();
  EXPECT_EQ(kSize2x, gfx::Size(size.fWidth, size.fHeight));

  EXPECT_TRUE(image_skia.BackedBySameObjectAs(image_skia2));
}

TEST_F(ImageTest, Copy_PreservesBackingStore) {
  constexpr gfx::Size kSize1x(25, 25);

  gfx::Image image(gt::CreateImageSkia(kSize1x.width(), kSize1x.height()));
  gfx::Image image2 = gfx::Image::CreateFrom1xBitmap(image.AsBitmap());

  gfx::ImageSkia image_skia = image.AsImageSkia();
  gfx::ImageSkia image_skia2 = image2.AsImageSkia();

  // Because we haven't copied the image representation (scale info, etc.) the
  // new Skia image isn't backed by the same object, but it should still contain
  // the same bitmap data.
  EXPECT_FALSE(image_skia2.BackedBySameObjectAs(image_skia));
  EXPECT_EQ(image_skia.bitmap()->getPixels(),
            image_skia2.bitmap()->getPixels());
}

TEST_F(ImageTest, MultiResolutionImageSkia) {
  constexpr int kWidth1x = 10;
  constexpr int kHeight1x = 12;
  constexpr int kWidth2x = 20;
  constexpr int kHeight2x = 24;

  gfx::ImageSkia image_skia;
  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      gt::CreateBitmap(kWidth1x, kHeight1x),
      1.0f));
  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      gt::CreateBitmap(kWidth2x, kHeight2x),
      2.0f));

  std::vector<float> scales;
  scales.push_back(1.0f);
  scales.push_back(2.0f);
  EXPECT_TRUE(gt::ImageSkiaStructureMatches(image_skia, kWidth1x, kHeight1x,
                                            scales));

  // Check that the image has a single representation.
  gfx::Image image(image_skia);
  EXPECT_EQ(1u, image.RepresentationCount());
  EXPECT_EQ(kWidth1x, image.Width());
  EXPECT_EQ(kHeight1x, image.Height());
}

TEST_F(ImageTest, RemoveFromMultiResolutionImageSkia) {
  constexpr int kWidth2x = 20;
  constexpr int kHeight2x = 24;

  gfx::ImageSkia image_skia;

  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      gt::CreateBitmap(kWidth2x, kHeight2x), 2.0f));
  EXPECT_EQ(1u, image_skia.image_reps().size());

  image_skia.RemoveRepresentation(1.0f);
  EXPECT_EQ(1u, image_skia.image_reps().size());

  image_skia.RemoveRepresentation(2.0f);
  EXPECT_EQ(0u, image_skia.image_reps().size());
}

// Tests that gfx::Image does indeed take ownership of the SkBitmap it is
// passed.
TEST_F(ImageTest, OwnershipTest) {
  gfx::Image image;
  {
    SkBitmap bitmap(gt::CreateBitmap(10, 10));
    EXPECT_TRUE(!bitmap.isNull());
    image = gfx::Image(gfx::ImageSkia(
        gfx::ImageSkiaRep(bitmap, 1.0f)));
  }
  EXPECT_TRUE(!image.ToSkBitmap()->isNull());
}

// Integration tests with UI toolkit frameworks require linking against the
// Views library and cannot be here (ui_base_unittests doesn't include it). They
// instead live in /chrome/browser/ui/tests/ui_gfx_image_unittest.cc.

}  // namespace
