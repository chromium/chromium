// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/image_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
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
  ImageViewTest() = default;

  ImageViewTest(const ImageViewTest&) = delete;
  ImageViewTest& operator=(const ImageViewTest&) = delete;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(200, 200);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(std::move(params));
    auto container = std::make_unique<View>();
    // Make sure children can take up exactly as much space as they require.
    BoxLayout::Orientation orientation =
        GetParam() == Axis::kHorizontal ? BoxLayout::Orientation::kHorizontal
                                        : BoxLayout::Orientation::kVertical;
    container->SetLayoutManager(std::make_unique<BoxLayout>(orientation));
    image_view_ = container->AddChildView(std::make_unique<ImageView>());
    widget_.SetContentsView(std::move(container));

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
  raw_ptr<ImageView> image_view_ = nullptr;
  Widget widget_;
};

// Test the image origin of the internal ImageSkia is correct when it is
// center-aligned (both horizontally and vertically).
TEST_P(ImageViewTest, CenterAlignment) {
  image_view()->SetHorizontalAlignment(ImageView::Alignment::kCenter);

  constexpr int kImageSkiaSize = 4;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kImageSkiaSize, kImageSkiaSize);
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  image_view()->SetImage(image_skia);
  views::test::RunScheduledLayout(image_view());
  EXPECT_NE(gfx::Size(), image_skia.size());

  // With no changes to the size / padding of |image_view|, the origin of
  // |image_skia| is the same as the origin of |image_view|.
  EXPECT_EQ(0, CurrentImageOriginForParam());

  // Test insets are always respected in LTR and RTL.
  constexpr int kInset = 5;
  image_view()->SetBorder(CreateEmptyBorder(kInset));
  views::test::RunScheduledLayout(image_view());
  EXPECT_EQ(kInset, CurrentImageOriginForParam());

  SetRTL(true);
  views::test::RunScheduledLayout(image_view());
  EXPECT_EQ(kInset, CurrentImageOriginForParam());

  // Check this still holds true when the insets are asymmetrical.
  constexpr int kLeadingInset = 4;
  constexpr int kTrailingInset = 6;
  image_view()->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(
      kLeadingInset, kLeadingInset, kTrailingInset, kTrailingInset)));
  views::test::RunScheduledLayout(image_view());
  EXPECT_EQ(kLeadingInset, CurrentImageOriginForParam());

  SetRTL(false);
  views::test::RunScheduledLayout(image_view());
  EXPECT_EQ(kLeadingInset, CurrentImageOriginForParam());
}

TEST_P(ImageViewTest, ImageOriginForCustomViewBounds) {
  gfx::Rect image_view_bounds(10, 10, 80, 80);
  image_view()->SetHorizontalAlignment(ImageView::Alignment::kCenter);
  image_view()->SetBoundsRect(image_view_bounds);

  SkBitmap bitmap;
  constexpr int kImageSkiaSize = 20;
  bitmap.allocN32Pixels(kImageSkiaSize, kImageSkiaSize);
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  image_view()->SetImage(image_skia);

  EXPECT_EQ(gfx::Point(30, 30), image_view()->GetImageBounds().origin());
  EXPECT_EQ(image_view_bounds, image_view()->bounds());
}

// Verifies setting the accessible name will be call NotifyAccessibilityEvent.
TEST_P(ImageViewTest, SetAccessibleNameNotifiesAccessibilityEvent) {
  std::u16string test_tooltip_text = u"Test Tooltip Text";
  test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged));
  image_view()->SetAccessibleName(test_tooltip_text);
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged));
  EXPECT_EQ(test_tooltip_text, image_view()->GetAccessibleName());
  ui::AXNodeData data;
  image_view()->GetAccessibleNodeData(&data);
  const std::string& name =
      data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  EXPECT_EQ(test_tooltip_text, base::ASCIIToUTF16(name));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ImageViewTest,
                         ::testing::Values(Axis::kHorizontal, Axis::kVertical));

}  // namespace views
