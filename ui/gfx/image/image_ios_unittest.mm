// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image.h"

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#include <stddef.h>

#include "base/mac/scoped_cftyperef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// Helper function to return a UIImage with the given size and scale.
UIImage* UIImageWithSizeAndScale(CGFloat width, CGFloat height, CGFloat scale) {
  CGSize target_size = CGSizeMake(width * scale, height * scale);

  // Create a UIImage directly from a CGImage in order to control the exact
  // pixel size of the underlying image.
  base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  base::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      NULL, target_size.width, target_size.height, 8, target_size.width * 4,
      color_space,
      kCGImageAlphaPremultipliedFirst |
          static_cast<CGImageAlphaInfo>(kCGBitmapByteOrder32Host)));

  CGRect target_rect = CGRectMake(0, 0,
                                  target_size.width, target_size.height);
  CGContextSetFillColorWithColor(context, [[UIColor redColor] CGColor]);
  CGContextFillRect(context, target_rect);

  base::ScopedCFTypeRef<CGImageRef> cg_image(
      CGBitmapContextCreateImage(context));
  return [UIImage imageWithCGImage:cg_image
                             scale:scale
                       orientation:UIImageOrientationUp];
}


class ImageIOSTest : public testing::Test {
 public:
  ImageIOSTest() {}

  ImageIOSTest(const ImageIOSTest&) = delete;
  ImageIOSTest& operator=(const ImageIOSTest&) = delete;

  ~ImageIOSTest() override {}

  void SetUp() override {
    original_scale_factors_ = gfx::ImageSkia::GetSupportedScales();
  }

  void TearDown() override {
    gfx::ImageSkia::SetSupportedScales(original_scale_factors_);
  }

 private:
  // Used to save and restore the scale factors in effect before this test.
  std::vector<float> original_scale_factors_;
};

// Tests image conversion when the scale factor of the source image is not in
// the list of supported scale factors.
TEST_F(ImageIOSTest, ImageConversionWithUnsupportedScaleFactor) {
  const CGFloat kWidth = 200;
  const CGFloat kHeight = 100;
  const CGFloat kTestScales[3] = { 1.0f, 2.0f, 3.0f };

  for (size_t i = 0; i < std::size(kTestScales); ++i) {
    for (size_t j = 0; j < std::size(kTestScales); ++j) {
      const CGFloat source_scale = kTestScales[i];
      const CGFloat supported_scale = kTestScales[j];

      // Set the supported scale for testing.
      std::vector<float> supported_scales;
      supported_scales.push_back(supported_scale);
      gfx::ImageSkia::SetSupportedScales(supported_scales);

      // Create an UIImage with the appropriate source_scale.
      UIImage* ui_image =
          UIImageWithSizeAndScale(kWidth, kHeight, source_scale);
      ASSERT_EQ(kWidth, ui_image.size.width);
      ASSERT_EQ(kHeight, ui_image.size.height);
      ASSERT_EQ(source_scale, ui_image.scale);

      // Convert to SkBitmap and test its size.
      gfx::Image to_skbitmap([ui_image retain]);
      const SkBitmap* bitmap = to_skbitmap.ToSkBitmap();
      ASSERT_TRUE(bitmap != NULL);
      EXPECT_EQ(kWidth * supported_scale, bitmap->width());
      EXPECT_EQ(kHeight * supported_scale, bitmap->height());

      // Convert to ImageSkia and test its size.
      gfx::Image to_imageskia([ui_image retain]);
      const gfx::ImageSkia* imageskia = to_imageskia.ToImageSkia();
      EXPECT_EQ(kWidth, imageskia->width());
      EXPECT_EQ(kHeight, imageskia->height());

      // TODO(rohitrao): Convert from ImageSkia back to UIImage.  This should
      // scale the image based on the current set of supported scales.
    }
  }
}

} // namespace
