// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

using base::Optional;
using gfx::Insets;
using gfx::Point;
using gfx::Rect;
using gfx::Size;

class MockView : public View {
 public:
  void SetMinimumSize(const Size& minimum_size) {
    minimum_size_ = minimum_size;
  }

  Size GetMinimumSize() const override {
    return minimum_size_.value_or(GetPreferredSize());
  }

  void SetVisible(bool visible) override {
    View::SetVisible(visible);
    ++set_visible_count_;
  }

  int GetSetVisibleCount() const { return set_visible_count_; }

  void ResetCounts() { set_visible_count_ = 0; }

 private:
  Optional<Size> minimum_size_;
  int set_visible_count_ = 0;
};

// Custom flex rule that snaps a view between its preffered size and half that
// size in each dimension.
Size CustomFlexImpl(bool snap_to_zero,
                    const View* view,
                    const SizeBounds& maximum_size) {
  const Size large_size = view->GetPreferredSize();
  const Size small_size = Size(large_size.width() / 2, large_size.height() / 2);
  int horizontal, vertical;
  if (!maximum_size.width() || *maximum_size.width() > large_size.width()) {
    horizontal = large_size.width();
  } else if (snap_to_zero && *maximum_size.width() < small_size.width()) {
    horizontal = 0;
  } else {
    horizontal = small_size.width();
  }
  if (!maximum_size.height() || *maximum_size.height() > large_size.height()) {
    vertical = large_size.height();
  } else if (snap_to_zero && *maximum_size.height() < small_size.height()) {
    vertical = 0;
  } else {
    vertical = small_size.height();
  }
  return Size(horizontal, vertical);
}

class FlexLayoutTest : public testing::Test {
 public:
  void SetUp() override {
    host_ = std::make_unique<View>();
    layout_ = host_->SetLayoutManager(std::make_unique<FlexLayout>());
  }

  MockView* AddChild(const Size& preferred_size,
                     const Optional<Size>& minimum_size = Optional<Size>(),
                     bool visible = true) {
    return AddChild(host_.get(), preferred_size, minimum_size, visible);
  }

  static MockView* AddChild(
      View* parent,
      const Size& preferred_size,
      const Optional<Size>& minimum_size = Optional<Size>(),
      bool visible = true) {
    MockView* const child = new MockView();
    child->SetPreferredSize(preferred_size);
    if (minimum_size.has_value())
      child->SetMinimumSize(minimum_size.value());
    if (!visible)
      child->SetVisible(false);
    parent->AddChildView(child);
    return child;
  }

  std::vector<Rect> GetChildBounds() const {
    std::vector<Rect> result;
    std::transform(host_->children().cbegin(), host_->children().cend(),
                   std::back_inserter(result),
                   [](const View* v) { return v->bounds(); });
    return result;
  }

 protected:
  // Constants re-used in many tests.
  static constexpr Insets kSmallInsets = Insets(1, 2, 3, 4);
  static constexpr Insets kLayoutInsets = Insets(5, 6, 7, 9);
  static constexpr Insets kLargeInsets = Insets(10, 11, 12, 13);
  static constexpr Size kChild1Size = Size(12, 10);
  static constexpr Size kChild2Size = Size(13, 11);
  static constexpr Size kChild3Size = Size(17, 13);

  // Preferred size or drop out.
  static const FlexSpecification kDropOut;
  static const FlexSpecification kDropOutHighPriority;

  // Scale from preferred down to minimum or zero.
  static const FlexSpecification kFlex1ScaleToMinimum;
  static const FlexSpecification kFlex2ScaleToMinimum;
  static const FlexSpecification kFlex1ScaleToMinimumHighPriority;
  static const FlexSpecification kFlex1ScaleToZero;

  // Scale from a minimum value up to infinity.
  static const FlexSpecification kUnboundedSnapToMinimum;
  static const FlexSpecification kUnboundedScaleToMinimumSnapToZero;
  static const FlexSpecification kUnboundedScaleToZero;
  static const FlexSpecification kUnboundedScaleToMinimum;
  static const FlexSpecification kUnboundedScaleToMinimumHighPriority;

  // Custom flex which scales step-wise.
  static const FlexSpecification kCustomFlex;
  static const FlexSpecification kCustomFlexSnapToZero;

  std::unique_ptr<View> host_;
  FlexLayout* layout_;
};

// static
constexpr Insets FlexLayoutTest::kSmallInsets;
constexpr Insets FlexLayoutTest::kLayoutInsets;
constexpr Insets FlexLayoutTest::kLargeInsets;
constexpr Size FlexLayoutTest::kChild1Size;
constexpr Size FlexLayoutTest::kChild2Size;
constexpr Size FlexLayoutTest::kChild3Size;

const FlexSpecification FlexLayoutTest::kDropOut =
    FlexSpecification::ForSizeRule(MinimumFlexSizeRule::kPreferredSnapToZero,
                                   MaximumFlexSizeRule::kPreferred)
        .WithWeight(0)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kDropOutHighPriority =
    FlexLayoutTest::kDropOut.WithOrder(1);
const FlexSpecification FlexLayoutTest::kFlex1ScaleToZero =
    FlexSpecification::ForSizeRule(MinimumFlexSizeRule::kScaleToZero,
                                   MaximumFlexSizeRule::kPreferred)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kFlex1ScaleToMinimum =
    FlexSpecification::ForSizeRule(MinimumFlexSizeRule::kScaleToMinimum,
                                   MaximumFlexSizeRule::kPreferred)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kFlex2ScaleToMinimum =
    FlexLayoutTest::kFlex1ScaleToMinimum.WithWeight(2);
const FlexSpecification FlexLayoutTest::kFlex1ScaleToMinimumHighPriority =
    FlexLayoutTest::kFlex1ScaleToMinimum.WithOrder(1);
const FlexSpecification FlexLayoutTest::kUnboundedSnapToMinimum =
    FlexSpecification::ForSizeRule(MinimumFlexSizeRule::kPreferredSnapToMinimum,
                                   MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kUnboundedScaleToMinimumSnapToZero =
    FlexSpecification::ForSizeRule(
        MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
        MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kUnboundedScaleToZero =
    FlexSpecification::ForSizeRule(MinimumFlexSizeRule::kScaleToZero,
                                   MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kUnboundedScaleToMinimumHighPriority =
    FlexSpecification::ForSizeRule(MinimumFlexSizeRule::kScaleToMinimum,
                                   MaximumFlexSizeRule::kUnbounded);
const FlexSpecification FlexLayoutTest::kUnboundedScaleToMinimum =
    kUnboundedScaleToMinimumHighPriority.WithOrder(2);

const FlexSpecification FlexLayoutTest::kCustomFlex =
    FlexSpecification::ForCustomRule(
        base::BindRepeating(&CustomFlexImpl, false))
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kCustomFlexSnapToZero =
    FlexSpecification::ForCustomRule(base::BindRepeating(&CustomFlexImpl, true))
        .WithOrder(2);

}  // namespace

// Size Tests ------------------------------------------------------------------

TEST_F(FlexLayoutTest, GetMinimumSize_Empty) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(false);
  EXPECT_EQ(Size(0, 0), host_->GetMinimumSize());
}

TEST_F(FlexLayoutTest, GetMinimumSize_Empty_ViewInsets_Horizontal) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(false);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  EXPECT_EQ(Size(15, 12), host_->GetMinimumSize());
}

TEST_F(FlexLayoutTest, GetMinimumSize_Empty_ViewInsets_Vertical) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(false);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  EXPECT_EQ(Size(15, 12), host_->GetMinimumSize());
}

TEST_F(FlexLayoutTest, GetMinimumSize_Empty_InternalMargin_Collapsed) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  EXPECT_EQ(Size(9, 7), host_->GetMinimumSize());
}

TEST_F(FlexLayoutTest, GetMinimumSize_Empty_InternalMargin_NotCollapsed) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(false);
  layout_->SetInteriorMargin(kLayoutInsets);
  EXPECT_EQ(Size(15, 12), host_->GetMinimumSize());
}

TEST_F(FlexLayoutTest,
       GetMinimumSize_Empty_InternalMargin_DefaultMarginHasNoEffect) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(false);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(11, 11));
  EXPECT_EQ(Size(15, 12), host_->GetMinimumSize());
}

TEST_F(FlexLayoutTest, GetMinimumSize_MinimumCross_Horizontal) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMinimumCrossAxisSize(5);
  EXPECT_EQ(Size(9, 7), host_->GetMinimumSize());
  layout_->SetMinimumCrossAxisSize(10);
  EXPECT_EQ(Size(9, 10), host_->GetMinimumSize());
  host_->SetBorder(CreateEmptyBorder(kSmallInsets));
  EXPECT_EQ(Size(15, 14), host_->GetMinimumSize());
}

TEST_F(FlexLayoutTest, GetMinimumSize_MinimumCross_Vertical) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMinimumCrossAxisSize(5);
  EXPECT_EQ(Size(9, 7), host_->GetMinimumSize());
  layout_->SetMinimumCrossAxisSize(10);
  EXPECT_EQ(Size(10, 7), host_->GetMinimumSize());
  host_->SetBorder(CreateEmptyBorder(kSmallInsets));
  EXPECT_EQ(Size(16, 11), host_->GetMinimumSize());
}

// Visibility and Inclusion Tests ----------------------------------------------

TEST_F(FlexLayoutTest, Layout_VisibilitySetBeforeInstall) {
  // Since our test fixture creates a host and adds the layout manager right
  // away, we need to create our own for this test.
  std::unique_ptr<views::View> host = std::make_unique<views::View>();
  View* child1 =
      AddChild(host.get(), Size(10, 10), base::Optional<Size>(), false);
  View* child2 =
      AddChild(host.get(), Size(10, 10), base::Optional<Size>(), true);
  host->SetLayoutManager(std::make_unique<FlexLayout>());

  host->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());

  child1->SetVisible(true);
  child2->SetVisible(false);

  host->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_VisibilitySetAfterInstall) {
  // Unlike the last test, we'll use the built-in host and layout manager since
  // they're already set up.
  View* child1 = AddChild(Size(10, 10), base::Optional<Size>(), false);
  View* child2 = AddChild(Size(10, 10), base::Optional<Size>(), true);

  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());

  child1->SetVisible(true);
  child2->SetVisible(false);

  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_VisibilitySetBeforeAdd) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size, Optional<Size>(), false);
  View* child3 = AddChild(kChild3Size);

  host_->Layout();
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  EXPECT_EQ(Size(44, 25), host_->GetPreferredSize());

  // This should have no additional effect since the child is already invisible.
  child2->SetVisible(false);
  host_->Layout();
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  EXPECT_EQ(Size(44, 25), host_->GetPreferredSize());

  child2->SetVisible(true);
  host_->Layout();
  std::vector<Rect> expected{Rect(6, 5, 12, 10), Rect(18, 5, 13, 11),
                             Rect(31, 5, 17, 13)};
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, Layout_VisibilitySetAfterAdd) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);

  child2->SetVisible(false);
  host_->Layout();
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  EXPECT_EQ(Size(44, 25), host_->GetPreferredSize());

  child2->SetVisible(true);
  host_->Layout();
  std::vector<Rect> expected{Rect(6, 5, 12, 10), Rect(18, 5, 13, 11),
                             Rect(31, 5, 17, 13)};
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest,
       Layout_ViewVisibilitySetNotContingentOnActualVisibility) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);

  // Layout makes child view invisible due to flex rule.
  host_->SetSize(Size(40, 25));
  host_->Layout();
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  // Preferred size should still reflect child hidden due to flex rule.
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize());

  // Now we will make child explicitly hidden.
  child2->SetVisible(false);
  host_->Layout();
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  EXPECT_EQ(Size(44, 25), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, Layout_Exlcude) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  const View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  const View* child3 = AddChild(kChild3Size);

  layout_->SetChildViewIgnoredByLayout(child2, true);
  child2->SetBounds(3, 3, 3, 3);
  host_->Layout();
  EXPECT_EQ(Rect(3, 3, 3, 3), child2->bounds());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  EXPECT_EQ(Size(44, 25), host_->GetPreferredSize());

  layout_->SetChildViewIgnoredByLayout(child2, false);
  host_->Layout();
  std::vector<Rect> expected{Rect(6, 5, 12, 10), Rect(18, 5, 13, 11),
                             Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize());
}

// Child Positioning Tests -----------------------------------------------------

TEST_F(FlexLayoutTest, LayoutSingleView_Horizontal) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  View* child = AddChild(kChild1Size);
  host_->Layout();
  EXPECT_EQ(Rect(6, 5, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, LayoutSingleView_Vertical) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  View* child = AddChild(kChild1Size);
  host_->Layout();
  EXPECT_EQ(Rect(6, 5, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Horizontal_CrossStart) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  host_->Layout();
  std::vector<Rect> expected{Rect(6, 5, 12, 10), Rect(18, 5, 13, 11),
                             Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Horizontal_CrossCenter) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  host_->Layout();
  std::vector<Rect> expected{Rect(6, 6, 12, 10), Rect(18, 6, 13, 11),
                             Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Horizontal_CrossEnd) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  host_->Layout();
  std::vector<Rect> expected{Rect(6, 8, 12, 10), Rect(18, 7, 13, 11),
                             Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Horizontal_CrossStretch) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  host_->SetSize(Size(100, 25));
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  host_->Layout();
  std::vector<Rect> expected{Rect(6, 5, 12, 13), Rect(18, 5, 13, 13),
                             Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Vertical_CrossStart) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  host_->Layout();
  std::vector<Rect> expected{Rect(6, 5, 12, 10), Rect(6, 15, 13, 11),
                             Rect(6, 26, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(32, 46), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Vertical_CrossCenter) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  host_->Layout();
  std::vector<Rect> expected{Rect(8, 5, 12, 10), Rect(8, 15, 13, 11),
                             Rect(6, 26, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(32, 46), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Vertical_CrossEnd) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  host_->Layout();
  std::vector<Rect> expected{Rect(11, 5, 12, 10), Rect(10, 15, 13, 11),
                             Rect(6, 26, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(32, 46), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Vertical_CrossStretch) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  host_->SetSize(Size(32, 50));
  host_->Layout();
  std::vector<Rect> expected{Rect(6, 5, 17, 10), Rect(6, 15, 17, 11),
                             Rect(6, 26, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(32, 46), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest,
       LayoutMultipleViews_MarginAndSpacing_NoCollapse_Horizontal) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(false);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  host_->Layout();
  std::vector<Rect> expected{Rect(6, 5, 12, 10), Rect(18, 5, 13, 11),
                             Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize());

  child1->SetProperty(views::kMarginsKey, Insets(20, 21, 22, 23));
  host_->InvalidateLayout();
  host_->Layout();
  expected = std::vector<Rect>{Rect(27, 25, 12, 10), Rect(62, 5, 13, 11),
                               Rect(75, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(101, 64), host_->GetPreferredSize());

  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  host_->InvalidateLayout();
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(0, 3));
  host_->Layout();
  expected = std::vector<Rect>{Rect(27, 25, 12, 10), Rect(63, 6, 13, 11),
                               Rect(80, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(109, 64), host_->GetPreferredSize());

  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->InvalidateLayout();
  host_->Layout();
  expected = std::vector<Rect>{Rect(27, 25, 12, 10), Rect(63, 6, 13, 11),
                               Rect(79, 7, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(107, 64), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest,
       LayoutMultipleViews_MarginAndSpacing_NoCollapse_Vertical) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(false);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->InvalidateLayout();
  host_->Layout();
  std::vector<Rect> expected{Rect(27, 25, 12, 10), Rect(7, 58, 13, 11),
                             Rect(8, 72, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(71, 94), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest,
       LayoutMultipleViews_MarginAndSpacing_Collapse_Horizontal) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->InvalidateLayout();
  host_->Layout();
  std::vector<Rect> expected{Rect(21, 20, 12, 10), Rect(56, 5, 13, 11),
                             Rect(71, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(97, 52), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_MarginAndSpacing_Collapse_Vertical) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->InvalidateLayout();
  host_->Layout();
  std::vector<Rect> expected{Rect(21, 20, 12, 10), Rect(6, 52, 13, 11),
                             Rect(6, 65, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(56, 85), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_InteriorPadding) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(10));
  View* child = AddChild(Size(13, 15));
  AddChild(kChild3Size);
  child->SetProperty(views::kInternalPaddingKey, Insets(1, 2, 4, 8));
  host_->InvalidateLayout();
  host_->Layout();
  std::vector<Rect> expected{
      Rect(8, 9, 13, 15),
      Rect(23, 10, 17, 13),
  };
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(50, 33), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_InteriorPadding_Margins) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(2));
  View* child = AddChild(Size(13, 15));
  View* child2 = AddChild(kChild3Size);
  child->SetProperty(views::kInternalPaddingKey, Insets(1, 2, 4, 8));
  child2->SetProperty(views::kMarginsKey, Insets(5, 5, 5, 5));
  host_->InvalidateLayout();
  host_->Layout();
  std::vector<Rect> expected{
      Rect(4, 4, 13, 15),
      Rect(17, 5, 17, 13),
  };
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(43, 25), host_->GetPreferredSize());
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_InteriorPadding_Additive) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(20));
  View* child = AddChild(Size(13, 15));
  View* child2 = AddChild(kChild3Size);
  child->SetProperty(views::kInternalPaddingKey, Insets(1, 2, 4, 8));
  child2->SetProperty(views::kInternalPaddingKey, Insets(5, 5, 5, 5));
  host_->InvalidateLayout();
  host_->Layout();
  std::vector<Rect> expected{
      Rect(18, 19, 13, 15),
      Rect(38, 15, 17, 13),
  };
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(70, 50), host_->GetPreferredSize());
}

// Host insets tests -----------------------------------------------------------

TEST_F(FlexLayoutTest, Layout_HostInsets_Horizontal) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  View* child = AddChild(kChild1Size);
  host_->Layout();
  EXPECT_EQ(Rect(6, 5, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, Layout_HostInsets_Vertical) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  View* child = AddChild(kChild1Size);
  host_->Layout();
  EXPECT_EQ(Rect(6, 5, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, Layout_HostInsets_Horizontal_Leading) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  View* child = AddChild(kChild1Size);
  host_->SetSize({100, 100});
  EXPECT_EQ(Rect(6, 5, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, Layout_HostInsets_Vertical_Leading) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  View* child = AddChild(kChild1Size);
  host_->SetSize({100, 100});
  EXPECT_EQ(Rect(6, 5, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, Layout_HostInsets_Horizontal_Center) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetMainAxisAlignment(LayoutAlignment::kCenter);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  View* child = AddChild(kChild1Size);
  host_->SetSize({100, 100});
  const int expected_x =
      kLayoutInsets.left() +
      (host_->size().width() - kChild1Size.width() - kLayoutInsets.width()) / 2;
  EXPECT_EQ(Rect(expected_x, 5, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, Layout_HostInsets_Vertical_Center) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kCenter);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  View* child = AddChild(kChild1Size);
  host_->SetSize({100, 100});
  const int expected_y =
      kLayoutInsets.top() +
      (host_->size().height() - kChild1Size.height() - kLayoutInsets.height()) /
          2;
  EXPECT_EQ(Rect(6, expected_y, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, Layout_HostInsets_Horizontal_End) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetMainAxisAlignment(LayoutAlignment::kEnd);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  View* child = AddChild(kChild1Size);
  host_->SetSize({100, 100});
  const int expected_x =
      kLayoutInsets.left() +
      (host_->size().width() - kChild1Size.width() - kLayoutInsets.width());
  EXPECT_EQ(Rect(expected_x, 5, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, Layout_HostInsets_Vertical_End) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kEnd);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  View* child = AddChild(kChild1Size);
  host_->SetSize({100, 100});
  const int expected_y =
      kLayoutInsets.top() +
      (host_->size().height() - kChild1Size.height() - kLayoutInsets.height());
  EXPECT_EQ(Rect(6, expected_y, 12, 10), child->bounds());
}

// Include Host Insets Tests ---------------------------------------------------

TEST_F(FlexLayoutTest, SetIncludeHostInsetsInLayout_NoChange) {
  host_->SetBorder(views::CreateEmptyBorder(2, 2, 2, 2));
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(false);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(4));
  constexpr Size kChildSize(10, 10);
  AddChild(kChildSize);
  View* const child2 = AddChild(kChildSize);
  AddChild(kChildSize);
  child2->SetProperty(views::kMarginsKey, gfx::Insets(10));

  const Size expected_preferred_size = host_->GetPreferredSize();
  host_->SetSize(expected_preferred_size);
  const std::vector<Rect> expected_bounds = GetChildBounds();

  layout_->SetIncludeHostInsetsInLayout(true);
  const Size preferred_size = host_->GetPreferredSize();
  EXPECT_EQ(expected_preferred_size, preferred_size);
  host_->Layout();
  EXPECT_EQ(expected_bounds, GetChildBounds());
}

TEST_F(FlexLayoutTest, SetIncludeHostInsetsInLayout_CollapseIntoInsets) {
  host_->SetBorder(views::CreateEmptyBorder(2, 2, 2, 2));
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(4));
  constexpr Size kChildSize(10, 10);
  AddChild(kChildSize);
  View* const child2 = AddChild(kChildSize);
  AddChild(kChildSize);
  child2->SetProperty(views::kMarginsKey, gfx::Insets(15));

  layout_->SetIncludeHostInsetsInLayout(true);
  const Size preferred_size = host_->GetPreferredSize();
  EXPECT_EQ(Size(40, 76), preferred_size);
  host_->SetSize(preferred_size);
  const std::vector<Rect> expected{Rect(19, 7, 10, 10), Rect(15, 32, 10, 10),
                                   Rect(19, 57, 10, 10)};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, SetIncludeHostInsetsInLayout_OverlapInsets) {
  host_->SetBorder(views::CreateEmptyBorder(4, 5, 5, 5));
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* const child = AddChild(Size(10, 10));
  child->SetProperty(views::kInternalPaddingKey, Insets(10, 10, 10, 10));

  layout_->SetIncludeHostInsetsInLayout(true);
  const Size preferred_size = host_->GetPreferredSize();
  EXPECT_EQ(Size(15, 12), preferred_size);
  host_->SetSize(preferred_size);
  EXPECT_EQ(Rect(1, 0, 10, 10), child->bounds());
}

// Default Main Axis Margins Tests ---------------------------------------------

TEST_F(FlexLayoutTest, SetIgnoreDefaultMainAxisMargins_IgnoresDefaultMargins) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(false);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(4));
  constexpr Size kChildSize(10, 10);
  AddChild(kChildSize);
  AddChild(kChildSize);
  AddChild(kChildSize);

  Size preferred_size = host_->GetPreferredSize();
  EXPECT_EQ(Size(33, 66), preferred_size);

  layout_->SetIgnoreDefaultMainAxisMargins(true);
  preferred_size = host_->GetPreferredSize();
  EXPECT_EQ(Size(33, 58), preferred_size);

  host_->SetSize(preferred_size);
  const std::vector<Rect> expected{Rect(10, 5, 10, 10), Rect(10, 23, 10, 10),
                                   Rect(10, 41, 10, 10)};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest,
       SetIgnoreDefaultMainAxisMargins_IncludesExplicitMargins) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(4));
  constexpr Size kChildSize(10, 10);
  View* const child1 = AddChild(kChildSize);
  AddChild(kChildSize);
  View* const child3 = AddChild(kChildSize);

  child1->SetProperty(views::kMarginsKey, gfx::Insets(11));
  child3->SetProperty(views::kMarginsKey, gfx::Insets(12));

  Size preferred_size = host_->GetPreferredSize();
  EXPECT_EQ(Size(34, 76), preferred_size);

  layout_->SetIgnoreDefaultMainAxisMargins(true);
  preferred_size = host_->GetPreferredSize();
  EXPECT_EQ(Size(34, 76), preferred_size);

  host_->SetSize(preferred_size);
  const std::vector<Rect> expected{Rect(11, 11, 10, 10), Rect(6, 32, 10, 10),
                                   Rect(12, 54, 10, 10)};
  EXPECT_EQ(expected, GetChildBounds());
}

// Alignment Tests -------------------------------------------------------------

TEST_F(FlexLayoutTest, Layout_CrossStart) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(kLargeInsets));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->SetSize(Size(200, 200));
  host_->Layout();
  EXPECT_EQ(10, child1->origin().y());
  EXPECT_EQ(5, child2->origin().y());
  EXPECT_EQ(5, child3->origin().y());
}

TEST_F(FlexLayoutTest, Layout_CrossCenter) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(kLargeInsets));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->SetSize(Size(200, 200));
  host_->Layout();
  EXPECT_EQ(94, child1->origin().y());
  EXPECT_EQ(93, child2->origin().y());
  EXPECT_EQ(92, child3->origin().y());
}

TEST_F(FlexLayoutTest, Layout_CrossEnd) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(kLargeInsets));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->SetSize(Size(200, 200));
  host_->Layout();
  EXPECT_EQ(178, child1->origin().y());
  EXPECT_EQ(182, child2->origin().y());
  EXPECT_EQ(180, child3->origin().y());
}

TEST_F(FlexLayoutTest, Layout_CrossStretch) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(kLargeInsets));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->SetSize(Size(200, 200));
  host_->Layout();
  EXPECT_EQ(10, child1->origin().y());
  EXPECT_EQ(5, child2->origin().y());
  EXPECT_EQ(5, child3->origin().y());
  EXPECT_EQ(178, child1->size().height());
  EXPECT_EQ(188, child2->size().height());
  EXPECT_EQ(188, child3->size().height());
}

TEST_F(FlexLayoutTest, Layout_AlignStart) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->SetSize(Size(105, 50));
  host_->Layout();
  std::vector<Rect> expected{Rect(21, 20, 12, 10), Rect(56, 5, 13, 11),
                             Rect(71, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_AlignCenter) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kCenter);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->SetSize(Size(105, 50));
  host_->Layout();
  std::vector<Rect> expected{Rect(25, 20, 12, 10), Rect(60, 5, 13, 11),
                             Rect(75, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_AlignEnd) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kEnd);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->SetSize(Size(105, 50));
  host_->Layout();
  std::vector<Rect> expected{Rect(29, 20, 12, 10), Rect(64, 5, 13, 11),
                             Rect(79, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_AddDroppedMargins) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(false);
  layout_->SetInteriorMargin(Insets(5, 5, 5, 5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(Size(10, 10));
  View* child2 = AddChild(Size(10, 10));
  View* child3 = AddChild(Size(10, 10));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);
  EXPECT_EQ(Size(30, 20), host_->GetMinimumSize());

  host_->SetSize(Size(100, 50));
  host_->Layout();
  std::vector<Rect> expected{Rect(5, 5, 10, 10), Rect(16, 6, 10, 10),
                             Rect(27, 5, 10, 10)};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize(Size(25, 50));
  host_->Layout();
  EXPECT_EQ(Rect(5, 5, 10, 10), child1->bounds());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(15, 5, 10, 10), child3->bounds());
}

TEST_F(FlexLayoutTest, Layout_VerticalAlign_WiderThanTall) {
  // This test ensures we do not regress http://crbug.com/983941
  // Previously, the width of the host view was erroneously used when
  // calculating excess main-axis size, causing center-alignment in vertical
  // layouts in host views that were much wider than tall to be incorrect.
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kCenter);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->SetSize(Size(1000, 100));
  host_->Layout();
  std::vector<Rect> expected{Rect(21, 27, 12, 10), Rect(6, 59, 13, 11),
                             Rect(6, 72, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
}

// Flex Tests ------------------------------------------------------------------

TEST_F(FlexLayoutTest, Layout_IgnoreMinimumSize_DropViews) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(kLargeInsets));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  host_->SetSize(Size(55, 50));
  host_->Layout();
  std::vector<Rect> expected{Rect(11, 10, 12, 10), Rect(36, 5, 13, 11),
                             Rect(51, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());

  child1->SetProperty(views::kFlexBehaviorKey, kDropOut);
  host_->InvalidateLayout();
  EXPECT_EQ(Size(77, 32), host_->GetPreferredSize());
  EXPECT_EQ(Size(47, 25), host_->GetMinimumSize());
  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());
  EXPECT_EQ(Rect(6, 5, 13, 11), child2->bounds());
  EXPECT_EQ(Rect(21, 5, 17, 13), child3->bounds());

  child1->ClearProperty(views::kFlexBehaviorKey);
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);
  host_->InvalidateLayout();
  EXPECT_EQ(Size(77, 32), host_->GetPreferredSize());
  EXPECT_EQ(Size(62, 32), host_->GetMinimumSize());
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());
  EXPECT_EQ(Rect(11, 10, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(36, 5, 17, 13), child3->bounds());

  child2->ClearProperty(views::kFlexBehaviorKey);
  child3->SetProperty(views::kFlexBehaviorKey, kDropOut);
  host_->InvalidateLayout();
  EXPECT_EQ(Size(77, 32), host_->GetPreferredSize());
  EXPECT_EQ(Size(58, 32), host_->GetMinimumSize());
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());
  EXPECT_EQ(Rect(11, 10, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(36, 5, 13, 11), child2->bounds());
}

TEST_F(FlexLayoutTest, Layout_IgnoreMinimumSize_DropInOrder) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(kLargeInsets));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  // Set flex separately; we'll test default flex later.
  child1->SetProperty(views::kFlexBehaviorKey, kDropOut);
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);
  child3->SetProperty(views::kFlexBehaviorKey, kDropOut);
  EXPECT_EQ(Size(9, 7), host_->GetMinimumSize());

  host_->SetSize(Size(100, 50));
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());

  host_->SetSize(Size(58, 50));
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  host_->SetSize(Size(57, 50));
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  // Since there's no room for child1, child2 becomes visible.
  host_->SetSize(Size(28, 50));
  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  host_->SetSize(Size(27, 50));
  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_IgnoreMinimumSize_DropInOrder_DefaultFlex) {
  // Perform the same test as above but with default flex set instead.
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(kLargeInsets));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  layout_->SetDefault(views::kFlexBehaviorKey, kDropOut);
  EXPECT_EQ(Size(9, 7), host_->GetMinimumSize());

  host_->SetSize(Size(100, 50));
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());

  host_->SetSize(Size(58, 50));
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  host_->SetSize(Size(57, 50));
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  // Since there's no room for child1, child2 becomes visible.
  host_->SetSize(Size(28, 50));
  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  host_->SetSize(Size(27, 50));
  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_IgnoreMinimumSize_DropByPriority) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(3));
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  View* child3 = AddChild(kChild3Size);
  child1->SetProperty(views::kMarginsKey, Insets(kLargeInsets));
  child2->SetProperty(views::kMarginsKey, Insets(1, 1, 1, 1));
  child3->SetProperty(views::kMarginsKey, Insets(2, 2, 2, 2));
  layout_->SetDefault(views::kFlexBehaviorKey, kDropOut);
  child3->SetProperty(views::kFlexBehaviorKey, kDropOutHighPriority);
  EXPECT_EQ(Size(9, 7), host_->GetMinimumSize());

  host_->SetSize(Size(100, 50));
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());

  host_->SetSize(Size(65, 50));
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());

  host_->SetSize(Size(40, 50));
  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());

  host_->SetSize(Size(20, 50));
  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_Flex_OneViewScales) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(10, 20), Size(5, 5));
  View* child2 = AddChild(Size(10, 10));
  child1->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToMinimum);

  host_->SetSize(Size(20, 50));
  host_->Layout();
  EXPECT_EQ(Size(10, 20), child1->size());
  EXPECT_EQ(Size(10, 10), child2->size());

  host_->SetSize(Size(20, 35));
  host_->Layout();
  EXPECT_EQ(Size(10, 10), child1->size());
  EXPECT_EQ(Size(10, 10), child2->size());

  host_->SetSize(Size(20, 30));
  host_->Layout();
  EXPECT_EQ(Size(10, 5), child1->size());
  EXPECT_EQ(Size(10, 10), child2->size());
}

TEST_F(FlexLayoutTest, Layout_Flex_OneViewScales_BelowMinimum) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(10, 20), Size(5, 5));
  View* child2 = AddChild(Size(10, 10));
  child1->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToMinimum);

  host_->SetSize(Size(20, 20));
  host_->Layout();
  EXPECT_EQ(Size(10, 5), child1->size());
  EXPECT_EQ(Size(10, 10), child2->size());
}

TEST_F(FlexLayoutTest,
       Layout_Flex_OneViewScales_CausesSubsequentControlToDropOut) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(10, 20), Size(5, 5));
  View* child2 = AddChild(Size(10, 10), Size(5, 5));
  child1->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToMinimum);
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);

  host_->SetSize(Size(20, 20));
  host_->Layout();
  EXPECT_EQ(Size(10, 10), child1->size());
  EXPECT_FALSE(child2->GetVisible());
}

TEST_F(FlexLayoutTest,
       Layout_Flex_OneViewScales_CausesSubsequentFlexControlToDropOut) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(10, 20), Size(5, 5));
  View* child2 = AddChild(Size(10, 10), Size(5, 5));
  child1->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToMinimum);
  child2->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToZero);

  host_->SetSize(Size(20, 19));
  host_->Layout();
  // TODO(dfried): Make this work.
  // EXPECT_EQ(Size(10, 9), child1->size());
  EXPECT_EQ(Size(10, 7), child1->size());
  EXPECT_FALSE(child2->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_EqualWeight) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  layout_->SetDefault(views::kFlexBehaviorKey, kFlex1ScaleToMinimum);
  View* child1 = AddChild(Size(20, 10), Size(5, 5));
  View* child2 = AddChild(Size(20, 10), Size(5, 5));

  host_->SetSize(Size(45, 20));
  host_->Layout();
  EXPECT_EQ(Size(15, 10), child1->size());
  EXPECT_EQ(Size(15, 10), child2->size());

  host_->SetSize(Size(60, 20));
  host_->Layout();
  EXPECT_EQ(Size(20, 10), child1->size());
  EXPECT_EQ(Size(20, 10), child2->size());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_DefaultFlex) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  layout_->SetDefault(views::kFlexBehaviorKey, kFlex1ScaleToMinimum);
  View* child1 = AddChild(Size(20, 10), Size(5, 5));
  View* child2 = AddChild(Size(20, 10), Size(5, 5));

  host_->SetSize(Size(45, 20));
  host_->Layout();
  EXPECT_EQ(Size(15, 10), child1->size());
  EXPECT_EQ(Size(15, 10), child2->size());

  host_->SetSize(Size(60, 20));
  host_->Layout();
  EXPECT_EQ(Size(20, 10), child1->size());
  EXPECT_EQ(Size(20, 10), child2->size());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_UnequalWeight) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(20, 10), Size(5, 5));
  View* child2 = AddChild(Size(20, 10), Size(5, 5));
  child1->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToMinimum);
  child2->SetProperty(views::kFlexBehaviorKey, kFlex2ScaleToMinimum);

  host_->SetSize(Size(45, 20));
  host_->Layout();
  EXPECT_EQ(kChild1Size, child1->size());
  EXPECT_EQ(Size(18, 10), child2->size());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_UnequalWeight_FirstAtMax) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(20, 10), Size(5, 5));
  View* child2 = AddChild(Size(20, 10), Size(5, 5));
  child1->SetProperty(views::kFlexBehaviorKey, kFlex2ScaleToMinimum);
  child2->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToMinimum);

  host_->SetSize(Size(50, 20));
  host_->Layout();
  EXPECT_EQ(Size(20, 10), child1->size());
  EXPECT_EQ(Size(15, 10), child2->size());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_UnequalWeight_SecondAtMax) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(20, 10), Size(5, 5));
  View* child2 = AddChild(Size(20, 10), Size(5, 5));
  child1->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToMinimum);
  child2->SetProperty(views::kFlexBehaviorKey, kFlex2ScaleToMinimum);

  host_->SetSize(Size(50, 20));
  host_->Layout();

  // TODO(dfried): Make this work.
  // EXPECT_EQ(Size(15, 10), child1->size());
  EXPECT_EQ(Size(14, 10), child1->size());
  EXPECT_EQ(Size(20, 10), child2->size());
}

// This is a regression test for a case where a view marked as having flex
// weight but which could not flex larger than its preferred size would cause
// other views at that weight to not receive available flex space.
TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_FirstViewFillsAvailableSpace) {
  const FlexSpecification can_flex = FlexSpecification::ForSizeRule(
      MinimumFlexSizeRule::kPreferred, MaximumFlexSizeRule::kUnbounded);
  const FlexSpecification cannot_flex = FlexSpecification::ForSizeRule(
      MinimumFlexSizeRule::kPreferred, MaximumFlexSizeRule::kPreferred);

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(20, 10));
  View* child2 = AddChild(Size(20, 10));
  child1->SetProperty(views::kFlexBehaviorKey, can_flex);
  child2->SetProperty(views::kFlexBehaviorKey, cannot_flex);

  host_->SetSize(Size(70, 20));
  const std::vector<Rect> expected_bounds{{5, 5, 35, 10}, {45, 5, 20, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_Priority) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(20, 10), Size(5, 5));
  View* child2 = AddChild(Size(20, 10), Size(5, 5));
  child1->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToMinimum);
  child2->SetProperty(views::kFlexBehaviorKey,
                      kFlex1ScaleToMinimumHighPriority);

  host_->SetSize(Size(50, 20));
  host_->Layout();
  EXPECT_EQ(Size(15, 10), child1->size());
  EXPECT_EQ(Size(20, 10), child2->size());

  host_->SetSize(Size(35, 20));
  host_->Layout();
  EXPECT_EQ(Size(5, 10), child1->size());
  EXPECT_EQ(Size(15, 10), child2->size());
}

TEST_F(FlexLayoutTest,
       Layout_Flex_TwoChildViews_Priority_LowerPriorityDropsOut) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(20, 10), Size(5, 5));
  View* child2 = AddChild(Size(20, 10), Size(5, 5));
  child1->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToZero);
  child2->SetProperty(views::kFlexBehaviorKey,
                      kFlex1ScaleToMinimumHighPriority);

  host_->SetSize(Size(35, 20));
  host_->Layout();
  EXPECT_EQ(Size(20, 10), child2->size());
  EXPECT_FALSE(child1->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_FlexRule_UnboundedSnapToMinimum) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child = AddChild(Size(20, 10), Size(5, 5));
  child->SetProperty(views::kFlexBehaviorKey, kUnboundedSnapToMinimum);

  host_->SetSize(Size(35, 25));
  host_->Layout();
  EXPECT_EQ(Size(25, 10), child->size());

  host_->SetSize(Size(30, 25));
  host_->Layout();
  EXPECT_EQ(Size(20, 10), child->size());

  host_->SetSize(Size(29, 25));
  host_->Layout();
  EXPECT_EQ(Size(5, 10), child->size());

  host_->SetSize(Size(25, 10));
  host_->Layout();
  EXPECT_EQ(Size(5, 5), child->size());

  // This is actually less space than the child needs, but its flex rule does
  // not allow it to drop out.
  host_->SetSize(Size(10, 10));
  host_->Layout();
  EXPECT_EQ(Size(5, 5), child->size());
}

TEST_F(FlexLayoutTest, Layout_FlexRule_UnboundedScaleToMinimumSnapToZero) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child = AddChild(Size(20, 10), Size(5, 5));
  child->SetProperty(views::kFlexBehaviorKey,
                     kUnboundedScaleToMinimumSnapToZero);

  host_->SetSize(Size(35, 25));
  host_->Layout();
  EXPECT_EQ(Size(25, 10), child->size());

  host_->SetSize(Size(30, 25));
  host_->Layout();
  EXPECT_EQ(Size(20, 10), child->size());

  host_->SetSize(Size(29, 25));
  host_->Layout();
  EXPECT_EQ(Size(19, 10), child->size());

  host_->SetSize(Size(25, 16));
  host_->Layout();
  EXPECT_EQ(Size(15, 6), child->size());

  // This is too short to display the view, however it has horizontal size, so
  // the view does not drop out.
  host_->SetSize(Size(25, 10));
  host_->Layout();
  EXPECT_TRUE(child->GetVisible());
  EXPECT_EQ(Size(15, 0), child->size());

  host_->SetSize(Size(15, 15));
  host_->Layout();
  EXPECT_EQ(Size(5, 5), child->size());

  host_->SetSize(Size(14, 15));
  host_->Layout();
  EXPECT_FALSE(child->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_FlexRule_UnboundedScaleToZero) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  // Because we are using a flex rule that scales all the way to zero, ensure
  // that the child view's minimum size is *not* respected.
  View* child = AddChild(Size(20, 10), Size(5, 5));
  child->SetProperty(views::kFlexBehaviorKey, kUnboundedScaleToZero);

  host_->SetSize(Size(35, 25));
  host_->Layout();
  EXPECT_EQ(Size(25, 10), child->size());

  host_->SetSize(Size(30, 25));
  host_->Layout();
  EXPECT_EQ(Size(20, 10), child->size());

  host_->SetSize(Size(29, 25));
  host_->Layout();
  EXPECT_EQ(Size(19, 10), child->size());

  host_->SetSize(Size(25, 16));
  host_->Layout();
  EXPECT_EQ(Size(15, 6), child->size());

  // This is too short to display the view, however it has horizontal size, so
  // the view does not drop out.
  host_->SetSize(Size(25, 10));
  host_->Layout();
  EXPECT_TRUE(child->GetVisible());
  EXPECT_EQ(Size(15, 0), child->size());

  host_->SetSize(Size(15, 15));
  host_->Layout();
  EXPECT_EQ(Size(5, 5), child->size());

  host_->SetSize(Size(14, 14));
  host_->Layout();
  EXPECT_EQ(Size(4, 4), child->size());

  host_->SetSize(Size(9, 14));
  host_->Layout();
  EXPECT_FALSE(child->GetVisible());
}

// A higher priority view which can expand past its maximum size should displace
// a lower priority view up to the first view's preferred size.
TEST_F(FlexLayoutTest,
       Layout_FlexRule_TwoPassScaling_PreferredSizeTakesPrecedence) {
  constexpr Size kLargeSize(10, 10);
  constexpr Size kSmallSize(5, 5);
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(kLargeSize, kSmallSize);
  child1->SetProperty(views::kFlexBehaviorKey,
                      kUnboundedScaleToMinimumHighPriority);
  View* child2 = AddChild(kSmallSize);
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);

  // When there is no room for the second view, it drops out.
  host_->SetSize(Size(4, 5));
  host_->Layout();
  EXPECT_EQ(kSmallSize, child1->size());
  EXPECT_FALSE(child2->GetVisible());

  // When the first view has less room than its preferred size, it should still
  // take up all of the space.
  constexpr Size kIntermediateSize(8, 7);
  host_->SetSize(kIntermediateSize);
  host_->Layout();
  EXPECT_EQ(kIntermediateSize, child1->size());
  EXPECT_FALSE(child2->GetVisible());

  // When the first view has more room than its preferred size, but not enough
  // to make room for the second view, the second view still drops out.
  constexpr Size kLargerSize(13, 8);
  host_->SetSize(kLargerSize);
  host_->Layout();
  EXPECT_EQ(kLargerSize, child1->size());
  EXPECT_FALSE(child2->GetVisible());
}

// When a view is allowed to flex above its preferred size, it will still yield
// that additional space to a lower-priority view, if there is space for the
// second view.
TEST_F(FlexLayoutTest, Layout_FlexRule_TwoPassScaling_StopAtPreferredSize) {
  constexpr Size kLargeSize(10, 10);
  constexpr Size kSmallSize(5, 5);
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(kLargeSize, kSmallSize);
  child1->SetProperty(views::kFlexBehaviorKey,
                      kUnboundedScaleToMinimumHighPriority);
  View* child2 = AddChild(kSmallSize);
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);

  constexpr Size kEnoughSpace(kSmallSize.width() + kLargeSize.width(),
                              kLargeSize.height());
  host_->SetSize(kEnoughSpace);
  host_->Layout();
  EXPECT_EQ(kLargeSize, child1->size());
  EXPECT_EQ(kSmallSize, child2->size());
}

// Once lower-priority views have reached their preferred sizes, a
// higher-priority view which can expand past its preferred size should start to
// consume the remaining space.
TEST_F(FlexLayoutTest, Layout_FlexRule_TwoPassScaling_GrowPastPreferredSize) {
  constexpr Size kLargeSize(10, 10);
  constexpr Size kSmallSize(5, 5);
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(kLargeSize, kSmallSize);
  child1->SetProperty(views::kFlexBehaviorKey,
                      kUnboundedScaleToMinimumHighPriority);
  View* child2 = AddChild(kSmallSize);
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);

  constexpr int kExtra = 7;
  constexpr Size kExtraSpace(kSmallSize.width() + kLargeSize.width() + kExtra,
                             kLargeSize.height() + kExtra);
  host_->SetSize(kExtraSpace);
  EXPECT_EQ(Size(kLargeSize.width() + kExtra, kLargeSize.height()),
            child1->size());
  EXPECT_EQ(kSmallSize, child2->size());
}

// If two views can both scale past their preferred size with the same priority,
// once space has been allocated for each's preferred size, additional space
// will be divided according to flex weight.
TEST_F(FlexLayoutTest,
       Layout_FlexRule_GrowPastPreferredSize_TwoViews_SamePriority) {
  constexpr Size kLargeSize(10, 10);
  constexpr Size kSmallSize(5, 5);
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  // Because we are using a flex rule that scales all the way to zero, ensure
  // that the child view's minimum size is *not* respected.
  View* child1 = AddChild(kLargeSize, kSmallSize);
  child1->SetProperty(views::kFlexBehaviorKey,
                      kUnboundedScaleToMinimumHighPriority);
  View* child2 = AddChild(kLargeSize, kSmallSize);
  child2->SetProperty(views::kFlexBehaviorKey,
                      kUnboundedScaleToMinimumHighPriority);

  constexpr int kExtra = 8;
  constexpr Size kExtraSpace(2 * kLargeSize.width() + kExtra,
                             kLargeSize.height());
  host_->SetSize(kExtraSpace);
  EXPECT_EQ(Size(kLargeSize.width() + kExtra / 2, kLargeSize.height()),
            child1->size());
  EXPECT_EQ(Size(kLargeSize.width() + kExtra / 2, kLargeSize.height()),
            child2->size());
}

// If two views can both scale past their preferred size once space has been
// allocated for each's preferred size, additional space will be given to the
// higher-precedence view.
TEST_F(FlexLayoutTest,
       Layout_FlexRule_GrowPastPreferredSize_TwoViews_DifferentPriority) {
  constexpr Size kLargeSize(10, 10);
  constexpr Size kSmallSize(5, 5);
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  // Because we are using a flex rule that scales all the way to zero, ensure
  // that the child view's minimum size is *not* respected.
  View* child1 = AddChild(kLargeSize, kSmallSize);
  child1->SetProperty(views::kFlexBehaviorKey,
                      kUnboundedScaleToMinimumHighPriority);
  View* child2 = AddChild(kLargeSize, kSmallSize);
  child2->SetProperty(views::kFlexBehaviorKey, kUnboundedScaleToMinimum);

  constexpr int kExtra = 8;
  constexpr Size kExtraSpace(2 * kLargeSize.width() + kExtra,
                             kLargeSize.height());
  host_->SetSize(kExtraSpace);
  EXPECT_EQ(Size(kLargeSize.width() + kExtra, kLargeSize.height()),
            child1->size());
  EXPECT_EQ(kLargeSize, child2->size());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_FlexAlignment_Start) {
  const FlexSpecification float_start =
      FlexSpecification::ForSizeRule(MinimumFlexSizeRule::kPreferred,
                                     MaximumFlexSizeRule::kUnbounded)
          .WithAlignment(LayoutAlignment::kStart);

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(10, 10));
  AddChild(Size(10, 10));
  child1->SetProperty(views::kFlexBehaviorKey, float_start);

  host_->SetSize(Size(50, 20));
  const std::vector<Rect> expected_bounds{{5, 5, 10, 10}, {35, 5, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_FlexAlignment_End) {
  const FlexSpecification float_start =
      FlexSpecification::ForSizeRule(MinimumFlexSizeRule::kPreferred,
                                     MaximumFlexSizeRule::kUnbounded)
          .WithAlignment(LayoutAlignment::kEnd);

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(10, 10));
  AddChild(Size(10, 10));
  child1->SetProperty(views::kFlexBehaviorKey, float_start);

  host_->SetSize(Size(50, 20));
  const std::vector<Rect> expected_bounds{{20, 5, 10, 10}, {35, 5, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_FlexAlignment_Center) {
  const FlexSpecification float_start =
      FlexSpecification::ForSizeRule(MinimumFlexSizeRule::kPreferred,
                                     MaximumFlexSizeRule::kUnbounded)
          .WithAlignment(LayoutAlignment::kCenter);

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(10, 10));
  AddChild(Size(10, 10));
  child1->SetProperty(views::kFlexBehaviorKey, float_start);

  host_->SetSize(Size(50, 20));
  const std::vector<Rect> expected_bounds{{12, 5, 10, 10}, {35, 5, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_FlexRule_CustomFlexRule) {
  constexpr int kFullSize = 50;
  constexpr int kHalfSize = 25;

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child = AddChild(Size(kFullSize, kFullSize));
  child->SetProperty(views::kFlexBehaviorKey, kCustomFlex);

  host_->SetSize(Size(100, 100));
  host_->Layout();
  EXPECT_EQ(Size(kFullSize, kFullSize), child->size());

  host_->SetSize(Size(100, 50));
  host_->Layout();
  EXPECT_EQ(Size(kFullSize, kHalfSize), child->size());

  host_->SetSize(Size(50, 100));
  host_->Layout();
  EXPECT_EQ(Size(kHalfSize, kFullSize), child->size());

  host_->SetSize(Size(45, 40));
  host_->Layout();
  EXPECT_EQ(Size(kHalfSize, kHalfSize), child->size());

  // Custom flex rule does not go below half size.
  host_->SetSize(Size(20, 20));
  host_->Layout();
  EXPECT_EQ(Size(kHalfSize, kHalfSize), child->size());
}

TEST_F(FlexLayoutTest, Layout_FlexRule_CustomFlexRule_WithNonFlex) {
  constexpr int kFullSize = 50;
  constexpr int kHalfSize = 25;

  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child = AddChild(Size(kFullSize, kFullSize));
  AddChild(Size(10, 10));
  child->SetProperty(views::kFlexBehaviorKey, kCustomFlex);

  host_->SetSize(Size(100, 100));
  host_->Layout();
  EXPECT_EQ(Size(kFullSize, kFullSize), child->size());

  host_->SetSize(Size(100, 65));
  host_->Layout();
  EXPECT_EQ(Size(kFullSize, kHalfSize), child->size());

  host_->SetSize(Size(50, 100));
  host_->Layout();
  EXPECT_EQ(Size(kHalfSize, kFullSize), child->size());

  host_->SetSize(Size(45, 40));
  host_->Layout();
  EXPECT_EQ(Size(kHalfSize, kHalfSize), child->size());
}

TEST_F(FlexLayoutTest, Layout_FlexRule_CustomFlexRule_ShrinkToZero) {
  constexpr int kFullSize = 50;
  constexpr int kHalfSize = 25;

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child = AddChild(Size(kFullSize, kFullSize));
  child->SetProperty(views::kFlexBehaviorKey, kCustomFlexSnapToZero);

  host_->SetSize(Size(100, 100));
  host_->Layout();
  EXPECT_EQ(Size(kFullSize, kFullSize), child->size());

  host_->SetSize(Size(100, 50));
  host_->Layout();
  EXPECT_EQ(Size(kFullSize, kHalfSize), child->size());

  host_->SetSize(Size(50, 100));
  host_->Layout();
  EXPECT_EQ(Size(kHalfSize, kFullSize), child->size());

  host_->SetSize(Size(45, 40));
  host_->Layout();
  EXPECT_EQ(Size(kHalfSize, kHalfSize), child->size());

  host_->SetSize(Size(20, 20));
  host_->Layout();
  EXPECT_FALSE(child->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_OnlyCallsSetViewVisibilityWhenNecessary) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  MockView* child1 = AddChild(Size(20, 10), Size(5, 5));
  MockView* child2 = AddChild(Size(20, 10), Size(5, 5));
  child1->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToZero);
  child2->SetProperty(views::kFlexBehaviorKey,
                      kFlex1ScaleToMinimumHighPriority);

  child1->ResetCounts();
  child2->ResetCounts();
  host_->SetSize(Size(40, 20));
  host_->Layout();
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(0, child1->GetSetVisibleCount());
  EXPECT_EQ(0, child2->GetSetVisibleCount());

  host_->SetSize(Size(35, 20));
  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(1, child1->GetSetVisibleCount());
  EXPECT_EQ(0, child2->GetSetVisibleCount());

  child1->ResetCounts();
  child2->ResetCounts();
  host_->SetSize(Size(30, 20));
  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(0, child1->GetSetVisibleCount());
  EXPECT_EQ(0, child2->GetSetVisibleCount());

  child1->SetVisible(false);
  child1->ResetCounts();

  host_->SetSize(Size(40, 20));
  host_->Layout();
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(0, child1->GetSetVisibleCount());
  EXPECT_EQ(0, child2->GetSetVisibleCount());
}

TEST_F(FlexLayoutTest, Between_Child_Spacing) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetInteriorMargin(Insets(7));
  layout_->SetBetweenChildSpacing(8);
  View* v1 = AddChild(gfx::Size(10, 20));
  View* v2 = AddChild(gfx::Size(10, 20));
  EXPECT_EQ(gfx::Size(42, 34), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 100, 100);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(7, 7, 10, 86), v1->bounds());
  EXPECT_EQ(gfx::Rect(25, 7, 10, 86), v2->bounds());
}

TEST_F(FlexLayoutTest, Between_Child_Spacing_With_Margins) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetDefault(views::kMarginsKey, Insets(7));
  layout_->SetBetweenChildSpacing(8);
  View* v1 = AddChild(gfx::Size(10, 20));
  View* v2 = AddChild(gfx::Size(10, 20));
  EXPECT_EQ(gfx::Size(56, 34), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 100, 100);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(7, 7, 10, 86), v1->bounds());
  EXPECT_EQ(gfx::Rect(39, 7, 10, 86), v2->bounds());
}

TEST_F(FlexLayoutTest, Between_Child_Spacing_With_Margins_Collapsed) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetDefault(views::kMarginsKey, Insets(7));
  layout_->SetBetweenChildSpacing(8);
  layout_->SetCollapseMargins(true);
  View* v1 = AddChild(gfx::Size(10, 20));
  View* v2 = AddChild(gfx::Size(10, 20));
  EXPECT_EQ(gfx::Size(42, 34), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 100, 100);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(7, 7, 10, 86), v1->bounds());
  EXPECT_EQ(gfx::Rect(25, 7, 10, 86), v2->bounds());
}

// Cross-axis Fit Tests --------------------------------------------------------

// Tests for cross-axis alignment that checks three different conditions:
//  - child1 fits entirely in the space provided, with margins
//  - child2 fits in the space, but its margins don't
//  - child3 does not fit in the space provided
class FlexLayoutCrossAxisFitTest : public FlexLayoutTest {
 public:
  void SetUp() override {
    FlexLayoutTest::SetUp();
    DCHECK(child_views_.empty());

    for (size_t i = 0; i < kNumChildren; ++i) {
      View* const child = AddChild(kChildSizes[i]);
      child->SetProperty(kMarginsKey, gfx::Insets(kChildMargins[i]));
      child_views_.push_back(child);
    }

    layout_->SetOrientation(LayoutOrientation::kHorizontal);
    layout_->SetCollapseMargins(true);
    layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
    host_->SetSize(kHostSize);
  }

  void TearDown() override { child_views_.clear(); }

 protected:
  static constexpr size_t kNumChildren = 3;
  static constexpr gfx::Size kHostSize = gfx::Size(200, 20);
  static constexpr gfx::Size kChildSizes[kNumChildren] = {{10, 10},
                                                          {10, 10},
                                                          {10, 30}};
  static constexpr gfx::Insets kChildMargins[kNumChildren] = {{6, 0, 2, 0},
                                                              {10, 0, 5, 0},
                                                              {6, 0, 2, 0}};

  std::vector<View*> child_views_;
};

// static
constexpr gfx::Size FlexLayoutCrossAxisFitTest::kHostSize;
constexpr gfx::Size FlexLayoutCrossAxisFitTest::kChildSizes[kNumChildren];
constexpr gfx::Insets FlexLayoutCrossAxisFitTest::kChildMargins[kNumChildren];

TEST_F(FlexLayoutCrossAxisFitTest, Layout_CrossStretch) {
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  host_->Layout();

  // Expect child views to respect their leading margin and to occupy all
  // available space (other than margins), with a minimum size of zero.
  for (size_t i = 0; i < kNumChildren; ++i) {
    EXPECT_EQ(kChildMargins[i].top(), child_views_[i]->origin().y());
    const int expected_height =
        std::max(0, kHostSize.height() - kChildMargins[i].height());
    EXPECT_EQ(expected_height, child_views_[i]->size().height());
  }
}

TEST_F(FlexLayoutCrossAxisFitTest, Layout_CrossStart) {
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  host_->Layout();

  // These should all justify to the leading edge and keep their original size.
  for (size_t i = 0; i < kNumChildren; ++i) {
    EXPECT_EQ(kChildMargins[i].top(), child_views_[i]->origin().y());
    EXPECT_EQ(kChildSizes[i].height(), child_views_[i]->size().height());
  }
}

TEST_F(FlexLayoutCrossAxisFitTest, Layout_CrossCenter) {
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  host_->Layout();

  // First child view fits entirely in the host view with margins (18 DIPs).
  // The entire height (including margins) will be centered vertically.
  int remain = kHostSize.height() -
               (kChildSizes[0].height() + kChildMargins[0].height());
  int expected = remain / 2 + kChildMargins[0].top();
  EXPECT_EQ(expected, child_views_[0]->origin().y());

  // Second child view is smaller than the host view, but margins don't fit.
  // The margins will be scaled down.
  remain = kHostSize.height() - kChildSizes[0].height();
  expected = std::roundf(kChildMargins[1].top() * float{remain} /
                         float{kChildMargins[1].height()});
  EXPECT_EQ(expected, child_views_[1]->origin().y());

  // Third child view does not fit, so is centered.
  remain = kHostSize.height() - kChildSizes[2].height();
  expected = std::ceilf(remain * 0.5f);
  EXPECT_EQ(expected, child_views_[2]->origin().y());

  // Expect child views to retain their preferred sizes.
  for (size_t i = 0; i < kNumChildren; ++i)
    EXPECT_EQ(kChildSizes[i].height(), child_views_[i]->size().height());
}

TEST_F(FlexLayoutCrossAxisFitTest, Layout_CrossEnd) {
  layout_->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  host_->Layout();

  // These should all justify to the trailing edge and keep their original size.
  for (size_t i = 0; i < kNumChildren; ++i) {
    EXPECT_EQ(kHostSize.height() - kChildMargins[i].bottom(),
              child_views_[i]->bounds().bottom());
    EXPECT_EQ(kChildSizes[i].height(), child_views_[i]->size().height());
  }
}

// Nested Layout Tests ---------------------------------------------------------

class NestedFlexLayoutTest : public FlexLayoutTest {
 public:
  void AddChildren(size_t num_children) {
    for (size_t i = 0; i < num_children; ++i) {
      auto v = std::make_unique<View>();
      FlexLayout* layout = v->SetLayoutManager(std::make_unique<FlexLayout>());
      children_.push_back(v.get());
      layouts_.push_back(layout);
      host_->AddChildView(std::move(v));
    }
  }

  View* AddGrandchild(
      size_t child_index,
      const gfx::Size& preferred,
      const base::Optional<gfx::Size>& minimum = base::nullopt) {
    return AddChild(children_[child_index - 1], preferred, minimum);
  }

  View* child(size_t child_index) const { return children_[child_index - 1]; }

  FlexLayout* layout(size_t child_index) const {
    return layouts_[child_index - 1];
  }

  View* grandchild(size_t child_index, size_t grandchild_index) const {
    return children_[child_index - 1]->children()[grandchild_index - 1];
  }

 private:
  std::vector<FlexLayout*> layouts_;
  View::Views children_;
};

TEST_F(NestedFlexLayoutTest, SetVisible_UpdatesLayout) {
  AddChildren(1);
  AddGrandchild(1, gfx::Size(5, 5));
  AddGrandchild(1, gfx::Size(5, 5));

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout(1)->SetOrientation(LayoutOrientation::kHorizontal);
  EXPECT_EQ(gfx::Size(10, 5), host_->GetPreferredSize());
  grandchild(1, 1)->SetVisible(false);
  EXPECT_EQ(gfx::Size(5, 5), host_->GetPreferredSize());
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), grandchild(1, 2)->bounds());
}

TEST_F(NestedFlexLayoutTest, AddChild_UpdatesLayout) {
  AddChildren(1);
  AddGrandchild(1, gfx::Size(5, 5));

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout(1)->SetOrientation(LayoutOrientation::kHorizontal);
  EXPECT_EQ(gfx::Size(5, 5), host_->GetPreferredSize());
  AddGrandchild(1, gfx::Size(5, 5));
  EXPECT_EQ(gfx::Size(10, 5), host_->GetPreferredSize());
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 10, 5), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), grandchild(1, 1)->bounds());
  EXPECT_EQ(gfx::Rect(5, 0, 5, 5), grandchild(1, 2)->bounds());
}

TEST_F(NestedFlexLayoutTest, RemoveChild_UpdatesLayout) {
  AddChildren(1);
  AddGrandchild(1, gfx::Size(5, 5));
  AddGrandchild(1, gfx::Size(5, 5));

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout(1)->SetOrientation(LayoutOrientation::kHorizontal);
  EXPECT_EQ(gfx::Size(10, 5), host_->GetPreferredSize());

  // Remove one grandchild view, avoiding a memory leak since the view is no
  // longer owned.
  View* const to_remove = grandchild(1, 2);
  child(1)->RemoveChildView(to_remove);
  delete to_remove;

  EXPECT_EQ(gfx::Size(5, 5), host_->GetPreferredSize());
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), grandchild(1, 1)->bounds());
}

TEST_F(NestedFlexLayoutTest, Layout_OppositeOrientation) {
  AddChildren(2);
  AddGrandchild(1, gfx::Size(5, 5));
  AddGrandchild(1, gfx::Size(5, 5));
  AddGrandchild(2, gfx::Size(6, 6));
  AddGrandchild(2, gfx::Size(6, 6));

  layout_->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(false)
      .SetCrossAxisAlignment(LayoutAlignment::kStart)
      .SetDefault(views::kMarginsKey, gfx::Insets(2, 3, 4, 5))
      .SetInteriorMargin(gfx::Insets(4, 3, 2, 1));

  layout(1)
      ->SetOrientation(LayoutOrientation::kVertical)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(2))
      .SetInteriorMargin(gfx::Insets(1));

  layout(2)
      ->SetOrientation(LayoutOrientation::kVertical)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(1))
      .SetInteriorMargin(gfx::Insets(2));

  EXPECT_EQ(gfx::Size(39, 29), host_->GetPreferredSize());
  host_->SetSize(gfx::Size(50, 30));
  host_->Layout();
  EXPECT_EQ(gfx::Rect(6, 6, 9, 16), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(23, 6, 10, 17), child(2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 5, 5), grandchild(1, 1)->bounds());
  EXPECT_EQ(gfx::Rect(2, 9, 5, 5), grandchild(1, 2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 6, 6), grandchild(2, 1)->bounds());
  EXPECT_EQ(gfx::Rect(2, 9, 6, 6), grandchild(2, 2)->bounds());
}

TEST_F(NestedFlexLayoutTest, Layout_SameOrientation) {
  AddChildren(2);
  AddGrandchild(1, gfx::Size(5, 5));
  AddGrandchild(1, gfx::Size(5, 5));
  AddGrandchild(2, gfx::Size(6, 6));
  AddGrandchild(2, gfx::Size(6, 6));

  layout_->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(false)
      .SetCrossAxisAlignment(LayoutAlignment::kStart)
      .SetDefault(views::kMarginsKey, gfx::Insets(2, 3, 4, 5))
      .SetInteriorMargin(gfx::Insets(4, 3, 2, 1));

  layout(1)
      ->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(2))
      .SetInteriorMargin(gfx::Insets(1));

  layout(2)
      ->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(1))
      .SetInteriorMargin(gfx::Insets(2));

  EXPECT_EQ(gfx::Size(53, 22), host_->GetPreferredSize());
  host_->SetSize(gfx::Size(60, 30));
  host_->Layout();
  EXPECT_EQ(gfx::Rect(6, 6, 16, 9), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(30, 6, 17, 10), child(2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 5, 5), grandchild(1, 1)->bounds());
  EXPECT_EQ(gfx::Rect(9, 2, 5, 5), grandchild(1, 2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 6, 6), grandchild(2, 1)->bounds());
  EXPECT_EQ(gfx::Rect(9, 2, 6, 6), grandchild(2, 2)->bounds());
}

TEST_F(NestedFlexLayoutTest, Layout_Flex) {
  const FlexSpecification flex_specification = FlexSpecification::ForSizeRule(
      MinimumFlexSizeRule::kScaleToZero, MaximumFlexSizeRule::kPreferred);

  AddChildren(2);
  AddGrandchild(1, gfx::Size(5, 5));
  AddGrandchild(1, gfx::Size(5, 5));
  AddGrandchild(2, gfx::Size(6, 6));
  AddGrandchild(2, gfx::Size(6, 6));

  layout_->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetCrossAxisAlignment(LayoutAlignment::kStart)
      .SetDefault(views::kMarginsKey, gfx::Insets(2))
      .SetInteriorMargin(gfx::Insets(2));
  child(1)->SetProperty(views::kFlexBehaviorKey, flex_specification);
  child(2)->SetProperty(views::kFlexBehaviorKey, flex_specification);

  layout(1)
      ->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(2))
      .SetInteriorMargin(gfx::Insets(2));
  grandchild(1, 1)->SetProperty(views::kFlexBehaviorKey, flex_specification);
  grandchild(1, 2)->SetProperty(views::kFlexBehaviorKey, flex_specification);

  layout(2)
      ->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(2))
      .SetInteriorMargin(gfx::Insets(2));
  grandchild(2, 1)->SetProperty(views::kFlexBehaviorKey, flex_specification);
  grandchild(2, 2)->SetProperty(views::kFlexBehaviorKey, flex_specification);

  EXPECT_EQ(gfx::Size(40, 14), host_->GetPreferredSize());
  host_->SetSize(gfx::Size(20, 15));
  host_->Layout();
  EXPECT_EQ(gfx::Rect(2, 2, 7, 9), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(11, 2, 7, 10), child(2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 1, 5), grandchild(1, 1)->bounds());
  EXPECT_FALSE(grandchild(1, 2)->GetVisible());
  EXPECT_EQ(gfx::Rect(2, 2, 1, 6), grandchild(2, 1)->bounds());
  EXPECT_FALSE(grandchild(2, 2)->GetVisible());

  host_->SetSize(gfx::Size(25, 15));
  host_->Layout();
  EXPECT_EQ(gfx::Rect(2, 2, 10, 9), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(14, 2, 9, 10), child(2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 2, 5), grandchild(1, 1)->bounds());
  EXPECT_EQ(gfx::Rect(6, 2, 2, 5), grandchild(1, 2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 2, 6), grandchild(2, 1)->bounds());
  EXPECT_EQ(gfx::Rect(6, 2, 1, 6), grandchild(2, 2)->bounds());
}

}  // namespace views
