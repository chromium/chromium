// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/box_layout.h"

#include <stddef.h>

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

constexpr BoxLayout::MainAxisAlignment kMainAlignments[3] = {
    BoxLayout::MainAxisAlignment::kStart,
    BoxLayout::MainAxisAlignment::kCenter,
    BoxLayout::MainAxisAlignment::kEnd,
};

class BoxLayoutTest : public testing::Test {
 public:
  void SetUp() override { host_ = std::make_unique<View>(); }

  std::unique_ptr<View> host_;
};

}  // namespace

TEST_F(BoxLayoutTest, Empty) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(10), 20));
  EXPECT_EQ(gfx::Size(20, 20), layout->GetPreferredSize(host_.get()));
}

TEST_F(BoxLayoutTest, AlignmentHorizontal) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
  View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 20), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 20, 20);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 20), v2->bounds());
}

TEST_F(BoxLayoutTest, AlignmentVertical) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 20), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 20, 20);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, SetInsideBorderInsets) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(20, 10)));
  View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(40, 60), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 40, 60);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 20, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(20, 20, 10, 20), v2->bounds());

  layout->set_inside_border_insets(gfx::Insets(5, 10, 15, 20));
  EXPECT_EQ(gfx::Size(50, 40), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 50, 40);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 5, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(20, 5, 10, 20), v2->bounds());
}

TEST_F(BoxLayoutTest, Spacing) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(7), 8));
  View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(42, 34), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 100, 100);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(7, 7, 10, 86), v1->bounds());
  EXPECT_EQ(gfx::Rect(25, 7, 10, 86), v2->bounds());
}

TEST_F(BoxLayoutTest, Overflow) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v2);
  host_->SetBounds(0, 0, 15, 10);

  // Overflows by positioning views at the start and truncating anything that
  // doesn't fit.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 15, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());

  // Clipping of children should occur at the opposite end(s) to the main axis
  // alignment position.
  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kStart);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 15, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());

  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kCenter);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 13, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(13, 0, 2, 10), v2->bounds());

  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kEnd);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 5, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(5, 0, 10, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, NoSpace) {
  host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(10), 10));
  View* childView = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(childView);
  host_->SetBounds(0, 0, 10, 10);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), childView->bounds());
}

TEST_F(BoxLayoutTest, InvisibleChild) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(10), 10));
  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  v1->SetVisible(false);
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(30, 30), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 30, 30);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 10, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, UseHeightForWidth) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  ProportionallySizedView* v2 = new ProportionallySizedView(2);
  v2->SetPreferredWidth(10);
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 50), layout->GetPreferredSize(host_.get()));

  host_->SetBounds(0, 0, 20, 50);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 40), v2->bounds());

  EXPECT_EQ(110, layout->GetPreferredHeightForWidth(host_.get(), 50));

  // Test without horizontal stretching of the views.
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kEnd);
  EXPECT_EQ(gfx::Size(20, 30).ToString(),
            layout->GetPreferredSize(host_.get()).ToString());

  host_->SetBounds(0, 0, 20, 30);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(10, 10, 10, 20), v2->bounds());

  EXPECT_EQ(30, layout->GetPreferredHeightForWidth(host_.get(), 50));
}

TEST_F(BoxLayoutTest, EmptyPreferredSize) {
  for (size_t i = 0; i < 2; i++) {
    BoxLayout::Orientation orientation =
        i == 0 ? BoxLayout::Orientation::kHorizontal
               : BoxLayout::Orientation::kVertical;
    host_->RemoveAllChildViews(true);
    host_->SetLayoutManager(
        std::make_unique<BoxLayout>(orientation, gfx::Insets(), 5));
    View* v1 = new StaticSizedView(gfx::Size());
    host_->AddChildView(v1);
    View* v2 = new StaticSizedView(gfx::Size(10, 10));
    host_->AddChildView(v2);
    host_->SizeToPreferredSize();
    host_->Layout();

    EXPECT_EQ(v2->GetPreferredSize().width(), host_->bounds().width()) << i;
    EXPECT_EQ(v2->GetPreferredSize().height(), host_->bounds().height()) << i;
    EXPECT_EQ(v1->GetPreferredSize().width(), v1->bounds().width()) << i;
    EXPECT_EQ(v1->GetPreferredSize().height(), v1->bounds().height()) << i;
    EXPECT_EQ(v2->GetPreferredSize().width(), v2->bounds().width()) << i;
    EXPECT_EQ(v2->GetPreferredSize().height(), v2->bounds().height()) << i;
  }
}

// Verifies that a BoxLayout correctly handles child spacing, flex layout, and
// empty preferred size, simultaneously.
TEST_F(BoxLayoutTest, EmptyPreferredSizeWithFlexLayoutAndChildSpacing) {
  host_->RemoveAllChildViews(true);
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(), 5));
  View* v1 = new StaticSizedView(gfx::Size());
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  View* v3 = new StaticSizedView(gfx::Size(1, 1));
  host_->AddChildViewAt(v3, 0);
  layout->SetFlexForView(v3, 1);
  host_->SetSize(gfx::Size(1000, 15));
  host_->Layout();

  EXPECT_EQ(host_->bounds().right(), v2->bounds().right());
}

TEST_F(BoxLayoutTest, MainAxisAlignmentHorizontal) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(10), 10));

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);

  host_->SetBounds(0, 0, 100, 40);

  // Align children to the horizontal start by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 20).ToString(), v2->bounds().ToString());

  // Ensure same results for MainAxisAlignment::kStart.
  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kStart);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 20).ToString(), v2->bounds().ToString());

  // Aligns children to the center horizontally.
  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kCenter);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(30, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(60, 10, 10, 20).ToString(), v2->bounds().ToString());

  // Aligns children to the end of the host horizontally, accounting for the
  // inside border spacing.
  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kEnd);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(50, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(80, 10, 10, 20).ToString(), v2->bounds().ToString());
}

TEST_F(BoxLayoutTest, MainAxisAlignmentVertical) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);

  host_->SetBounds(0, 0, 40, 100);

  // Align children to the vertical start by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 20, 10).ToString(), v2->bounds().ToString());

  // Ensure same results for MainAxisAlignment::kStart.
  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kStart);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 20, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the center vertically.
  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kCenter);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 30, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 60, 20, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the end of the host vertically, accounting for the
  // inside border spacing.
  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kEnd);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 50, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 80, 20, 10).ToString(), v2->bounds().ToString());
}

TEST_F(BoxLayoutTest, CrossAxisAlignmentHorizontal) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(10), 10));

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);

  host_->SetBounds(0, 0, 100, 60);

  // Stretch children to fill the available height by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 40).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 40).ToString(), v2->bounds().ToString());

  // Ensure same results for kStretch.
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 40).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 40).ToString(), v2->bounds().ToString());

  // Aligns children to the start vertically.
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStart);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the center vertically.
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 20, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 25, 10, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the end of the host vertically, accounting for the
  // inside border spacing.
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kEnd);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 30, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 40, 10, 10).ToString(), v2->bounds().ToString());
}

TEST_F(BoxLayoutTest, CrossAxisAlignmentVertical) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);

  host_->SetBounds(0, 0, 60, 100);

  // Stretch children to fill the available width by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 40, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 40, 10).ToString(), v2->bounds().ToString());

  // Ensure same results for kStretch.
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 40, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 40, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the start horizontally.
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStart);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 10, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the center horizontally.
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(20, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(25, 40, 10, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the end of the host horizontally, accounting for the
  // inside border spacing.
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kEnd);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(30, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 40, 10, 10).ToString(), v2->bounds().ToString());
}

TEST_F(BoxLayoutTest, CrossAxisAlignmentVerticalChildPreferredWidth) {
  const int available_width = 40;  // box-width - inset - margin
  const int preferred_width = 30;

  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));

  auto* v1 = host_->AddChildView(std::make_unique<ProportionallySizedView>(1));
  v1->SetPreferredWidth(preferred_width);

  host_->SetBounds(0, 0, 60, 100);

  // Default alignment should stretch child to full available width
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, available_width, available_width), v1->bounds());

  // Stretch alignment should stretch child to full available width
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, available_width, available_width), v1->bounds());

  // Child aligned to start should use preferred area
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStart);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, preferred_width, preferred_width), v1->bounds());

  // Child aligned to center should use preferred area
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(15, 10, preferred_width, preferred_width), v1->bounds());

  // Child aligned to end should use preferred area
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kEnd);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(20, 10, preferred_width, preferred_width), v1->bounds());
}

TEST_F(BoxLayoutTest, CrossAxisAlignmentVerticalChildHugePreferredWidth) {
  // Here we test the case where the child's preferred width is more than the
  // actual available width.
  // In this case the height should be calculated using the available width
  // and not the preferred width.
  const int available_width = 40;

  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));

  auto* v1 = host_->AddChildView(std::make_unique<ProportionallySizedView>(1));
  v1->SetPreferredWidth(100);

  host_->SetBounds(0, 0, 60, 100);

  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, available_width, available_width), v1->bounds());

  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, available_width, available_width), v1->bounds());

  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStart);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, available_width, available_width), v1->bounds());

  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, available_width, available_width), v1->bounds());

  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kEnd);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, available_width, available_width), v1->bounds());
}

TEST_F(BoxLayoutTest, FlexAll) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(10), 10));
  layout->SetDefaultFlex(1);

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  View* v3 = new StaticSizedView(gfx::Size(30, 30));
  host_->AddChildView(v3);
  EXPECT_EQ(gfx::Size(100, 50), layout->GetPreferredSize(host_.get()));

  host_->SetBounds(0, 0, 120, 50);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 27, 30).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(47, 10, 16, 30).ToString(), v2->bounds().ToString());
  EXPECT_EQ(gfx::Rect(73, 10, 37, 30).ToString(), v3->bounds().ToString());
}

TEST_F(BoxLayoutTest, FlexGrowVertical) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  View* v3 = new StaticSizedView(gfx::Size(30, 30));
  host_->AddChildView(v3);

  host_->SetBounds(0, 0, 50, 130);

  // Views don't fill the available space by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 30, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 30, 10).ToString(), v2->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 60, 30, 30).ToString(), v3->bounds().ToString());

  for (auto main_alignment : kMainAlignments) {
    layout->set_main_axis_alignment(main_alignment);

    // Set the first view to consume all free space.
    layout->SetFlexForView(v1, 1);
    layout->ClearFlexForView(v2);
    layout->ClearFlexForView(v3);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 30, 50).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 70, 30, 10).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 90, 30, 30).ToString(), v3->bounds().ToString());

    // Set the third view to take 2/3s of the free space and leave the first
    // view
    // with 1/3.
    layout->SetFlexForView(v3, 2);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 30, 30).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 50, 30, 10).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 70, 30, 50).ToString(), v3->bounds().ToString());

    // Clear the previously set flex values and set the second view to take all
    // the free space.
    layout->ClearFlexForView(v1);
    layout->SetFlexForView(v2, 1);
    layout->ClearFlexForView(v3);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 30, 20).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 40, 30, 40).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 90, 30, 30).ToString(), v3->bounds().ToString());
  }
}

TEST_F(BoxLayoutTest, FlexGrowHorizontalWithRemainder) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
  layout->SetDefaultFlex(1);
  std::vector<View*> views;
  for (int i = 0; i < 5; ++i) {
    View* view = new StaticSizedView(gfx::Size(10, 10));
    views.push_back(view);
    host_->AddChildView(view);
  }

  EXPECT_EQ(gfx::Size(50, 10), layout->GetPreferredSize(host_.get()));

  host_->SetBounds(0, 0, 52, 10);
  host_->Layout();
  // The 2nd and 4th views should have an extra pixel as they correspond to 20.8
  // and 41.6 which round up.
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10).ToString(), views[0]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 0, 11, 10).ToString(), views[1]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(21, 0, 10, 10).ToString(), views[2]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(31, 0, 11, 10).ToString(), views[3]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(42, 0, 10, 10).ToString(), views[4]->bounds().ToString());
}

TEST_F(BoxLayoutTest, FlexGrowHorizontalWithRemainder2) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
  layout->SetDefaultFlex(1);
  std::vector<View*> views;
  for (int i = 0; i < 4; ++i) {
    View* view = new StaticSizedView(gfx::Size(1, 10));
    views.push_back(view);
    host_->AddChildView(view);
  }

  EXPECT_EQ(gfx::Size(4, 10), layout->GetPreferredSize(host_.get()));

  host_->SetBounds(0, 0, 10, 10);
  host_->Layout();
  // The 1st and 3rd views should have an extra pixel as they correspond to 2.5
  // and 7.5 which round up.
  EXPECT_EQ(gfx::Rect(0, 0, 3, 10).ToString(), views[0]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(3, 0, 2, 10).ToString(), views[1]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(5, 0, 3, 10).ToString(), views[2]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(8, 0, 2, 10).ToString(), views[3]->bounds().ToString());
}

TEST_F(BoxLayoutTest, FlexShrinkHorizontal) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(10), 10));

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  View* v3 = new StaticSizedView(gfx::Size(30, 30));
  host_->AddChildView(v3);

  host_->SetBounds(0, 0, 85, 50);

  // Truncate width by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 30).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 30).ToString(), v2->bounds().ToString());
  EXPECT_EQ(gfx::Rect(60, 10, 15, 30).ToString(), v3->bounds().ToString());

  for (auto main_alignment : kMainAlignments) {
    layout->set_main_axis_alignment(main_alignment);

    // Set the first view to shrink as much as necessary.
    layout->SetFlexForView(v1, 1);
    layout->ClearFlexForView(v2);
    layout->ClearFlexForView(v3);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 5, 30).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(25, 10, 10, 30).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(45, 10, 30, 30).ToString(), v3->bounds().ToString());

    // Set the third view to shrink 2/3s of the free space and leave the first
    // view with 1/3.
    layout->SetFlexForView(v3, 2);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 15, 30).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(35, 10, 10, 30).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(55, 10, 20, 30).ToString(), v3->bounds().ToString());

    // Clear the previously set flex values and set the second view to take all
    // the free space with MainAxisAlignment::kEnd set. This causes the second
    // view to shrink to zero and the third view still doesn't fit so it
    // overflows.
    layout->ClearFlexForView(v1);
    layout->SetFlexForView(v2, 2);
    layout->ClearFlexForView(v3);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 20, 30).ToString(), v1->bounds().ToString());
    // Conceptually this view is at 10, 40, 0, 0.
    EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(50, 10, 25, 30).ToString(), v3->bounds().ToString());
  }
}

TEST_F(BoxLayoutTest, FlexShrinkVerticalWithRemainder) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v2);
  View* v3 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v3);
  host_->SetBounds(0, 0, 20, 20);

  for (auto main_alignment : kMainAlignments) {
    layout->set_main_axis_alignment(main_alignment);

    // The first view shrinks by 1/3 of the excess, the second view shrinks by
    // 2/3 of the excess and the third view should maintain its preferred size.
    layout->SetFlexForView(v1, 1);
    layout->SetFlexForView(v2, 2);
    layout->ClearFlexForView(v3);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(0, 0, 20, 3).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 3, 20, 7).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 10, 20, 10).ToString(), v3->bounds().ToString());

    // The second view shrinks to 2/3 of the excess, the third view shrinks to
    // 1/3 of the excess and the first view should maintain its preferred size.
    layout->ClearFlexForView(v1);
    layout->SetFlexForView(v2, 2);
    layout->SetFlexForView(v3, 1);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(0, 0, 20, 10).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 10, 20, 7).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 17, 20, 3).ToString(), v3->bounds().ToString());

    // Each view shrinks equally to fit within the available space.
    layout->SetFlexForView(v1, 1);
    layout->SetFlexForView(v2, 1);
    layout->SetFlexForView(v3, 1);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(0, 0, 20, 3).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 3, 20, 14).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 17, 20, 3).ToString(), v3->bounds().ToString());
  }
}

TEST_F(BoxLayoutTest, MinimumCrossAxisVertical) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  layout->set_minimum_cross_axis_size(30);

  EXPECT_EQ(gfx::Size(30, 10), layout->GetPreferredSize(host_.get()));
}

TEST_F(BoxLayoutTest, MinimumCrossAxisHorizontal) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  layout->set_minimum_cross_axis_size(30);

  EXPECT_EQ(gfx::Size(20, 30), layout->GetPreferredSize(host_.get()));
}

TEST_F(BoxLayoutTest, MarginsUncollapsedHorizontal) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  v1->SetProperty(kMarginsKey, gfx::Insets(5, 5, 5, 5));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 10));
  v2->SetProperty(kMarginsKey, gfx::Insets(6, 4, 6, 4));
  host_->AddChildView(v2);

  EXPECT_EQ(gfx::Size(58, 22), layout->GetPreferredSize(host_.get()));
  host_->SizeToPreferredSize();
  layout->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(5, 5, 20, 12), v1->bounds());
  EXPECT_EQ(gfx::Rect(34, 6, 20, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, MarginsCollapsedHorizontal) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(0, 0), 0, true));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  v1->SetProperty(kMarginsKey, gfx::Insets(5, 5, 5, 5));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 10));
  v2->SetProperty(kMarginsKey, gfx::Insets(6, 4, 6, 4));
  host_->AddChildView(v2);

  EXPECT_EQ(gfx::Size(54, 22), layout->GetPreferredSize(host_.get()));
  host_->SizeToPreferredSize();
  layout->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(5, 5, 20, 12), v1->bounds());
  EXPECT_EQ(gfx::Rect(30, 6, 20, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, MarginsUncollapsedVertical) {
  BoxLayout* layout = host_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  v1->SetProperty(kMarginsKey, gfx::Insets(5, 5, 5, 5));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 10));
  v2->SetProperty(kMarginsKey, gfx::Insets(6, 4, 6, 4));
  host_->AddChildView(v2);

  EXPECT_EQ(gfx::Size(30, 42), layout->GetPreferredSize(host_.get()));
  host_->SizeToPreferredSize();
  layout->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(5, 5, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(4, 26, 22, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, MarginsCollapsedVertical) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(0, 0), 0, true));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  v1->SetProperty(kMarginsKey, gfx::Insets(5, 5, 5, 5));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 10));
  v2->SetProperty(kMarginsKey, gfx::Insets(6, 4, 6, 4));
  host_->AddChildView(v2);

  EXPECT_EQ(gfx::Size(30, 37), layout->GetPreferredSize(host_.get()));
  host_->SizeToPreferredSize();
  layout->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(5, 5, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(4, 21, 22, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, UnbalancedMarginsUncollapsedHorizontal) {
  auto layout_owner =
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  layout_owner->set_cross_axis_alignment(
      BoxLayout::CrossAxisAlignment::kCenter);
  BoxLayout* layout = host_->SetLayoutManager(std::move(layout_owner));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  v1->SetProperty(kMarginsKey, gfx::Insets(5, 5, 4, 4));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 10));
  v2->SetProperty(kMarginsKey, gfx::Insets(6, 4, 3, 6));
  host_->AddChildView(v2);

  EXPECT_EQ(gfx::Size(59, 20), layout->GetPreferredSize(host_.get()));
  host_->SizeToPreferredSize();
  layout->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(5, 5, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(33, 6, 20, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, UnbalancedMarginsCollapsedHorizontal) {
  auto layout_owner = std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(0, 0), 0, true);
  layout_owner->set_cross_axis_alignment(
      BoxLayout::CrossAxisAlignment::kCenter);
  BoxLayout* layout = host_->SetLayoutManager(std::move(layout_owner));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  v1->SetProperty(kMarginsKey, gfx::Insets(5, 5, 4, 4));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 10));
  v2->SetProperty(kMarginsKey, gfx::Insets(6, 4, 3, 6));
  host_->AddChildView(v2);

  EXPECT_EQ(gfx::Size(55, 20), layout->GetPreferredSize(host_.get()));
  host_->SizeToPreferredSize();
  layout->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(5, 5, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(29, 6, 20, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, UnbalancedMarginsUncollapsedVertical) {
  auto layout_owner =
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  layout_owner->set_cross_axis_alignment(
      BoxLayout::CrossAxisAlignment::kCenter);
  BoxLayout* layout = host_->SetLayoutManager(std::move(layout_owner));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  v1->SetProperty(kMarginsKey, gfx::Insets(4, 5, 5, 3));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 10));
  v2->SetProperty(kMarginsKey, gfx::Insets(6, 4, 3, 5));
  host_->AddChildView(v2);

  EXPECT_EQ(gfx::Size(30, 38), layout->GetPreferredSize(host_.get()));
  host_->SizeToPreferredSize();
  layout->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(5, 4, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(5, 25, 20, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, UnbalancedMarginsCollapsedVertical) {
  auto layout_owner = std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(0, 0), 0, true);
  layout_owner->set_cross_axis_alignment(
      BoxLayout::CrossAxisAlignment::kCenter);
  BoxLayout* layout = host_->SetLayoutManager(std::move(layout_owner));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  v1->SetProperty(kMarginsKey, gfx::Insets(4, 5, 5, 3));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 10));
  v2->SetProperty(kMarginsKey, gfx::Insets(6, 4, 3, 5));
  host_->AddChildView(v2);

  EXPECT_EQ(gfx::Size(30, 33), layout->GetPreferredSize(host_.get()));
  host_->SizeToPreferredSize();
  layout->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(5, 4, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(5, 20, 20, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, OverlappingCrossMarginsAlignEnd) {
  {
    BoxLayout* layout = host_->SetLayoutManager(
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kEnd);
    View* v1 = new StaticSizedView(gfx::Size(20, 4));
    v1->SetProperty(kMarginsKey, gfx::Insets(3, 0, 0, 0));
    host_->AddChildView(v1);
    View* v2 = new StaticSizedView(gfx::Size(20, 5));
    v2->SetProperty(kMarginsKey, gfx::Insets(0, 0, 2, 0));
    host_->AddChildView(v2);

    EXPECT_EQ(9, layout->GetPreferredSize(host_.get()).height());
  }
  host_->RemoveAllChildViews(true);
  {
    BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
        BoxLayout::Orientation::kHorizontal, gfx::Insets(0, 0), 0, true));
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kEnd);
    View* v1 = new StaticSizedView(gfx::Size(20, 4));
    v1->SetProperty(kMarginsKey, gfx::Insets(3, 0, 0, 0));
    host_->AddChildView(v1);
    View* v2 = new StaticSizedView(gfx::Size(20, 5));
    v2->SetProperty(kMarginsKey, gfx::Insets(0, 0, 2, 0));
    host_->AddChildView(v2);

    EXPECT_EQ(9, layout->GetPreferredSize(host_.get()).height());
  }
}

TEST_F(BoxLayoutTest, OverlappingCrossMarginsAlignStretch) {
  {
    BoxLayout* layout = host_->SetLayoutManager(
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);
    View* v1 = new StaticSizedView(gfx::Size(20, 4));
    v1->SetProperty(kMarginsKey, gfx::Insets(3, 0, 0, 0));
    host_->AddChildView(v1);
    View* v2 = new StaticSizedView(gfx::Size(20, 5));
    v2->SetProperty(kMarginsKey, gfx::Insets(0, 0, 2, 0));
    host_->AddChildView(v2);

    EXPECT_EQ(10, layout->GetPreferredSize(host_.get()).height());
  }
  host_->RemoveAllChildViews(true);
  {
    BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
        BoxLayout::Orientation::kHorizontal, gfx::Insets(0, 0), 0, true));
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);
    View* v1 = new StaticSizedView(gfx::Size(20, 4));
    v1->SetProperty(kMarginsKey, gfx::Insets(3, 0, 0, 0));
    host_->AddChildView(v1);
    View* v2 = new StaticSizedView(gfx::Size(20, 5));
    v2->SetProperty(kMarginsKey, gfx::Insets(0, 0, 2, 0));
    host_->AddChildView(v2);

    EXPECT_EQ(10, layout->GetPreferredSize(host_.get()).height());
  }
}

TEST_F(BoxLayoutTest, OverlappingCrossMarginsAlignStart) {
  {
    BoxLayout* layout = host_->SetLayoutManager(
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStart);
    View* v1 = new StaticSizedView(gfx::Size(20, 4));
    v1->SetProperty(kMarginsKey, gfx::Insets(0, 0, 3, 0));
    host_->AddChildView(v1);
    View* v2 = new StaticSizedView(gfx::Size(20, 5));
    v2->SetProperty(kMarginsKey, gfx::Insets(2, 0, 0, 0));
    host_->AddChildView(v2);

    EXPECT_EQ(9, layout->GetPreferredSize(host_.get()).height());
  }
  host_->RemoveAllChildViews(true);
  {
    BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
        BoxLayout::Orientation::kHorizontal, gfx::Insets(0, 0), 0, true));
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStart);
    View* v1 = new StaticSizedView(gfx::Size(20, 4));
    v1->SetProperty(kMarginsKey, gfx::Insets(0, 0, 3, 0));
    host_->AddChildView(v1);
    View* v2 = new StaticSizedView(gfx::Size(20, 5));
    v2->SetProperty(kMarginsKey, gfx::Insets(2, 0, 0, 0));
    host_->AddChildView(v2);

    EXPECT_EQ(9, layout->GetPreferredSize(host_.get()).height());
  }
}

TEST_F(BoxLayoutTest, NegativeBetweenChildSpacing) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(), -10));
  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 15));
  host_->AddChildView(v2);

  EXPECT_EQ(25, layout->GetPreferredSize(host_.get()).height());
  EXPECT_EQ(20, layout->GetPreferredSize(host_.get()).width());
  host_->SetBounds(0, 0, 20, 25);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 15), v2->bounds());
}

TEST_F(BoxLayoutTest, MinimumChildSize) {
  BoxLayout* layout = host_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets()));
  StaticSizedView* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  StaticSizedView* v2 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v2);

  v1->set_minimum_size(gfx::Size(10, 20));
  layout->SetFlexForView(v1, 1, true);

  gfx::Size preferred_size = layout->GetPreferredSize(host_.get());
  EXPECT_EQ(40, preferred_size.width());
  EXPECT_EQ(20, preferred_size.height());

  host_->SetBounds(0, 0, 15, 20);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(10, 0, 5, 20), v2->bounds());

  v1->set_minimum_size(gfx::Size(5, 20));

  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 5, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(5, 0, 10, 20), v2->bounds());
}

}  // namespace views
