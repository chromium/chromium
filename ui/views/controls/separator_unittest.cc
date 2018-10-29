// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/separator.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/test/views_test_base.h"

namespace views {

// Base test fixture for Separator tests.
class SeparatorTest : public views::ViewsTestBase {
 public:
  SeparatorTest() = default;
  ~SeparatorTest() override = default;

 protected:
  void ExpectDrawAtLeastOnePixel(float image_scale);

  Separator separator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SeparatorTest);
};

void SeparatorTest::ExpectDrawAtLeastOnePixel(float image_scale) {
  const gfx::Size kTestImageSize = gfx::Size(24, 24);
  const SkColor kBackgroundColor = SK_ColorRED;
  gfx::Canvas init(kTestImageSize, image_scale, true);
  gfx::Canvas canvas(kTestImageSize, image_scale, true);
  init.DrawColor(kBackgroundColor);
  canvas.DrawColor(kBackgroundColor);
  ASSERT_TRUE(gfx::test::AreBitmapsEqual(canvas.GetBitmap(), init.GetBitmap()));
  separator_.OnPaint(&canvas);

  // At least 1 pixel should be changed.
  EXPECT_FALSE(
      gfx::test::AreBitmapsEqual(canvas.GetBitmap(), init.GetBitmap()));
}

TEST_F(SeparatorTest, ImageScaleBelowOne) {
  // Vertical line with 1[dp] thickness by default.
  separator_.SetPreferredHeight(8);
  ExpectDrawAtLeastOnePixel(0.4);
}

TEST_F(SeparatorTest, ImageScaleBelowOne_HorizontalLine) {
  const int kThickness = 1;
  // Use Separator as a horizontal line with 1[dp] thickness.
  separator_.SetBounds(4, 5, 8, kThickness);
  ExpectDrawAtLeastOnePixel(0.4);
}

}  // namespace views
