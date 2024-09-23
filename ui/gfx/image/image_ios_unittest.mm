// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/image/image.h"

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#include <stddef.h>

#include "base/apple/scoped_cftyperef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// Helper function to return a UIImage with the given size and scale.
UIImage* UIImageWithSizeAndScale(CGFloat width, CGFloat height, CGFloat scale) {
  CGSize target_size = CGSizeMake(width * scale, height * scale);

  // Create a UIImage directly from a CGImage in order to control the exact
  // pixel size of the underlying image.
  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  base::apple::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      NULL, target_size.width, target_size.height, 8, target_size.width * 4,
      color_space.get(),
      kCGImageAlphaPremultipliedFirst |
          static_cast<CGImageAlphaInfo>(kCGBitmapByteOrder32Host)));

  CGRect target_rect = CGRectMake(0, 0,
                                  target_size.width, target_size.height);
  CGContextSetFillColorWithColor(context.get(), [[UIColor redColor] CGColor]);
  CGContextFillRect(context.get(), target_rect);

  base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
      CGBitmapContextCreateImage(context.get()));
  return [UIImage imageWithCGImage:cg_image.get()
                             scale:scale
                       orientation:UIImageOrientationUp];
}


class ImageIOSTest : public testing::Test {
 public:
  ImageIOSTest() = default;
  ImageIOSTest(const ImageIOSTest&) = delete;
  ImageIOSTest& operator=(const ImageIOSTest&) = delete;
  ~ImageIOSTest() override = default;
};

// Tests image conversion when the scale factor of the source image is not in
// the list of supported scale factors.
TEST_F(ImageIOSTest, ImageConversionWithUnsupportedScaleFactor) {
  const CGFloat kWidth = 200;
  const CGFloat kHeight = 100;
  const ui::ResourceScaleFactor kTestScales[3] = {
      ui::k100Percent, ui::k200Percent, ui::k300Percent};

  for (size_t i = 0; i < std::size(kTestScales); ++i) {
    for (size_t j = 0; j < std::size(kTestScales); ++j) {
      const CGFloat source_scale = kTestScales[i];
      const ui::ResourceScaleFactor supported_scale = kTestScales[j];

      // Set the supported scale for testing.
      ui::test::ScopedSetSupportedResourceScaleFactors scoped_scale_factors(
          {supported_scale});

      // Create an UIImage with the appropriate source_scale.
      UIImage* ui_image =
          UIImageWithSizeAndScale(kWidth, kHeight, source_scale);
      ASSERT_EQ(kWidth, ui_image.size.width);
      ASSERT_EQ(kHeight, ui_image.size.height);
      ASSERT_EQ(source_scale, ui_image.scale);

      // Convert to SkBitmap and test its size.
      gfx::Image to_skbitmap(ui_image);
      const SkBitmap* bitmap = to_skbitmap.ToSkBitmap();
      ASSERT_TRUE(bitmap != NULL);
      EXPECT_EQ(kWidth * ui::GetScaleForResourceScaleFactor(supported_scale),
                bitmap->width());
      EXPECT_EQ(kHeight * ui::GetScaleForResourceScaleFactor(supported_scale),
                bitmap->height());

      // Convert to ImageSkia and test its size.
      gfx::Image to_imageskia(ui_image);
      const gfx::ImageSkia* imageskia = to_imageskia.ToImageSkia();
      EXPECT_EQ(kWidth, imageskia->width());
      EXPECT_EQ(kHeight, imageskia->height());

      // TODO(rohitrao): Convert from ImageSkia back to UIImage.  This should
      // scale the image based on the current set of supported scales.
    }
  }
}

} // namespace
