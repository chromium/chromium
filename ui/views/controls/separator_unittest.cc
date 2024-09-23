// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/separator.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/border.h"
#include "ui/views/test/views_test_base.h"

namespace views {

// Base test fixture for Separator tests.
class SeparatorTest : public ViewsTestBase {
 public:
  SeparatorTest() = default;

  SeparatorTest(const SeparatorTest&) = delete;
  SeparatorTest& operator=(const SeparatorTest&) = delete;

  ~SeparatorTest() override = default;

 protected:
  // views::ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

  SkBitmap PaintToCanvas(float image_scale);
  void ExpectDrawAtLeastOnePixel(float image_scale);

  std::unique_ptr<Widget> widget_;
  raw_ptr<Separator> separator_ = nullptr;

  SkColor expected_foreground_color_ = gfx::kPlaceholderColor;

  static const SkColor kBackgroundColor;
  static const ui::ColorId kForegroundColorId;
  static const gfx::Size kTestImageSize;
};

const SkColor SeparatorTest::kBackgroundColor = SK_ColorRED;
const ui::ColorId SeparatorTest::kForegroundColorId = ui::kColorSeparator;
const gfx::Size SeparatorTest::kTestImageSize{24, 24};

void SeparatorTest::SetUp() {
  ViewsTestBase::SetUp();
  widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  separator_ = widget_->SetContentsView(std::make_unique<Separator>());
  expected_foreground_color_ =
      widget_->GetColorProvider()->GetColor(kForegroundColorId);
}

void SeparatorTest::TearDown() {
  separator_ = nullptr;
  widget_.reset();
  ViewsTestBase::TearDown();
}

SkBitmap SeparatorTest::PaintToCanvas(float image_scale) {
  gfx::Canvas canvas(kTestImageSize, image_scale, true);
  canvas.DrawColor(kBackgroundColor);
  separator_->OnPaint(&canvas);
  return canvas.GetBitmap();
}

void SeparatorTest::ExpectDrawAtLeastOnePixel(float image_scale) {
  SkBitmap painted = PaintToCanvas(image_scale);
  gfx::Canvas unpainted(kTestImageSize, image_scale, true);
  unpainted.DrawColor(kBackgroundColor);

  // At least 1 pixel should be changed.
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(painted, unpainted.GetBitmap()));
}

TEST_F(SeparatorTest, GetPreferredSize_VerticalOrientation) {
  // Orientation is vertical by default.
  constexpr int kLength = 8;
  separator_->SetPreferredLength(kLength);
  EXPECT_EQ(separator_->GetPreferredSize({}),
            gfx::Size(Separator::kThickness, kLength));
}

TEST_F(SeparatorTest, GetPreferredSize_HorizontalOrientation) {
  constexpr int kLength = 8;
  separator_->SetOrientation(Separator::Orientation::kHorizontal);
  separator_->SetPreferredLength(kLength);
  EXPECT_EQ(separator_->GetPreferredSize({}),
            gfx::Size(kLength, Separator::kThickness));
}

TEST_F(SeparatorTest, ImageScaleBelowOne) {
  // Vertical line with 1[dp] thickness by default.
  separator_->SetPreferredLength(8);
  ExpectDrawAtLeastOnePixel(0.4);
}

TEST_F(SeparatorTest, ImageScaleBelowOne_HorizontalLine) {
  const int kThickness = 1;
  // Use Separator as a horizontal line with 1[dp] thickness.
  separator_->SetBounds(4, 5, 8, kThickness);
  ExpectDrawAtLeastOnePixel(0.4);
}

TEST_F(SeparatorTest, Paint_NoInsets_FillsCanvas_Scale100) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);

  SkBitmap painted = PaintToCanvas(1.0f);
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 9));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(9, 9));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(9, 0));
}

TEST_F(SeparatorTest, Paint_NoInsets_FillsCanvas_Scale125) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);

  SkBitmap painted = PaintToCanvas(1.25f);
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 12));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(12, 12));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(12, 0));
}

TEST_F(SeparatorTest, Paint_NoInsets_FillsCanvas_Scale150) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);

  SkBitmap painted = PaintToCanvas(1.5f);
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 14));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(14, 14));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(14, 0));
}

TEST_F(SeparatorTest, Paint_TopInset_Scale100) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(1, 0, 0, 0)));

  SkBitmap painted = PaintToCanvas(1.0f);
  EXPECT_EQ(kBackgroundColor, painted.getColor(0, 0));
  EXPECT_EQ(kBackgroundColor, painted.getColor(9, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 1));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(9, 1));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 9));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(9, 9));
}

TEST_F(SeparatorTest, Paint_TopInset_Scale125) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(1, 0, 0, 0)));

  SkBitmap painted = PaintToCanvas(1.25f);
  EXPECT_EQ(kBackgroundColor, painted.getColor(0, 1));
  EXPECT_EQ(kBackgroundColor, painted.getColor(12, 1));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 2));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(12, 2));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 12));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(12, 12));
}

TEST_F(SeparatorTest, Paint_LeftInset_Scale100) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(0, 1, 0, 0)));

  SkBitmap painted = PaintToCanvas(1.0f);
  EXPECT_EQ(kBackgroundColor, painted.getColor(0, 0));
  EXPECT_EQ(kBackgroundColor, painted.getColor(0, 9));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(1, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(1, 9));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(9, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(9, 9));
}

TEST_F(SeparatorTest, Paint_LeftInset_Scale125) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(0, 1, 0, 0)));

  SkBitmap painted = PaintToCanvas(1.25f);
  EXPECT_EQ(kBackgroundColor, painted.getColor(1, 0));
  EXPECT_EQ(kBackgroundColor, painted.getColor(1, 12));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(2, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(2, 12));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(12, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(12, 12));
}

TEST_F(SeparatorTest, Paint_BottomInset_Scale100) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 1, 0)));

  SkBitmap painted = PaintToCanvas(1.0f);
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(9, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 8));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(9, 8));
  EXPECT_EQ(kBackgroundColor, painted.getColor(0, 9));
  EXPECT_EQ(kBackgroundColor, painted.getColor(9, 9));
}

TEST_F(SeparatorTest, Paint_BottomInset_Scale125) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 1, 0)));

  SkBitmap painted = PaintToCanvas(1.25f);
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(12, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 10));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(12, 10));
  EXPECT_EQ(kBackgroundColor, painted.getColor(0, 11));
  EXPECT_EQ(kBackgroundColor, painted.getColor(12, 11));
}

TEST_F(SeparatorTest, Paint_RightInset_Scale100) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 0, 1)));

  SkBitmap painted = PaintToCanvas(1.0f);
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 9));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(8, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(8, 9));
  EXPECT_EQ(kBackgroundColor, painted.getColor(9, 0));
  EXPECT_EQ(kBackgroundColor, painted.getColor(9, 9));
}

TEST_F(SeparatorTest, Paint_RightInset_Scale125) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 0, 1)));

  SkBitmap painted = PaintToCanvas(1.25f);
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 12));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(10, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(10, 12));
  EXPECT_EQ(kBackgroundColor, painted.getColor(11, 0));
  EXPECT_EQ(kBackgroundColor, painted.getColor(11, 12));
}

TEST_F(SeparatorTest, Paint_Vertical_Scale100) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(0, 4, 0, 5)));

  SkBitmap painted = PaintToCanvas(1.0f);
  EXPECT_EQ(kBackgroundColor, painted.getColor(3, 0));
  EXPECT_EQ(kBackgroundColor, painted.getColor(3, 9));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(4, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(4, 9));
  EXPECT_EQ(kBackgroundColor, painted.getColor(5, 0));
  EXPECT_EQ(kBackgroundColor, painted.getColor(5, 9));
}

TEST_F(SeparatorTest, Paint_Vertical_Scale125) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(0, 4, 0, 5)));

  SkBitmap painted = PaintToCanvas(1.25f);
  EXPECT_EQ(kBackgroundColor, painted.getColor(4, 0));
  EXPECT_EQ(kBackgroundColor, painted.getColor(4, 12));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(5, 0));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(5, 12));
  EXPECT_EQ(kBackgroundColor, painted.getColor(6, 0));
  EXPECT_EQ(kBackgroundColor, painted.getColor(6, 12));
}

TEST_F(SeparatorTest, Paint_Horizontal_Scale100) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(4, 0, 5, 0)));

  SkBitmap painted = PaintToCanvas(1.0f);
  EXPECT_EQ(kBackgroundColor, painted.getColor(0, 3));
  EXPECT_EQ(kBackgroundColor, painted.getColor(9, 3));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 4));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(9, 4));
  EXPECT_EQ(kBackgroundColor, painted.getColor(0, 5));
  EXPECT_EQ(kBackgroundColor, painted.getColor(9, 5));
}

TEST_F(SeparatorTest, Paint_Horizontal_Scale125) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(4, 0, 5, 0)));

  SkBitmap painted = PaintToCanvas(1.25f);
  EXPECT_EQ(kBackgroundColor, painted.getColor(0, 4));
  EXPECT_EQ(kBackgroundColor, painted.getColor(12, 4));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(0, 5));
  EXPECT_EQ(expected_foreground_color_, painted.getColor(12, 5));
  EXPECT_EQ(kBackgroundColor, painted.getColor(0, 6));
  EXPECT_EQ(kBackgroundColor, painted.getColor(12, 6));
}

// Ensure that the separator is always at least 1px, even if insets would reduce
// it to zero.
TEST_F(SeparatorTest, Paint_MinimumSize_Scale100) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(5, 5, 5, 5)));

  SkBitmap painted = PaintToCanvas(1.0f);
  EXPECT_EQ(expected_foreground_color_, painted.getColor(5, 5));
  EXPECT_EQ(kBackgroundColor, painted.getColor(4, 5));
  EXPECT_EQ(kBackgroundColor, painted.getColor(5, 4));
  EXPECT_EQ(kBackgroundColor, painted.getColor(5, 6));
  EXPECT_EQ(kBackgroundColor, painted.getColor(6, 5));
}

// Ensure that the separator is always at least 1px, even if insets would reduce
// it to zero (with scale factor > 1).
TEST_F(SeparatorTest, Paint_MinimumSize_Scale125) {
  separator_->SetSize({10, 10});
  separator_->SetColorId(kForegroundColorId);
  separator_->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(5, 5, 5, 5)));

  SkBitmap painted = PaintToCanvas(1.25f);
  EXPECT_EQ(expected_foreground_color_, painted.getColor(7, 7));
  EXPECT_EQ(kBackgroundColor, painted.getColor(6, 7));
  EXPECT_EQ(kBackgroundColor, painted.getColor(7, 6));
  EXPECT_EQ(kBackgroundColor, painted.getColor(7, 8));
  EXPECT_EQ(kBackgroundColor, painted.getColor(8, 7));
}

}  // namespace views
