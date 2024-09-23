// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/image_button.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/border.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/views_test_base.h"

namespace {

class Parent : public views::View {
 public:
  Parent() = default;

  Parent(const Parent&) = delete;
  Parent& operator=(const Parent&) = delete;

  void ChildPreferredSizeChanged(views::View* view) override {
    pref_size_changed_calls_++;
  }

  int pref_size_changed_calls() const { return pref_size_changed_calls_; }

 private:
  int pref_size_changed_calls_ = 0;
};

}  // namespace

namespace views {

using ImageButtonTest = ViewsTestBase;

TEST_F(ImageButtonTest, FocusBehavior) {
  ImageButton button;

  EXPECT_EQ(PlatformStyle::kDefaultFocusBehavior, button.GetFocusBehavior());
}

TEST_F(ImageButtonTest, Basics) {
  ImageButton button;

  // Our image to paint starts empty.
  EXPECT_TRUE(button.GetImageToPaint().isNull());

  // Without an image, buttons are 16x14 by default.
  EXPECT_EQ(gfx::Size(16, 14), button.GetPreferredSize({}));

  // The minimum image size should be applied even when there is no image.
  button.SetMinimumImageSize(gfx::Size(5, 15));
  EXPECT_EQ(gfx::Size(5, 15), button.GetMinimumImageSize());
  EXPECT_EQ(gfx::Size(16, 15), button.GetPreferredSize({}));

  // Set a normal image.
  gfx::ImageSkia normal_image = gfx::test::CreateImageSkia(10, 20);
  button.SetImageModel(Button::STATE_NORMAL,
                       ui::ImageModel::FromImageSkia(normal_image));

  // Image uses normal image for painting.
  EXPECT_FALSE(button.GetImageToPaint().isNull());
  EXPECT_EQ(10, button.GetImageToPaint().width());
  EXPECT_EQ(20, button.GetImageToPaint().height());

  // Preferred size is the normal button size.
  EXPECT_EQ(gfx::Size(10, 20), button.GetPreferredSize({}));

  // Set a pushed image.
  gfx::ImageSkia pushed_image = gfx::test::CreateImageSkia(11, 21);
  button.SetImageModel(Button::STATE_PRESSED,
                       ui::ImageModel::FromImageSkia(pushed_image));

  // By convention, preferred size doesn't change, even though pushed image
  // is bigger.
  EXPECT_EQ(gfx::Size(10, 20), button.GetPreferredSize({}));

  // We're still painting the normal image.
  EXPECT_FALSE(button.GetImageToPaint().isNull());
  EXPECT_EQ(10, button.GetImageToPaint().width());
  EXPECT_EQ(20, button.GetImageToPaint().height());

  // The minimum image size should make the preferred size bigger.
  button.SetMinimumImageSize(gfx::Size(15, 5));
  EXPECT_EQ(gfx::Size(15, 5), button.GetMinimumImageSize());
  EXPECT_EQ(gfx::Size(15, 20), button.GetPreferredSize({}));
  button.SetMinimumImageSize(gfx::Size(15, 25));
  EXPECT_EQ(gfx::Size(15, 25), button.GetMinimumImageSize());
  EXPECT_EQ(gfx::Size(15, 25), button.GetPreferredSize({}));
}

TEST_F(ImageButtonTest, SetAndGetImage) {
  ImageButton button;

  // Images start as null.
  EXPECT_TRUE(button.GetImage(Button::STATE_NORMAL).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_HOVERED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_PRESSED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_DISABLED).isNull());

  // Setting images works as expected.
  gfx::ImageSkia image1 = gfx::test::CreateImageSkia(10, 11);
  gfx::ImageSkia image2 = gfx::test::CreateImageSkia(20, 21);
  button.SetImageModel(Button::STATE_NORMAL,
                       ui::ImageModel::FromImageSkia(image1));
  button.SetImageModel(Button::STATE_HOVERED,
                       ui::ImageModel::FromImageSkia(image2));
  EXPECT_TRUE(
      button.GetImage(Button::STATE_NORMAL).BackedBySameObjectAs(image1));
  EXPECT_TRUE(
      button.GetImage(Button::STATE_HOVERED).BackedBySameObjectAs(image2));
  EXPECT_TRUE(button.GetImage(Button::STATE_PRESSED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_DISABLED).isNull());

  // ImageButton supports NULL image pointers.
  button.SetImageModel(Button::STATE_NORMAL, ui::ImageModel());
  EXPECT_TRUE(button.GetImage(Button::STATE_NORMAL).isNull());
}

TEST_F(ImageButtonTest, ImagePositionWithBorder) {
  ImageButton button;
  gfx::ImageSkia image = gfx::test::CreateImageSkia(20, 30);
  button.SetImageModel(Button::STATE_NORMAL,
                       ui::ImageModel::FromImageSkia(image));

  // The image should be painted at the top-left corner.
  EXPECT_EQ(gfx::Point(), button.ComputeImagePaintPosition(image));

  button.SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(10, 5, 0, 0)));
  EXPECT_EQ(gfx::Point(5, 10), button.ComputeImagePaintPosition(image));

  button.SetBorder(NullBorder());
  button.SetBounds(0, 0, 50, 50);
  EXPECT_EQ(gfx::Point(), button.ComputeImagePaintPosition(image));

  button.SetImageHorizontalAlignment(ImageButton::ALIGN_CENTER);
  button.SetImageVerticalAlignment(ImageButton::ALIGN_MIDDLE);
  EXPECT_EQ(gfx::Point(15, 10), button.ComputeImagePaintPosition(image));
  button.SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(10, 10, 0, 0)));
  EXPECT_EQ(gfx::Point(20, 15), button.ComputeImagePaintPosition(image));

  // The entire button's size should take the border into account.
  EXPECT_EQ(gfx::Size(30, 40), button.GetPreferredSize({}));

  // The border should be added on top of the minimum image size.
  button.SetMinimumImageSize(gfx::Size(30, 5));
  EXPECT_EQ(gfx::Size(40, 40), button.GetPreferredSize({}));
}

TEST_F(ImageButtonTest, LeftAlignedMirrored) {
  ImageButton button;
  gfx::ImageSkia image = gfx::test::CreateImageSkia(20, 30);
  button.SetImageModel(Button::STATE_NORMAL,
                       ui::ImageModel::FromImageSkia(image));
  button.SetBounds(0, 0, 50, 30);
  button.SetImageVerticalAlignment(ImageButton::ALIGN_BOTTOM);
  button.SetDrawImageMirrored(true);

  // Because the coordinates are flipped, we should expect this to draw as if
  // it were ALIGN_RIGHT.
  EXPECT_EQ(gfx::Point(30, 0), button.ComputeImagePaintPosition(image));
}

TEST_F(ImageButtonTest, RightAlignedMirrored) {
  ImageButton button;
  gfx::ImageSkia image = gfx::test::CreateImageSkia(20, 30);
  button.SetImageModel(Button::STATE_NORMAL,
                       ui::ImageModel::FromImageSkia(image));
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
  ImageButton button;
  gfx::ImageSkia first_image = gfx::test::CreateImageSkia(20, 30);
  gfx::ImageSkia second_image = gfx::test::CreateImageSkia(/*size=*/50);
  button.SetImageModel(Button::STATE_NORMAL,
                       ui::ImageModel::FromImageSkia(first_image));
  parent.AddChildView(&button);
  ASSERT_EQ(0, parent.pref_size_changed_calls());

  button.SetImageModel(Button::STATE_NORMAL,
                       ui::ImageModel::FromImageSkia(first_image));
  EXPECT_EQ(0, parent.pref_size_changed_calls());

  button.SetImageModel(Button::STATE_HOVERED,
                       ui::ImageModel::FromImageSkia(second_image));
  EXPECT_EQ(0, parent.pref_size_changed_calls());

  // Changing normal state image size leads to a change in preferred size.
  button.SetImageModel(Button::STATE_NORMAL,
                       ui::ImageModel::FromImageSkia(second_image));
  EXPECT_EQ(1, parent.pref_size_changed_calls());
}

}  // namespace views
