// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/ios/ui_image_test_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui::test {

// Test the creation of UIImages.
TEST(UIImageTestUtilsTest, TestImageCreation) {
  CGSize size = CGSizeMake(10, 10);
  UIImage* image =
      uiimage_utils::UIImageWithSizeAndSolidColor(size, [UIColor redColor]);
  EXPECT_TRUE(CGSizeEqualToSize(size, image.size));
}

// Test the detection of identical UIImages.
TEST(UIImageTestUtilsTest, TestImageEquality) {
  CGSize size10x10 = CGSizeMake(10, 10);
  CGSize size5x20 = CGSizeMake(5, 20);
  UIColor* green = [UIColor greenColor];
  UIColor* blue = [UIColor blueColor];

  UIImage* imageblue10x10 =
      uiimage_utils::UIImageWithSizeAndSolidColor(size10x10, blue);
  UIImage* imageblue5x20 =
      uiimage_utils::UIImageWithSizeAndSolidColor(size5x20, blue);
  UIImage* imageGreen10x10 =
      uiimage_utils::UIImageWithSizeAndSolidColor(size10x10, green);
  UIImage* imageGreen10x10Bis =
      uiimage_utils::UIImageWithSizeAndSolidColor(size10x10, green);

  // Test with |nil| parameters.
  EXPECT_TRUE(uiimage_utils::UIImagesAreEqual(nil, nil));
  EXPECT_FALSE(uiimage_utils::UIImagesAreEqual(nil, imageblue10x10));
  EXPECT_FALSE(uiimage_utils::UIImagesAreEqual(imageblue10x10, nil));

  // Test with images with different sizes (but same amount of pixels), same
  // color.
  EXPECT_FALSE(uiimage_utils::UIImagesAreEqual(imageblue10x10, imageblue5x20));

  // Test with images with same size, different color.
  EXPECT_FALSE(
      uiimage_utils::UIImagesAreEqual(imageblue10x10, imageGreen10x10));

  // Test with images with same size, same color.
  EXPECT_TRUE(
      uiimage_utils::UIImagesAreEqual(imageGreen10x10, imageGreen10x10Bis));
}

}  // namespace ui::test
