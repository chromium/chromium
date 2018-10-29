// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/image_view.h"

#include "base/i18n/rtl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

enum class Axis {
  kHorizontal,
  kVertical,
};

// A test utility function to set the application default text direction.
void SetRTL(bool rtl) {
  // Override the current locale/direction.
  base::i18n::SetICUDefaultLocale(rtl ? "he" : "en");
  EXPECT_EQ(rtl, base::i18n::IsRTL());
}

}  // namespace

namespace views {

class ImageViewTest : public ViewsTestBase,
                      public ::testing::WithParamInterface<Axis> {
 public:
  ImageViewTest() {}

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(200, 200);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(params);
    View* container = new View();
    // Make sure children can take up exactly as much space as they require.
    BoxLayout::Orientation orientation = GetParam() == Axis::kHorizontal
                                             ? BoxLayout::kHorizontal
                                             : BoxLayout::kVertical;
    container->SetLayoutManager(std::make_unique<BoxLayout>(orientation));
    widget_.SetContentsView(container);

    image_view_ = new ImageView();
    container->AddChildView(image_view_);

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

  int CurrentImageOriginForParam() {
    image_view()->UpdateImageOrigin();
    gfx::Point origin = image_view()->GetImageBounds().origin();
    return GetParam() == Axis::kHorizontal ? origin.x() : origin.y();
  }

 protected:
  ImageView* image_view() { return image_view_; }
  Widget* widget() { return &widget_; }

 private:
  ImageView* image_view_ = nullptr;
  Widget widget_;

  DISALLOW_COPY_AND_ASSIGN(ImageViewTest);
};

// Test the image origin of the internal ImageSkia is correct when it is
// center-aligned (both horizontally and vertically).
TEST_P(ImageViewTest, CenterAlignment) {
  image_view()->SetHorizontalAlignment(ImageView::CENTER);

  constexpr int kImageSkiaSize = 4;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kImageSkiaSize, kImageSkiaSize);
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  image_view()->SetImage(image_skia);
  widget()->GetContentsView()->Layout();
  EXPECT_NE(gfx::Size(), image_skia.size());

  // With no changes to the size / padding of |image_view|, the origin of
  // |image_skia| is the same as the origin of |image_view|.
  EXPECT_EQ(0, CurrentImageOriginForParam());

  // Test insets are always respected in LTR and RTL.
  constexpr int kInset = 5;
  image_view()->SetBorder(CreateEmptyBorder(gfx::Insets(kInset)));
  widget()->GetContentsView()->Layout();
  EXPECT_EQ(kInset, CurrentImageOriginForParam());

  SetRTL(true);
  widget()->GetContentsView()->Layout();
  EXPECT_EQ(kInset, CurrentImageOriginForParam());

  // Check this still holds true when the insets are asymmetrical.
  constexpr int kLeadingInset = 4;
  constexpr int kTrailingInset = 6;
  image_view()->SetBorder(CreateEmptyBorder(
      gfx::Insets(/*top=*/kLeadingInset, /*left=*/kLeadingInset,
                  /*bottom=*/kTrailingInset, /*right=*/kTrailingInset)));
  widget()->GetContentsView()->Layout();
  EXPECT_EQ(kLeadingInset, CurrentImageOriginForParam());

  SetRTL(false);
  widget()->GetContentsView()->Layout();
  EXPECT_EQ(kLeadingInset, CurrentImageOriginForParam());
}

TEST_P(ImageViewTest, ImageOriginForCustomViewBounds) {
  gfx::Rect image_view_bounds(10, 10, 80, 80);
  image_view()->SetHorizontalAlignment(ImageView::CENTER);
  image_view()->SetBoundsRect(image_view_bounds);

  SkBitmap bitmap;
  constexpr int kImageSkiaSize = 20;
  bitmap.allocN32Pixels(kImageSkiaSize, kImageSkiaSize);
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  image_view()->SetImage(image_skia);

  EXPECT_EQ(gfx::Point(30, 30), image_view()->GetImageBounds().origin());
  EXPECT_EQ(image_view_bounds, image_view()->bounds());
}

INSTANTIATE_TEST_CASE_P(,
                        ImageViewTest,
                        ::testing::Values(Axis::kHorizontal, Axis::kVertical));

}  // namespace views
