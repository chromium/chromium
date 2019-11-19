// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

namespace {

gfx::ImageSkia CreateTestImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

class Parent : public views::View {
 public:
  Parent() = default;

  void ChildPreferredSizeChanged(views::View* view) override {
    pref_size_changed_calls_++;
  }

  int pref_size_changed_calls() const {
    return pref_size_changed_calls_;
  }

 private:
  int pref_size_changed_calls_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Parent);
};

}  // namespace

namespace views {

using ImageButtonTest = ViewsTestBase;

TEST_F(ImageButtonTest, Basics) {
  ImageButton button(nullptr);

  // Our image to paint starts empty.
  EXPECT_TRUE(button.GetImageToPaint().isNull());

  // Without an image, buttons are 16x14 by default.
  EXPECT_EQ(gfx::Size(16, 14), button.GetPreferredSize());

  // The minimum image size should be applied even when there is no image.
  button.SetMinimumImageSize(gfx::Size(5, 15));
  EXPECT_EQ(gfx::Size(5, 15), button.GetMinimumImageSize());
  EXPECT_EQ(gfx::Size(16, 15), button.GetPreferredSize());

  // Set a normal image.
  gfx::ImageSkia normal_image = CreateTestImage(10, 20);
  button.SetImage(Button::STATE_NORMAL, &normal_image);

  // Image uses normal image for painting.
  EXPECT_FALSE(button.GetImageToPaint().isNull());
  EXPECT_EQ(10, button.GetImageToPaint().width());
  EXPECT_EQ(20, button.GetImageToPaint().height());

  // Preferred size is the normal button size.
  EXPECT_EQ(gfx::Size(10, 20), button.GetPreferredSize());

  // Set a pushed image.
  gfx::ImageSkia pushed_image = CreateTestImage(11, 21);
  button.SetImage(Button::STATE_PRESSED, &pushed_image);

  // By convention, preferred size doesn't change, even though pushed image
  // is bigger.
  EXPECT_EQ(gfx::Size(10, 20), button.GetPreferredSize());

  // We're still painting the normal image.
  EXPECT_FALSE(button.GetImageToPaint().isNull());
  EXPECT_EQ(10, button.GetImageToPaint().width());
  EXPECT_EQ(20, button.GetImageToPaint().height());

  // The minimum image size should make the preferred size bigger.
  button.SetMinimumImageSize(gfx::Size(15, 5));
  EXPECT_EQ(gfx::Size(15, 5), button.GetMinimumImageSize());
  EXPECT_EQ(gfx::Size(15, 20), button.GetPreferredSize());
  button.SetMinimumImageSize(gfx::Size(15, 25));
  EXPECT_EQ(gfx::Size(15, 25), button.GetMinimumImageSize());
  EXPECT_EQ(gfx::Size(15, 25), button.GetPreferredSize());
}

TEST_F(ImageButtonTest, SetAndGetImage) {
  ImageButton button(nullptr);

  // Images start as null.
  EXPECT_TRUE(button.GetImage(Button::STATE_NORMAL).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_HOVERED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_PRESSED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_DISABLED).isNull());

  // Setting images works as expected.
  gfx::ImageSkia image1 = CreateTestImage(10, 11);
  gfx::ImageSkia image2 = CreateTestImage(20, 21);
  button.SetImage(Button::STATE_NORMAL, &image1);
  button.SetImage(Button::STATE_HOVERED, &image2);
  EXPECT_TRUE(
      button.GetImage(Button::STATE_NORMAL).BackedBySameObjectAs(image1));
  EXPECT_TRUE(
      button.GetImage(Button::STATE_HOVERED).BackedBySameObjectAs(image2));
  EXPECT_TRUE(button.GetImage(Button::STATE_PRESSED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_DISABLED).isNull());

  // ImageButton supports NULL image pointers.
  button.SetImage(Button::STATE_NORMAL, nullptr);
  EXPECT_TRUE(button.GetImage(Button::STATE_NORMAL).isNull());
}

TEST_F(ImageButtonTest, ImagePositionWithBorder) {
  ImageButton button(nullptr);
  gfx::ImageSkia image = CreateTestImage(20, 30);
  button.SetImage(Button::STATE_NORMAL, &image);

  // The image should be painted at the top-left corner.
  EXPECT_EQ(gfx::Point(), button.ComputeImagePaintPosition(image));

  button.SetBorder(views::CreateEmptyBorder(10, 5, 0, 0));
  EXPECT_EQ(gfx::Point(5, 10), button.ComputeImagePaintPosition(image));

  button.SetBorder(NullBorder());
  button.SetBounds(0, 0, 50, 50);
  EXPECT_EQ(gfx::Point(), button.ComputeImagePaintPosition(image));

  button.SetImageHorizontalAlignment(ImageButton::ALIGN_CENTER);
  button.SetImageVerticalAlignment(ImageButton::ALIGN_MIDDLE);
  EXPECT_EQ(gfx::Point(15, 10), button.ComputeImagePaintPosition(image));
  button.SetBorder(views::CreateEmptyBorder(10, 10, 0, 0));
  EXPECT_EQ(gfx::Point(20, 15), button.ComputeImagePaintPosition(image));

  // The entire button's size should take the border into account.
  EXPECT_EQ(gfx::Size(30, 40), button.GetPreferredSize());

  // The border should be added on top of the minimum image size.
  button.SetMinimumImageSize(gfx::Size(30, 5));
  EXPECT_EQ(gfx::Size(40, 40), button.GetPreferredSize());
}

TEST_F(ImageButtonTest, LeftAlignedMirrored) {
  ImageButton button(nullptr);
  gfx::ImageSkia image = CreateTestImage(20, 30);
  button.SetImage(Button::STATE_NORMAL, &image);
  button.SetBounds(0, 0, 50, 30);
  button.SetImageVerticalAlignment(ImageButton::ALIGN_BOTTOM);
  button.SetDrawImageMirrored(true);

  // Because the coordinates are flipped, we should expect this to draw as if
  // it were ALIGN_RIGHT.
  EXPECT_EQ(gfx::Point(30, 0), button.ComputeImagePaintPosition(image));
}

TEST_F(ImageButtonTest, RightAlignedMirrored) {
  ImageButton button(nullptr);
  gfx::ImageSkia image = CreateTestImage(20, 30);
  button.SetImage(Button::STATE_NORMAL, &image);
  button.SetBounds(0, 0, 50, 30);
  button.SetImageHorizontalAlignment(ImageButton::ALIGN_RIGHT);
  button.SetImageVerticalAlignment(ImageButton::ALIGN_BOTTOM);
  button.SetDrawImageMirrored(true);

  // Because the coordinates are flipped, we should expect this to draw as if
  // it were ALIGN_LEFT.
  EXPECT_EQ(gfx::Point(0, 0), button.ComputeImagePaintPosition(image));
}

TEST_F(ImageButtonTest, PreferredSizeInvalidation) {
  Parent parent;
  ImageButton button(nullptr);
  gfx::ImageSkia first_image = CreateTestImage(20, 30);
  gfx::ImageSkia second_image = CreateTestImage(50, 50);
  button.SetImage(Button::STATE_NORMAL, &first_image);
  parent.AddChildView(&button);
  ASSERT_EQ(0, parent.pref_size_changed_calls());

  button.SetImage(Button::STATE_NORMAL, &first_image);
  EXPECT_EQ(0, parent.pref_size_changed_calls());

  button.SetImage(Button::STATE_HOVERED, &second_image);
  EXPECT_EQ(0, parent.pref_size_changed_calls());

  // Changing normal state image size leads to a change in preferred size.
  button.SetImage(Button::STATE_NORMAL, &second_image);
  EXPECT_EQ(1, parent.pref_size_changed_calls());
}

}  // namespace views
