// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

using gfx::Insets;
using gfx::Point;
using gfx::Rect;
using gfx::Size;
using std::optional;

class MockView : public View {
  METADATA_HEADER(MockView, View)

 public:
  enum class SizeMode { kUsePreferredSize, kFixedArea };

  void set_preferred_size(gfx::Size preferred_size) {
    preferred_size_ = preferred_size;
  }

  void SetMinimumSize(const Size& minimum_size) {
    minimum_size_ = minimum_size;
  }

  Size GetMinimumSize() const override {
    return minimum_size_.value_or(GetPreferredSize({}));
  }

  void SetMaximumSize(gfx::Size maximum_size) { maximum_size_ = maximum_size; }

  Size GetMaximumSize() const override { return maximum_size_; }

  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override {
    gfx::Size preferred_size =
        preferred_size_ ? preferred_size_.value()
                        : View::CalculatePreferredSize(available_size);
    switch (size_mode_) {
      case SizeMode::kUsePreferredSize:
        return preferred_size;
      case SizeMode::kFixedArea: {
        int width = available_size.width().min_of(preferred_size.width());
        if (width <= 0) {
          return preferred_size;
        }

        width = std::max(width, minimum_size_ ? minimum_size_->width() : 0);
        return gfx::Size(
            width, (preferred_size.width() * preferred_size.height()) / width);
      }
    }
  }

  void set_size_mode(SizeMode size_mode) { size_mode_ = size_mode; }

  void SetVisible(bool visible) override {
    View::SetVisible(visible);
    ++set_visible_count_;
  }

  int GetSetVisibleCount() const { return set_visible_count_; }

  void ResetCounts() { set_visible_count_ = 0; }

 private:
  std::optional<gfx::Size> preferred_size_;
  optional<Size> minimum_size_;
  gfx::Size maximum_size_;
  int set_visible_count_ = 0;
  SizeMode size_mode_ = SizeMode::kUsePreferredSize;
};

BEGIN_METADATA(MockView)
ADD_PROPERTY_METADATA(gfx::Size, MaximumSize)
END_METADATA

// Custom flex rule that snaps a view between its preferred size and half that
// size in each dimension.
Size CustomFlexImpl(bool snap_to_zero,
                    const View* view,
                    const SizeBounds& maximum_size) {
  const Size large_size = view->GetPreferredSize({});
  const Size small_size = Size(large_size.width() / 2, large_size.height() / 2);
  int horizontal = 0;
  if (maximum_size.width() >= large_size.width())
    horizontal = large_size.width();
  else if (maximum_size.width() >= small_size.width() || !snap_to_zero)
    horizontal = small_size.width();
  int vertical = 0;
  if (maximum_size.height() >= large_size.height())
    vertical = large_size.height();
  else if (maximum_size.height() >= small_size.height() || !snap_to_zero)
    vertical = small_size.height();
  return Size(horizontal, vertical);
}

class FlexLayoutTest : public testing::Test {
 public:
  void SetUp() override {
    host_ = std::make_unique<View>();
    layout_ = host_->SetLayoutManager(std::make_unique<FlexLayout>());
  }

  MockView* AddChild(const Size& preferred_size,
                     const optional<Size>& minimum_size = optional<Size>(),
                     bool visible = true) {
    return AddChild(host_.get(), preferred_size, minimum_size, visible);
  }

  static MockView* AddChild(
      View* parent,
      const Size& preferred_size,
      const optional<Size>& minimum_size = optional<Size>(),
      bool visible = true) {
    MockView* const child = new MockView();
    child->set_preferred_size(preferred_size);
    if (minimum_size.has_value())
      child->SetMinimumSize(minimum_size.value());
    if (!visible)
      child->SetVisible(false);
    parent->AddChildView(child);
    return child;
  }

  std::vector<Rect> GetChildBounds() const {
    std::vector<Rect> result;
    base::ranges::transform(
        host_->children(), std::back_inserter(result), [](const View* v) {
          return v->GetVisible() ? v->bounds() : gfx::Rect();
        });
    return result;
  }

 protected:
  // Constants re-used in many tests.
  static constexpr Insets kSmallInsets = Insets::TLBR(1, 2, 3, 4);
  static constexpr Insets kLayoutInsets = Insets::TLBR(5, 6, 7, 9);
  static constexpr Insets kLargeInsets = Insets::TLBR(10, 11, 12, 13);
  static constexpr Size kChild1Size = Size(12, 10);
  static constexpr Size kChild2Size = Size(13, 11);
  static constexpr Size kChild3Size = Size(17, 13);

  // Use preferred size, but adjust height for width.
  static const FlexSpecification kPreferredAdjustHeight;

  // Preferred size or drop out.
  static const FlexSpecification kDropOut;
  static const FlexSpecification kDropOutHighPriority;

  // Scale from preferred down to minimum or zero.
  static const FlexSpecification kFlex1ScaleToMinimum;
  static const FlexSpecification kFlex2ScaleToMinimum;
  static const FlexSpecification kFlex1ScaleToMinimumHighPriority;
  static const FlexSpecification kFlex1ScaleToZero;

  // Scale from a minimum value up to infinity.
  static const FlexSpecification kUnbounded;
  static const FlexSpecification kUnboundedSnapToMinimum;
  static const FlexSpecification kUnboundedSnapToZero;
  static const FlexSpecification kUnboundedScaleToMinimumSnapToZero;
  static const FlexSpecification kUnboundedScaleToZero;
  static const FlexSpecification kUnboundedScaleToZeroAdjustHeight;
  static const FlexSpecification kUnboundedScaleToMinimum;
  static const FlexSpecification kUnboundedScaleToMinimumHighPriority;

  // Scale from a minimum value up to infinity, but only on the horizontal axis.
  static const FlexSpecification kUnboundedSnapToMinimumHorizontal;
  static const FlexSpecification kUnboundedScaleToMinimumSnapToZeroHorizontal;
  static const FlexSpecification kUnboundedScaleToZeroHorizontal;

  // Scale from a minimum value up to a maximum value.
  static const FlexSpecification kScaleToMaximum;

  // Custom flex which scales step-wise.
  static const FlexSpecification kCustomFlex;
  static const FlexSpecification kCustomFlexSnapToZero;

  std::unique_ptr<View> host_;
  raw_ptr<FlexLayout> layout_;
};

// static
constexpr Insets FlexLayoutTest::kSmallInsets;
constexpr Insets FlexLayoutTest::kLayoutInsets;
constexpr Insets FlexLayoutTest::kLargeInsets;
constexpr Size FlexLayoutTest::kChild1Size;
constexpr Size FlexLayoutTest::kChild2Size;
constexpr Size FlexLayoutTest::kChild3Size;

const FlexSpecification FlexLayoutTest::kPreferredAdjustHeight =
    FlexSpecification(MinimumFlexSizeRule::kPreferred,
                      MaximumFlexSizeRule::kPreferred,
                      true)
        .WithWeight(0);
const FlexSpecification FlexLayoutTest::kDropOut =
    FlexSpecification(MinimumFlexSizeRule::kPreferredSnapToZero,
                      MaximumFlexSizeRule::kPreferred)
        .WithWeight(0)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kDropOutHighPriority =
    FlexLayoutTest::kDropOut.WithOrder(1);
const FlexSpecification FlexLayoutTest::kFlex1ScaleToZero =
    FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                      MaximumFlexSizeRule::kPreferred)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kFlex1ScaleToMinimum =
    FlexSpecification(MinimumFlexSizeRule::kScaleToMinimum,
                      MaximumFlexSizeRule::kPreferred)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kFlex2ScaleToMinimum =
    FlexLayoutTest::kFlex1ScaleToMinimum.WithWeight(2);
const FlexSpecification FlexLayoutTest::kFlex1ScaleToMinimumHighPriority =
    FlexLayoutTest::kFlex1ScaleToMinimum.WithOrder(1);
const FlexSpecification FlexLayoutTest::kUnbounded =
    FlexSpecification(MinimumFlexSizeRule::kPreferred,
                      MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kUnboundedSnapToMinimum =
    FlexSpecification(MinimumFlexSizeRule::kPreferredSnapToMinimum,
                      MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kUnboundedSnapToZero =
    FlexSpecification(MinimumFlexSizeRule::kPreferredSnapToZero,
                      MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kUnboundedScaleToMinimumSnapToZero =
    FlexSpecification(MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                      MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kUnboundedScaleToZero =
    FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                      MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kUnboundedScaleToZeroAdjustHeight =
    FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                      MaximumFlexSizeRule::kUnbounded,
                      true)
        .WithOrder(2);
const FlexSpecification FlexLayoutTest::kUnboundedScaleToMinimumHighPriority(
    MinimumFlexSizeRule::kScaleToMinimum,
    MaximumFlexSizeRule::kUnbounded);
const FlexSpecification FlexLayoutTest::kUnboundedScaleToMinimum =
    kUnboundedScaleToMinimumHighPriority.WithOrder(2);

const FlexSpecification FlexLayoutTest::kUnboundedSnapToMinimumHorizontal =
    FlexSpecification(LayoutOrientation::kHorizontal,
                      MinimumFlexSizeRule::kPreferredSnapToMinimum,
                      MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);
const FlexSpecification
    FlexLayoutTest::kUnboundedScaleToMinimumSnapToZeroHorizontal =
        FlexSpecification(LayoutOrientation::kHorizontal,
                          MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                          MaximumFlexSizeRule::kUnbounded)
            .WithOrder(2);
const FlexSpecification FlexLayoutTest::kUnboundedScaleToZeroHorizontal =
    FlexSpecification(LayoutOrientation::kHorizontal,
                      MinimumFlexSizeRule::kScaleToZero,
                      MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);

const FlexSpecification FlexLayoutTest::kScaleToMaximum =
    FlexSpecification(MinimumFlexSizeRule::kPreferred,
                      MaximumFlexSizeRule::kScaleToMaximum)
        .WithOrder(2);

const FlexSpecification FlexLayoutTest::kCustomFlex =
    FlexSpecification(base::BindRepeating(&CustomFlexImpl, false)).WithOrder(2);
const FlexSpecification FlexLayoutTest::kCustomFlexSnapToZero =
    FlexSpecification(base::BindRepeating(&CustomFlexImpl, true)).WithOrder(2);

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
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(11));
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
      AddChild(host.get(), Size(10, 10), std::optional<Size>(), false);
  View* child2 =
      AddChild(host.get(), Size(10, 10), std::optional<Size>(), true);
  host->SetLayoutManager(std::make_unique<FlexLayout>());

  test::RunScheduledLayout(host.get());
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());

  child1->SetVisible(true);
  child2->SetVisible(false);

  test::RunScheduledLayout(host.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_VisibilitySetAfterInstall) {
  // Unlike the last test, we'll use the built-in host and layout manager since
  // they're already set up.
  View* child1 = AddChild(Size(10, 10), std::optional<Size>(), false);
  View* child2 = AddChild(Size(10, 10), std::optional<Size>(), true);

  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());

  child1->SetVisible(true);
  child2->SetVisible(false);

  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_VisibilitySetBeforeAdd) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size, optional<Size>(), false);
  View* child3 = AddChild(kChild3Size);

  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  EXPECT_EQ(Size(44, 25), host_->GetPreferredSize({}));

  // This should have no additional effect since the child is already invisible.
  child2->SetVisible(false);
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  EXPECT_EQ(Size(44, 25), host_->GetPreferredSize({}));

  child2->SetVisible(true);
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(6, 5, 12, 10), Rect(18, 5, 13, 11),
                                Rect(31, 5, 17, 13)};
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize({}));
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
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  EXPECT_EQ(Size(44, 25), host_->GetPreferredSize({}));

  child2->SetVisible(true);
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(6, 5, 12, 10), Rect(18, 5, 13, 11),
                                Rect(31, 5, 17, 13)};
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize({}));
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
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  // Preferred size should still reflect child hidden due to flex rule.
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize({}));

  // Now we will make child explicitly hidden.
  child2->SetVisible(false);
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  EXPECT_EQ(Size(44, 25), host_->GetPreferredSize({}));
}

TEST_F(FlexLayoutTest, Layout_Exlcude) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  const View* child1 = AddChild(kChild1Size);
  View* child2 = AddChild(kChild2Size);
  const View* child3 = AddChild(kChild3Size);

  child2->SetProperty(kViewIgnoredByLayoutKey, true);
  child2->SetBounds(3, 3, 3, 3);
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Rect(3, 3, 3, 3), child2->bounds());
  EXPECT_EQ(Rect(6, 5, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(18, 5, 17, 13), child3->bounds());
  EXPECT_EQ(Size(44, 25), host_->GetPreferredSize({}));

  child2->SetProperty(kViewIgnoredByLayoutKey, false);
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(6, 5, 12, 10), Rect(18, 5, 13, 11),
                                Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize({}));
}

// Child Positioning Tests -----------------------------------------------------

TEST_F(FlexLayoutTest, LayoutSingleView_Horizontal) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  View* child = AddChild(kChild1Size);
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Rect(6, 5, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, LayoutSingleView_Vertical) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  View* child = AddChild(kChild1Size);
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(6, 5, 12, 10), Rect(18, 5, 13, 11),
                                Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize({}));
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Horizontal_CrossCenter) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(6, 6, 12, 10), Rect(18, 6, 13, 11),
                                Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize({}));
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Horizontal_CrossEnd) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(6, 8, 12, 10), Rect(18, 7, 13, 11),
                                Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize({}));
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
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(6, 5, 12, 13), Rect(18, 5, 13, 13),
                                Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize({}));
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Vertical_CrossStart) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(6, 5, 12, 10), Rect(6, 15, 13, 11),
                                Rect(6, 26, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(32, 46), host_->GetPreferredSize({}));
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Vertical_CrossCenter) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(8, 5, 12, 10), Rect(8, 15, 13, 11),
                                Rect(6, 26, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(32, 46), host_->GetPreferredSize({}));
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_Vertical_CrossEnd) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  AddChild(kChild1Size);
  AddChild(kChild2Size);
  AddChild(kChild3Size);
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(11, 5, 12, 10), Rect(10, 15, 13, 11),
                                Rect(6, 26, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(32, 46), host_->GetPreferredSize({}));
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
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(6, 5, 17, 10), Rect(6, 15, 17, 11),
                                Rect(6, 26, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(32, 46), host_->GetPreferredSize({}));
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
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(6, 5, 12, 10), Rect(18, 5, 13, 11),
                                Rect(31, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(57, 25), host_->GetPreferredSize({}));

  child1->SetProperty(views::kMarginsKey, Insets::TLBR(20, 21, 22, 23));
  host_->InvalidateLayout();
  test::RunScheduledLayout(host_.get());
  expected = std::vector<Rect>{Rect(27, 25, 12, 10), Rect(62, 5, 13, 11),
                               Rect(75, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(101, 64), host_->GetPreferredSize({}));

  child2->SetProperty(views::kMarginsKey, Insets(1));
  host_->InvalidateLayout();
  layout_->SetDefault(views::kMarginsKey, gfx::Insets::VH(0, 3));
  test::RunScheduledLayout(host_.get());
  expected = std::vector<Rect>{Rect(27, 25, 12, 10), Rect(63, 6, 13, 11),
                               Rect(80, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(109, 64), host_->GetPreferredSize({}));

  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->InvalidateLayout();
  test::RunScheduledLayout(host_.get());
  expected = std::vector<Rect>{Rect(27, 25, 12, 10), Rect(63, 6, 13, 11),
                               Rect(79, 7, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(107, 64), host_->GetPreferredSize({}));
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
  child1->SetProperty(views::kMarginsKey, Insets::TLBR(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->InvalidateLayout();
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(27, 25, 12, 10), Rect(7, 58, 13, 11),
                                Rect(8, 72, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(71, 94), host_->GetPreferredSize({}));
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
  child1->SetProperty(views::kMarginsKey, Insets::TLBR(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->InvalidateLayout();
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(21, 20, 12, 10), Rect(56, 5, 13, 11),
                                Rect(71, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(97, 52), host_->GetPreferredSize({}));
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
  child1->SetProperty(views::kMarginsKey, Insets::TLBR(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->InvalidateLayout();
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(21, 20, 12, 10), Rect(6, 52, 13, 11),
                                Rect(6, 65, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(56, 85), host_->GetPreferredSize({}));
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_InteriorPadding) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(10));
  View* child = AddChild(Size(13, 15));
  AddChild(kChild3Size);
  child->SetProperty(views::kInternalPaddingKey, Insets::TLBR(1, 2, 4, 8));
  host_->InvalidateLayout();
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {
      Rect(8, 9, 13, 15),
      Rect(23, 10, 17, 13),
  };
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(50, 33), host_->GetPreferredSize({}));
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_InteriorPadding_Margins) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(2));
  View* child = AddChild(Size(13, 15));
  View* child2 = AddChild(kChild3Size);
  child->SetProperty(views::kInternalPaddingKey, Insets::TLBR(1, 2, 4, 8));
  child2->SetProperty(views::kMarginsKey, Insets(5));
  host_->InvalidateLayout();
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {
      Rect(4, 4, 13, 15),
      Rect(17, 5, 17, 13),
  };
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(43, 25), host_->GetPreferredSize({}));
}

TEST_F(FlexLayoutTest, LayoutMultipleViews_InteriorPadding_Additive) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(20));
  View* child = AddChild(Size(13, 15));
  View* child2 = AddChild(kChild3Size);
  child->SetProperty(views::kInternalPaddingKey, Insets::TLBR(1, 2, 4, 8));
  child2->SetProperty(views::kInternalPaddingKey, Insets(5));
  host_->InvalidateLayout();
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {
      Rect(18, 19, 13, 15),
      Rect(38, 15, 17, 13),
  };
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_EQ(Size(70, 50), host_->GetPreferredSize({}));
}

// Height-for-width tests ------------------------------------------------------

TEST_F(FlexLayoutTest, HeightForWidth_Vertical_CrossStart) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(kMarginsKey, gfx::Insets(5));
  layout_->SetDefault(kFlexBehaviorKey, kPreferredAdjustHeight);
  AddChild({10, 10})->set_size_mode(MockView::SizeMode::kFixedArea);
  AddChild({10, 10});

  EXPECT_EQ(gfx::Size(20, 40), host_->GetPreferredSize({}));
  EXPECT_EQ(40, host_->GetHeightForWidth(26));
  EXPECT_EQ(40, host_->GetHeightForWidth(20));
  EXPECT_EQ(46, host_->GetHeightForWidth(16));

  host_->SizeToPreferredSize();
  std::vector<gfx::Rect> expected = {{5, 5, 10, 10}, {5, 25, 10, 10}};
  EXPECT_EQ(expected, GetChildBounds());
  host_->SetSize({26, 50});
  EXPECT_EQ(expected, GetChildBounds());
  host_->SetSize({20, 50});
  EXPECT_EQ(expected, GetChildBounds());
  host_->SetSize({16, 50});
  expected = {{5, 5, 6, 16}, {5, 31, 6, 10}};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest,
       HeightForWidth_Vertical_CrossStretch_WidthChangesHeight) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  layout_->SetDefault(kMarginsKey, gfx::Insets(5));
  layout_->SetDefault(kFlexBehaviorKey, kPreferredAdjustHeight);
  AddChild({10, 10})->set_size_mode(MockView::SizeMode::kFixedArea);
  AddChild({10, 10});

  EXPECT_EQ(gfx::Size(20, 40), host_->GetPreferredSize({}));
  EXPECT_EQ(40, host_->GetHeightForWidth(26));
  EXPECT_EQ(40, host_->GetHeightForWidth(20));
  EXPECT_EQ(46, host_->GetHeightForWidth(16));

  host_->SizeToPreferredSize();
  std::vector<gfx::Rect> expected = {{5, 5, 10, 10}, {5, 25, 10, 10}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({26, 50});
  expected = {{5, 5, 16, 10}, {5, 25, 16, 10}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({20, 50});
  expected = {{5, 5, 10, 10}, {5, 25, 10, 10}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({16, 50});
  expected = {{5, 5, 6, 16}, {5, 31, 6, 10}};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, HeightForWidth_Vertical_CrossStretch_FlexPreferredSize) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  layout_->SetDefault(kMarginsKey, gfx::Insets(5));
  layout_->SetDefault(kFlexBehaviorKey, kUnboundedScaleToZeroAdjustHeight);
  AddChild({10, 10})->set_size_mode(MockView::SizeMode::kFixedArea);
  AddChild({10, 10});

  EXPECT_EQ(gfx::Size(20, 40), host_->GetPreferredSize({}));
  EXPECT_EQ(40, host_->GetHeightForWidth(26));
  EXPECT_EQ(40, host_->GetHeightForWidth(20));
  EXPECT_EQ(46, host_->GetHeightForWidth(16));

  host_->SizeToPreferredSize();
  test::RunScheduledLayout(host_.get());
  std::vector<gfx::Rect> expected = {{5, 5, 10, 10}, {5, 25, 10, 10}};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, HeightForWidth_Vertical_CrossStretch_FlexLarger) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  layout_->SetDefault(kMarginsKey, gfx::Insets(5));
  layout_->SetDefault(kFlexBehaviorKey, kUnboundedScaleToZeroAdjustHeight);
  AddChild({10, 10})->set_size_mode(MockView::SizeMode::kFixedArea);
  AddChild({10, 10});

  host_->SetSize({26, 50});
  test::RunScheduledLayout(host_.get());
  std::vector<gfx::Rect> expected = {{5, 5, 16, 15}, {5, 30, 16, 15}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({20, 50});
  test::RunScheduledLayout(host_.get());
  expected = {{5, 5, 10, 15}, {5, 30, 10, 15}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({16, 50});
  test::RunScheduledLayout(host_.get());
  expected = {{5, 5, 6, 18}, {5, 33, 6, 12}};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, HeightForWidth_Vertical_CrossStretch_FlexSmaller) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  layout_->SetDefault(kMarginsKey, gfx::Insets(5));
  layout_->SetDefault(kFlexBehaviorKey, kUnboundedScaleToZeroAdjustHeight);
  AddChild({10, 10})->set_size_mode(MockView::SizeMode::kFixedArea);
  AddChild({10, 10});

  EXPECT_EQ(gfx::Size(20, 40), host_->GetPreferredSize({}));
  EXPECT_EQ(40, host_->GetHeightForWidth(26));
  EXPECT_EQ(40, host_->GetHeightForWidth(20));
  EXPECT_EQ(46, host_->GetHeightForWidth(16));

  host_->SetSize({26, 30});
  test::RunScheduledLayout(host_.get());
  std::vector<gfx::Rect> expected = {{5, 5, 16, 5}, {5, 20, 16, 5}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({20, 30});
  test::RunScheduledLayout(host_.get());
  expected = {{5, 5, 10, 5}, {5, 20, 10, 5}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({16, 30});
  test::RunScheduledLayout(host_.get());
  expected = {{5, 5, 6, 8}, {5, 23, 6, 2}};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, HeightForWidth_Horizontal_PreferredSize) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(kFlexBehaviorKey, kUnboundedScaleToZeroAdjustHeight);
  MockView* const child = AddChild({10, 10});
  child->set_size_mode(MockView::SizeMode::kFixedArea);

  // In horizontal views, the height can expand if a child is compressed
  // horizontally and uses a height-for-width calculation, but it cannot
  // contract (lest we have zero-height views in some cases).
  EXPECT_EQ(gfx::Size(10, 10), host_->GetPreferredSize({}));
  EXPECT_EQ(10, host_->GetHeightForWidth(10));
  EXPECT_EQ(10, host_->GetHeightForWidth(20));
  EXPECT_EQ(20, host_->GetHeightForWidth(5));
}

// Host insets tests -----------------------------------------------------------

TEST_F(FlexLayoutTest, Layout_HostInsets_Horizontal) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  View* child = AddChild(kChild1Size);
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Rect(6, 5, 12, 10), child->bounds());
}

TEST_F(FlexLayoutTest, Layout_HostInsets_Vertical) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  host_->SetBorder(CreateEmptyBorder(kLayoutInsets));
  View* child = AddChild(kChild1Size);
  test::RunScheduledLayout(host_.get());
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
  host_->SetBorder(views::CreateEmptyBorder(2));
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

  const Size expected_preferred_size = host_->GetPreferredSize({});
  host_->SetSize(expected_preferred_size);
  const std::vector<Rect> expected_bounds = GetChildBounds();

  layout_->SetIncludeHostInsetsInLayout(true);
  const Size preferred_size = host_->GetPreferredSize({});
  EXPECT_EQ(expected_preferred_size, preferred_size);
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(expected_bounds, GetChildBounds());
}

TEST_F(FlexLayoutTest, SetIncludeHostInsetsInLayout_CollapseIntoInsets) {
  host_->SetBorder(views::CreateEmptyBorder(Insets(2)));
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
  const Size preferred_size = host_->GetPreferredSize({});
  EXPECT_EQ(Size(40, 76), preferred_size);
  host_->SetSize(preferred_size);
  const std::vector<Rect> expected = {Rect(19, 7, 10, 10), Rect(15, 32, 10, 10),
                                      Rect(19, 57, 10, 10)};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, SetIncludeHostInsetsInLayout_OverlapInsets) {
  host_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(4, 5, 5, 5)));
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(kLayoutInsets);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* const child = AddChild(Size(10, 10));
  child->SetProperty(views::kInternalPaddingKey, Insets(10));

  layout_->SetIncludeHostInsetsInLayout(true);
  const Size preferred_size = host_->GetPreferredSize({});
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

  Size preferred_size = host_->GetPreferredSize({});
  EXPECT_EQ(Size(33, 66), preferred_size);

  layout_->SetIgnoreDefaultMainAxisMargins(true);
  preferred_size = host_->GetPreferredSize({});
  EXPECT_EQ(Size(33, 58), preferred_size);

  host_->SetSize(preferred_size);
  const std::vector<Rect> expected = {Rect(10, 5, 10, 10), Rect(10, 23, 10, 10),
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

  Size preferred_size = host_->GetPreferredSize({});
  EXPECT_EQ(Size(34, 76), preferred_size);

  layout_->SetIgnoreDefaultMainAxisMargins(true);
  preferred_size = host_->GetPreferredSize({});
  EXPECT_EQ(Size(34, 76), preferred_size);

  host_->SetSize(preferred_size);
  const std::vector<Rect> expected = {Rect(11, 11, 10, 10), Rect(6, 32, 10, 10),
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
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->SetSize(Size(200, 200));
  test::RunScheduledLayout(host_.get());
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
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->SetSize(Size(200, 200));
  test::RunScheduledLayout(host_.get());
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
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->SetSize(Size(200, 200));
  test::RunScheduledLayout(host_.get());
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
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->SetSize(Size(200, 200));
  test::RunScheduledLayout(host_.get());
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
  child1->SetProperty(views::kMarginsKey, Insets::TLBR(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->SetSize(Size(105, 50));
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(21, 20, 12, 10), Rect(56, 5, 13, 11),
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
  child1->SetProperty(views::kMarginsKey, Insets::TLBR(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->SetSize(Size(105, 50));
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(25, 20, 12, 10), Rect(60, 5, 13, 11),
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
  child1->SetProperty(views::kMarginsKey, Insets::TLBR(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->SetSize(Size(105, 50));
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(29, 20, 12, 10), Rect(64, 5, 13, 11),
                                Rect(79, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_AddDroppedMargins) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(false);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  View* child1 = AddChild(Size(10, 10));
  View* child2 = AddChild(Size(10, 10));
  View* child3 = AddChild(Size(10, 10));
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);
  EXPECT_EQ(Size(30, 20), host_->GetMinimumSize());

  host_->SetSize(Size(100, 50));
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(5, 5, 10, 10), Rect(16, 6, 10, 10),
                                Rect(27, 5, 10, 10)};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize(Size(25, 50));
  test::RunScheduledLayout(host_.get());
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
  child1->SetProperty(views::kMarginsKey, Insets::TLBR(20, 21, 22, 23));
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->SetSize(Size(1000, 100));
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(21, 27, 12, 10), Rect(6, 59, 13, 11),
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
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  host_->SetSize(Size(55, 50));
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected = {Rect(11, 10, 12, 10), Rect(36, 5, 13, 11),
                                Rect(51, 5, 17, 13)};
  EXPECT_EQ(expected, GetChildBounds());

  child1->SetProperty(views::kFlexBehaviorKey, kDropOut);
  host_->InvalidateLayout();
  EXPECT_EQ(Size(77, 32), host_->GetPreferredSize({}));
  EXPECT_EQ(Size(47, 25), host_->GetMinimumSize());
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());
  EXPECT_EQ(Rect(6, 5, 13, 11), child2->bounds());
  EXPECT_EQ(Rect(21, 5, 17, 13), child3->bounds());

  child1->ClearProperty(views::kFlexBehaviorKey);
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);
  host_->InvalidateLayout();
  EXPECT_EQ(Size(77, 32), host_->GetPreferredSize({}));
  EXPECT_EQ(Size(62, 32), host_->GetMinimumSize());
  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());
  EXPECT_EQ(Rect(11, 10, 12, 10), child1->bounds());
  EXPECT_EQ(Rect(36, 5, 17, 13), child3->bounds());

  child2->ClearProperty(views::kFlexBehaviorKey);
  child3->SetProperty(views::kFlexBehaviorKey, kDropOut);
  host_->InvalidateLayout();
  EXPECT_EQ(Size(77, 32), host_->GetPreferredSize({}));
  EXPECT_EQ(Size(58, 32), host_->GetMinimumSize());
  test::RunScheduledLayout(host_.get());
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
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  // Set flex separately; we'll test default flex later.
  child1->SetProperty(views::kFlexBehaviorKey, kDropOut);
  child2->SetProperty(views::kFlexBehaviorKey, kDropOut);
  child3->SetProperty(views::kFlexBehaviorKey, kDropOut);
  EXPECT_EQ(Size(9, 7), host_->GetMinimumSize());

  host_->SetSize(Size(100, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());

  host_->SetSize(Size(58, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  host_->SetSize(Size(57, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  // Since there's no room for child1, child2 becomes visible.
  host_->SetSize(Size(28, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  host_->SetSize(Size(27, 50));
  test::RunScheduledLayout(host_.get());
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
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  layout_->SetDefault(views::kFlexBehaviorKey, kDropOut);
  EXPECT_EQ(Size(9, 7), host_->GetMinimumSize());

  host_->SetSize(Size(100, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());

  host_->SetSize(Size(58, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  host_->SetSize(Size(57, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  // Since there's no room for child1, child2 becomes visible.
  host_->SetSize(Size(28, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_FALSE(child3->GetVisible());

  host_->SetSize(Size(27, 50));
  test::RunScheduledLayout(host_.get());
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
  child2->SetProperty(views::kMarginsKey, Insets(1));
  child3->SetProperty(views::kMarginsKey, Insets(2));
  layout_->SetDefault(views::kFlexBehaviorKey, kDropOut);
  child3->SetProperty(views::kFlexBehaviorKey, kDropOutHighPriority);
  EXPECT_EQ(Size(9, 7), host_->GetMinimumSize());

  host_->SetSize(Size(100, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());

  host_->SetSize(Size(65, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());

  host_->SetSize(Size(40, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_FALSE(child2->GetVisible());
  EXPECT_TRUE(child3->GetVisible());

  host_->SetSize(Size(20, 50));
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(10, 20), child1->size());
  EXPECT_EQ(Size(10, 10), child2->size());

  host_->SetSize(Size(20, 35));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(10, 10), child1->size());
  EXPECT_EQ(Size(10, 10), child2->size());

  host_->SetSize(Size(20, 30));
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(10, 9), child1->size());
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
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(15, 10), child1->size());
  EXPECT_EQ(Size(15, 10), child2->size());

  host_->SetSize(Size(60, 20));
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(15, 10), child1->size());
  EXPECT_EQ(Size(15, 10), child2->size());

  host_->SetSize(Size(60, 20));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(20, 10), child1->size());
  EXPECT_EQ(Size(20, 10), child2->size());
}

TEST_F(FlexLayoutTest,
       Layout_Flex_TwoChildViews_UnequalWeight_FirstHigher_FlexSmaller) {
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

  // Deficit of 5 is allocated 2:1, but rounding gives us -3, -2.
  host_->SetSize(Size(50, 20));
  std::vector<gfx::Rect> expected = {{5, 5, 17, 10}, {27, 5, 18, 10}};
  EXPECT_EQ(expected, GetChildBounds());

  // Deficit of 6 divides evenly, gives us -4, -2.
  host_->SetSize(Size(49, 20));
  expected = {{5, 5, 16, 10}, {26, 5, 18, 10}};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest,
       Layout_Flex_TwoChildViews_UnequalWeight_SecondHigher_FlexSmaller) {
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

  // Deficit of 5 is allocated 1:2, but rounding gives us -2, -3.
  host_->SetSize(Size(50, 20));
  std::vector<gfx::Rect> expected = {{5, 5, 18, 10}, {28, 5, 17, 10}};
  EXPECT_EQ(expected, GetChildBounds());

  // Deficit of 6 divides evenly, gives us -2, -4.
  host_->SetSize(Size(49, 20));
  expected = {{5, 5, 18, 10}, {28, 5, 16, 10}};
  EXPECT_EQ(expected, GetChildBounds());
}

// This is a test for the case where one child's flex rule will cause it to
// scale to its minimum size, resulting in the other view getting more space
// than it otherwise would.
TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_UnequalWeight_OneHitsMinimum) {
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

  // Deficit of 20 divides up as -7 and -13.
  host_->SetSize(Size(35, 20));
  std::vector<gfx::Rect> expected = {{5, 5, 13, 10}, {23, 5, 7, 10}};
  EXPECT_EQ(expected, GetChildBounds());

  // Deficit of 25 divides up as -8 and -17, but second view can only shrink by
  // 15, so first view has to shrink by 10 instead.
  host_->SetSize(Size(30, 20));
  expected = {{5, 5, 10, 10}, {20, 5, 5, 10}};
  EXPECT_EQ(expected, GetChildBounds());
}

// This is a test for the case where one child's flex rule will cause it to
// drop out, resulting in the other view getting more space *than its preferred
// size*.
TEST_F(
    FlexLayoutTest,
    Layout_Flex_TwoChildViews_UnequalWeight_OneDropsOut_OtherExceedsPreferred) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(20, 10), Size(15, 10));
  View* child2 = AddChild(Size(20, 10), Size(15, 10));
  child1->SetProperty(views::kFlexBehaviorKey,
                      kUnboundedScaleToMinimumSnapToZero);
  child2->SetProperty(views::kFlexBehaviorKey,
                      kUnboundedScaleToMinimumSnapToZero.WithWeight(4));

  host_->SetSize(Size(45, 20));
  std::vector<gfx::Rect> expected = {{5, 5, 35, 10}, {}};
  EXPECT_EQ(expected, GetChildBounds());
}

// This is a regression test for a case where a view marked as having flex
// weight but which could not flex larger than its preferred size would cause
// other views at that weight to not receive available flex space.
TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_FirstViewFillsAvailableSpace) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(20, 10));
  View* child2 = AddChild(Size(20, 10));
  child1->SetProperty(views::kFlexBehaviorKey, kUnbounded);
  child2->SetProperty(views::kFlexBehaviorKey,
                      FlexSpecification(MinimumFlexSizeRule::kPreferred,
                                        MaximumFlexSizeRule::kPreferred));

  host_->SetSize(Size(70, 20));
  const std::vector<Rect> expected_bounds = {{5, 5, 35, 10}, {45, 5, 20, 10}};
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
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(15, 10), child1->size());
  EXPECT_EQ(Size(20, 10), child2->size());

  host_->SetSize(Size(35, 20));
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
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
  EXPECT_EQ(Size(25, 15), child->size());

  host_->SetSize(Size(30, 25));
  EXPECT_EQ(Size(20, 15), child->size());

  host_->SetSize(Size(29, 25));
  EXPECT_EQ(Size(5, 15), child->size());

  host_->SetSize(Size(25, 10));
  EXPECT_EQ(Size(5, 5), child->size());

  // This is actually less space than the child needs, but its flex rule does
  // not allow it to drop out.
  host_->SetSize(Size(10, 10));
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
  EXPECT_EQ(Size(25, 15), child->size());

  host_->SetSize(Size(30, 25));
  EXPECT_EQ(Size(20, 15), child->size());

  host_->SetSize(Size(29, 25));
  EXPECT_EQ(Size(19, 15), child->size());

  host_->SetSize(Size(25, 16));
  EXPECT_EQ(Size(15, 6), child->size());

  // This is too short to display the view, however it has horizontal size, so
  // the view does not drop out.
  host_->SetSize(Size(25, 10));
  EXPECT_TRUE(child->GetVisible());
  EXPECT_EQ(Size(15, 0), child->size());

  host_->SetSize(Size(15, 15));
  EXPECT_EQ(Size(5, 5), child->size());

  host_->SetSize(Size(14, 15));
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
  EXPECT_EQ(Size(25, 15), child->size());

  host_->SetSize(Size(30, 25));
  EXPECT_EQ(Size(20, 15), child->size());

  host_->SetSize(Size(29, 25));
  EXPECT_EQ(Size(19, 15), child->size());

  host_->SetSize(Size(25, 16));
  EXPECT_EQ(Size(15, 6), child->size());

  // This is too short to display the view, however it has horizontal size, so
  // the view does not drop out.
  host_->SetSize(Size(25, 10));
  EXPECT_TRUE(child->GetVisible());
  EXPECT_EQ(Size(15, 0), child->size());

  host_->SetSize(Size(15, 15));
  EXPECT_EQ(Size(5, 5), child->size());

  host_->SetSize(Size(14, 14));
  EXPECT_EQ(Size(4, 4), child->size());

  host_->SetSize(Size(9, 14));
  EXPECT_FALSE(child->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_FlexRule_UnboundedSnapToMinimum1D) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child = AddChild(Size(20, 10), Size(5, 5));
  child->SetProperty(views::kFlexBehaviorKey,
                     kUnboundedSnapToMinimumHorizontal);

  host_->SetSize(Size(35, 25));
  EXPECT_EQ(Size(25, 10), child->size());

  host_->SetSize(Size(30, 25));
  EXPECT_EQ(Size(20, 10), child->size());

  host_->SetSize(Size(29, 25));
  EXPECT_EQ(Size(5, 10), child->size());

  host_->SetSize(Size(25, 10));
  EXPECT_EQ(Size(5, 10), child->size());

  // This is actually less space than the child needs, but its flex rule does
  // not allow it to drop out.
  host_->SetSize(Size(10, 10));
  EXPECT_EQ(Size(5, 10), child->size());
}

TEST_F(FlexLayoutTest, Layout_FlexRule_UnboundedScaleToMinimumSnapToZero1D) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child = AddChild(Size(20, 10), Size(5, 5));
  child->SetProperty(views::kFlexBehaviorKey,
                     kUnboundedScaleToMinimumSnapToZeroHorizontal);

  host_->SetSize(Size(35, 25));
  EXPECT_EQ(Size(25, 10), child->size());

  host_->SetSize(Size(30, 25));
  EXPECT_EQ(Size(20, 10), child->size());

  host_->SetSize(Size(29, 25));
  EXPECT_EQ(Size(19, 10), child->size());

  host_->SetSize(Size(25, 16));
  EXPECT_EQ(Size(15, 10), child->size());

  host_->SetSize(Size(25, 10));
  EXPECT_TRUE(child->GetVisible());
  EXPECT_EQ(Size(15, 10), child->size());

  host_->SetSize(Size(15, 15));
  EXPECT_EQ(Size(5, 10), child->size());

  host_->SetSize(Size(14, 15));
  EXPECT_FALSE(child->GetVisible());
}

TEST_F(FlexLayoutTest, Layout_FlexRule_UnboundedScaleToZero1D) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  // Because we are using a flex rule that scales all the way to zero, ensure
  // that the child view's minimum size is *not* respected.
  View* child = AddChild(Size(20, 10), Size(5, 5));
  child->SetProperty(views::kFlexBehaviorKey, kUnboundedScaleToZeroHorizontal);

  host_->SetSize(Size(35, 25));
  EXPECT_EQ(Size(25, 10), child->size());

  host_->SetSize(Size(30, 25));
  EXPECT_EQ(Size(20, 10), child->size());

  host_->SetSize(Size(29, 25));
  EXPECT_EQ(Size(19, 10), child->size());

  host_->SetSize(Size(25, 16));
  EXPECT_EQ(Size(15, 10), child->size());

  host_->SetSize(Size(25, 10));
  EXPECT_TRUE(child->GetVisible());
  EXPECT_EQ(Size(15, 10), child->size());

  host_->SetSize(Size(15, 15));
  EXPECT_EQ(Size(5, 10), child->size());

  host_->SetSize(Size(14, 14));
  EXPECT_EQ(Size(4, 10), child->size());

  host_->SetSize(Size(9, 14));
  EXPECT_FALSE(child->GetVisible());
}

// Tests that views allowed to scale up to their maximum size will do so.
TEST_F(FlexLayoutTest, Layout_FlexRule_ScaleToMaximum) {
  auto* const child1 = AddChild(Size(10, 10));
  child1->SetMaximumSize(Size(20, 20));
  child1->SetProperty(kFlexBehaviorKey, kScaleToMaximum);
  auto* const child2 = AddChild(Size(10, 10));
  child2->SetMaximumSize(Size(20, 20));
  child2->SetProperty(kFlexBehaviorKey, kScaleToMaximum);
  auto* const child3 = AddChild(Size(10, 10));
  child3->SetMaximumSize(Size(20, 20));
  child3->SetProperty(kFlexBehaviorKey, kScaleToMaximum);

  host_->SetSize(Size(20, 10));
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected_bounds = {
      {0, 0, 10, 10}, {10, 0, 10, 10}, {20, 0, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());

  host_->SetSize(Size(30, 10));
  test::RunScheduledLayout(host_.get());
  expected_bounds = {{0, 0, 10, 10}, {10, 0, 10, 10}, {20, 0, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());

  host_->SetSize(Size(33, 10));
  test::RunScheduledLayout(host_.get());
  expected_bounds = {{0, 0, 11, 10}, {11, 0, 11, 10}, {22, 0, 11, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());

  host_->SetSize(Size(35, 10));
  test::RunScheduledLayout(host_.get());
  expected_bounds = {{0, 0, 12, 10}, {12, 0, 12, 10}, {24, 0, 11, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());

  host_->SetSize(Size(60, 10));
  test::RunScheduledLayout(host_.get());
  expected_bounds = {{0, 0, 20, 10}, {20, 0, 20, 10}, {40, 0, 20, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());

  host_->SetSize(Size(70, 10));
  test::RunScheduledLayout(host_.get());
  expected_bounds = {{0, 0, 20, 10}, {20, 0, 20, 10}, {40, 0, 20, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());
}

// Tests that views allowed to scale up to their maximum size will do so.
TEST_F(FlexLayoutTest, Layout_FlexRule_ScaleToMaximum_WithOrder) {
  auto* const child1 = AddChild(Size(10, 10));
  child1->SetMaximumSize(Size(20, 20));
  child1->SetProperty(kFlexBehaviorKey, kScaleToMaximum.WithOrder(1));
  auto* const child2 = AddChild(Size(10, 10));
  child2->SetMaximumSize(Size(20, 20));
  child2->SetProperty(kFlexBehaviorKey, kScaleToMaximum.WithOrder(2));
  auto* const child3 = AddChild(Size(10, 10));
  child3->SetMaximumSize(Size(20, 20));
  child3->SetProperty(kFlexBehaviorKey, kScaleToMaximum.WithOrder(3));

  host_->SetSize(Size(20, 10));
  test::RunScheduledLayout(host_.get());
  std::vector<Rect> expected_bounds = {
      {0, 0, 10, 10}, {10, 0, 10, 10}, {20, 0, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());

  host_->SetSize(Size(30, 10));
  test::RunScheduledLayout(host_.get());
  expected_bounds = {{0, 0, 10, 10}, {10, 0, 10, 10}, {20, 0, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());

  host_->SetSize(Size(33, 10));
  test::RunScheduledLayout(host_.get());
  expected_bounds = {{0, 0, 13, 10}, {13, 0, 10, 10}, {23, 0, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());

  host_->SetSize(Size(43, 10));
  test::RunScheduledLayout(host_.get());
  expected_bounds = {{0, 0, 20, 10}, {20, 0, 13, 10}, {33, 0, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());

  host_->SetSize(Size(53, 10));
  test::RunScheduledLayout(host_.get());
  expected_bounds = {{0, 0, 20, 10}, {20, 0, 20, 10}, {40, 0, 13, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());

  host_->SetSize(Size(70, 10));
  test::RunScheduledLayout(host_.get());
  expected_bounds = {{0, 0, 20, 10}, {20, 0, 20, 10}, {40, 0, 20, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());
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
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(kSmallSize, child1->size());
  EXPECT_FALSE(child2->GetVisible());

  // When the first view has less room than its preferred size, it should still
  // take up all of the space.
  constexpr Size kIntermediateSize(8, 7);
  host_->SetSize(kIntermediateSize);
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(kIntermediateSize, child1->size());
  EXPECT_FALSE(child2->GetVisible());

  // When the first view has more room than its preferred size, but not enough
  // to make room for the second view, the second view still drops out.
  constexpr Size kLargerSize(13, 8);
  host_->SetSize(kLargerSize);
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
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
  EXPECT_EQ(Size(kLargeSize.width() + kExtra, kLargeSize.height() + kExtra),
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
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(10, 10));
  AddChild(Size(10, 10));
  child1->SetProperty(views::kFlexBehaviorKey,
                      kUnbounded.WithAlignment(LayoutAlignment::kStart));

  host_->SetSize(Size(50, 20));
  const std::vector<Rect> expected_bounds = {{5, 5, 10, 10}, {35, 5, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_FlexAlignment_End) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(10, 10));
  AddChild(Size(10, 10));
  child1->SetProperty(views::kFlexBehaviorKey,
                      kUnbounded.WithAlignment(LayoutAlignment::kEnd));

  host_->SetSize(Size(50, 20));
  const std::vector<Rect> expected_bounds = {{20, 5, 10, 10}, {35, 5, 10, 10}};
  EXPECT_EQ(expected_bounds, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_Flex_TwoChildViews_FlexAlignment_Center) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetInteriorMargin(Insets(5));
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  View* child1 = AddChild(Size(10, 10));
  AddChild(Size(10, 10));
  child1->SetProperty(views::kFlexBehaviorKey,
                      kUnbounded.WithAlignment(LayoutAlignment::kCenter));

  host_->SetSize(Size(50, 20));
  const std::vector<Rect> expected_bounds = {{12, 5, 10, 10}, {35, 5, 10, 10}};
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
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kFullSize, kFullSize), child->size());

  host_->SetSize(Size(100, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kFullSize, kHalfSize), child->size());

  host_->SetSize(Size(50, 100));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kHalfSize, kFullSize), child->size());

  host_->SetSize(Size(45, 40));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kHalfSize, kHalfSize), child->size());

  // Custom flex rule does not go below half size.
  host_->SetSize(Size(20, 20));
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kFullSize, kFullSize), child->size());

  host_->SetSize(Size(100, 65));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kFullSize, kHalfSize), child->size());

  host_->SetSize(Size(50, 100));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kHalfSize, kFullSize), child->size());

  host_->SetSize(Size(45, 40));
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kFullSize, kFullSize), child->size());

  host_->SetSize(Size(100, 50));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kFullSize, kHalfSize), child->size());

  host_->SetSize(Size(50, 100));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kHalfSize, kFullSize), child->size());

  host_->SetSize(Size(45, 40));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(Size(kHalfSize, kHalfSize), child->size());

  host_->SetSize(Size(20, 20));
  test::RunScheduledLayout(host_.get());
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
  test::RunScheduledLayout(host_.get());
  EXPECT_TRUE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(0, child1->GetSetVisibleCount());
  EXPECT_EQ(0, child2->GetSetVisibleCount());

  host_->SetSize(Size(35, 20));
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(1, child1->GetSetVisibleCount());
  EXPECT_EQ(0, child2->GetSetVisibleCount());

  child1->ResetCounts();
  child2->ResetCounts();
  host_->SetSize(Size(30, 20));
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(0, child1->GetSetVisibleCount());
  EXPECT_EQ(0, child2->GetSetVisibleCount());

  child1->SetVisible(false);
  child1->ResetCounts();

  host_->SetSize(Size(40, 20));
  test::RunScheduledLayout(host_.get());
  EXPECT_FALSE(child1->GetVisible());
  EXPECT_TRUE(child2->GetVisible());
  EXPECT_EQ(0, child1->GetSetVisibleCount());
  EXPECT_EQ(0, child2->GetSetVisibleCount());
}

TEST_F(FlexLayoutTest, Layout_Vertical_ZeroWidthNonZeroHeight) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  layout_->SetDefault(kMarginsKey, gfx::Insets(5));
  layout_->SetCollapseMargins(true);
  AddChild(gfx::Size(10, 10));
  AddChild(gfx::Size(0, 10));
  AddChild(gfx::Size(10, 10));

  host_->SetSize(gfx::Size(20, 50));
  const std::vector<gfx::Rect> expected = {
      {5, 5, 10, 10}, {10, 20, 0, 10}, {5, 35, 10, 10}};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, Layout_Vertical_ZeroWidthZeroHeight) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  layout_->SetDefault(kMarginsKey, gfx::Insets(5));
  layout_->SetCollapseMargins(true);
  AddChild(gfx::Size(10, 10));
  AddChild(gfx::Size(0, 0));
  AddChild(gfx::Size(10, 10));

  host_->SetSize(gfx::Size(20, 40));
  const std::vector<gfx::Rect> expected = {{5, 5, 10, 10}, {}, {5, 20, 10, 10}};
  EXPECT_EQ(expected, GetChildBounds());
}

// Available Size Tests -------------------------------------------------------

TEST_F(FlexLayoutTest, GetAvailableSize_NoFlex) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  MockView* const child1 = AddChild(Size(20, 10));
  MockView* const child2 = AddChild(Size(10, 5));

  // With no flex at preferred size views will get their preferred main axis
  // size.
  host_->SizeToPreferredSize();
  EXPECT_EQ(SizeBounds(20, 10), host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(10, 10), host_->GetAvailableSize(child2));
}

TEST_F(FlexLayoutTest, GetAvailableSize_NoFlex_Margins) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  MockView* const child1 = AddChild(Size(20, 10));
  child1->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(3, 5, 7, 5));
  MockView* const child2 = AddChild(Size(10, 5));
  child2->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(9, 5, 5, 5));

  host_->SizeToPreferredSize();
  EXPECT_EQ(SizeBounds(20, 10), host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(10, 6), host_->GetAvailableSize(child2));
}

TEST_F(FlexLayoutTest, GetAvailableSize_NoFlex_ExtraSize) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  MockView* const child1 = AddChild(Size(20, 10));
  MockView* const child2 = AddChild(Size(10, 5));

  host_->SetSize({50, 25});
  const int excess = host_->width() - host_->GetPreferredSize({}).width();
  EXPECT_EQ(SizeBounds(child1->GetPreferredSize({}).width() + excess, 15),
            host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(child2->GetPreferredSize({}).width() + excess, 15),
            host_->GetAvailableSize(child2));
}

TEST_F(FlexLayoutTest, GetAvailableSize_NoFlex_Vertical) {
  layout_->SetOrientation(LayoutOrientation::kVertical);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  MockView* const child1 = AddChild(Size(20, 10));
  MockView* const child2 = AddChild(Size(10, 5));

  host_->SetSize({35, 35});
  const int excess = host_->height() - host_->GetPreferredSize({}).height();
  EXPECT_EQ(SizeBounds(25, child1->GetPreferredSize({}).height() + excess),
            host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(25, child2->GetPreferredSize({}).height() + excess),
            host_->GetAvailableSize(child2));
}

TEST_F(FlexLayoutTest, GetAvailableSize_Flex_AllSameSize) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  MockView* const child1 = AddChild(Size(20, 10));
  MockView* const child2 = AddChild(Size(10, 5));
  MockView* const child3 = AddChild(Size(5, 5));
  child3->SetProperty(kFlexBehaviorKey, kUnboundedScaleToZero);
  MockView* const child4 = AddChild(Size(5, 5));
  child4->SetProperty(kFlexBehaviorKey, kUnboundedScaleToZero);

  host_->SizeToPreferredSize();
  // Each of these views can expand into both the view space and the margins for
  // the third and fourth views (total 10 each).
  EXPECT_EQ(SizeBounds(40, 10), host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(30, 10), host_->GetAvailableSize(child2));
  // Each of these can expand into the view space and margin of the other.
  EXPECT_EQ(SizeBounds(15, 10), host_->GetAvailableSize(child3));
  EXPECT_EQ(SizeBounds(15, 10), host_->GetAvailableSize(child4));

  // At minimum size there should be no excess available.
  host_->SetSize(host_->GetMinimumSize());
  EXPECT_EQ(SizeBounds(20, 10), host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(10, 10), host_->GetAvailableSize(child2));
  EXPECT_EQ(SizeBounds(0, 10), host_->GetAvailableSize(child3));
  EXPECT_EQ(SizeBounds(0, 10), host_->GetAvailableSize(child4));
}

TEST_F(FlexLayoutTest, GetAvailableSize_Flex_VariedMinimumSizes) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  MockView* const child1 = AddChild(Size(20, 10));
  MockView* const child2 = AddChild(Size(10, 5));
  MockView* const child3 = AddChild(Size(10, 10), Size(7, 7));
  child3->SetProperty(kFlexBehaviorKey, kUnboundedScaleToMinimum);
  MockView* const child4 = AddChild(Size(12, 12), Size(5, 5));
  child4->SetProperty(kFlexBehaviorKey, kUnboundedScaleToMinimum);

  host_->SizeToPreferredSize();
  // Since the third and fourth views can only shrink a certain amount, the
  // excess available to the first two views is smaller than in previous tests.
  EXPECT_EQ(SizeBounds(30, 12), host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(20, 12), host_->GetAvailableSize(child2));
  // Each of these can only consume the difference between the other's minimum
  // and preferred sizes (as per the flex rule).
  EXPECT_EQ(SizeBounds(17, 12), host_->GetAvailableSize(child3));
  EXPECT_EQ(SizeBounds(15, 12), host_->GetAvailableSize(child4));

  // At minimum size there should be no excess available.
  host_->SetSize(host_->GetMinimumSize());
  EXPECT_EQ(SizeBounds(20, 10), host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(10, 10), host_->GetAvailableSize(child2));
  EXPECT_EQ(SizeBounds(7, 10), host_->GetAvailableSize(child3));
  EXPECT_EQ(SizeBounds(5, 10), host_->GetAvailableSize(child4));
}

TEST_F(FlexLayoutTest, GetAvailableSize_Flex_HiddenViews) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  MockView* const child1 = AddChild(Size(20, 10));
  MockView* const child2 = AddChild(Size(10, 5));
  MockView* const child3 = AddChild(Size(5, 5));
  child3->SetProperty(kFlexBehaviorKey, kDropOut);
  MockView* const child4 = AddChild(Size(5, 5));
  child4->SetProperty(kFlexBehaviorKey, kDropOut);

  // Make the second child invisible. This should exclude it from the layout.
  child2->SetVisible(false);

  host_->SizeToPreferredSize();
  // This view can expand into both the view space and the margins for the third
  // and fourth views (total 10 each).
  EXPECT_EQ(SizeBounds(40, 10), host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(), host_->GetAvailableSize(child2));
  // These views can expand into each others' space.
  EXPECT_EQ(SizeBounds(15, 10), host_->GetAvailableSize(child3));
  EXPECT_EQ(SizeBounds(15, 10), host_->GetAvailableSize(child4));

  // This should cause one of the third or fourth children to drop out, but both
  // will still have some space available.
  host_->SetSize({40, 20});
  EXPECT_EQ(SizeBounds(30, 10), host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(), host_->GetAvailableSize(child2));
  EXPECT_EQ(SizeBounds(5, 10), host_->GetAvailableSize(child3));
  EXPECT_EQ(SizeBounds(5, 10), host_->GetAvailableSize(child4));

  // At minimum size, there is no space for the third or fourth view.
  host_->SetSize(host_->GetMinimumSize());
  EXPECT_EQ(SizeBounds(20, 10), host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(), host_->GetAvailableSize(child2));
  EXPECT_EQ(SizeBounds(0, 10), host_->GetAvailableSize(child3));
  EXPECT_EQ(SizeBounds(0, 10), host_->GetAvailableSize(child4));
}

TEST_F(FlexLayoutTest, GetAvailableSize_Flex_DifferentWeights) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCollapseMargins(true);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  layout_->SetDefault(views::kMarginsKey, gfx::Insets(5));
  MockView* const child1 = AddChild(Size(20, 10));
  MockView* const child2 = AddChild(Size(10, 5));
  MockView* const child3 = AddChild(Size(12, 12), Size(7, 7));
  child3->SetProperty(kFlexBehaviorKey, kFlex1ScaleToMinimumHighPriority);
  MockView* const child4 = AddChild(Size(8, 8), Size(4, 4));
  child4->SetProperty(kFlexBehaviorKey, kFlex1ScaleToMinimum);

  host_->SetSize({80, 25});
  const int excess = host_->width() - host_->GetPreferredSize({}).width();
  const int child3_excess =
      child3->GetPreferredSize({}).width() - child3->GetMinimumSize().width();
  const int child4_excess =
      child4->GetPreferredSize({}).width() - child4->GetMinimumSize().width();
  // The first two views can take all of the excess plus the difference between
  // minimum and preferred size for each of the third and fourth views.
  EXPECT_EQ(SizeBounds(child1->GetPreferredSize({}).width() + excess +
                           child3_excess + child4_excess,
                       15),
            host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(child2->GetPreferredSize({}).width() + excess +
                           child3_excess + child4_excess,
                       15),
            host_->GetAvailableSize(child2));
  // The third view has a higher priority, so it can take the excess plus the
  // excess from the fourth view.
  EXPECT_EQ(
      SizeBounds(child3->GetPreferredSize({}).width() + excess + child4_excess,
                 15),
      host_->GetAvailableSize(child3));
  // This view has the lowest priority so it can only take the excess space in
  // the layout.
  EXPECT_EQ(SizeBounds(child4->GetPreferredSize({}).width() + excess, 15),
            host_->GetAvailableSize(child4));

  // Same as above, but there is no overall excess.
  host_->SizeToPreferredSize();
  EXPECT_EQ(SizeBounds(child1->GetPreferredSize({}).width() + child3_excess +
                           child4_excess,
                       12),
            host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(child2->GetPreferredSize({}).width() + child3_excess +
                           child4_excess,
                       12),
            host_->GetAvailableSize(child2));
  EXPECT_EQ(
      SizeBounds(child3->GetPreferredSize({}).width() + child4_excess, 12),
      host_->GetAvailableSize(child3));
  EXPECT_EQ(SizeBounds(child4->GetPreferredSize({}).width(), 12),
            host_->GetAvailableSize(child4));

  // At minimum size there is no excess; all views get their minimum size.
  host_->SetSize(host_->GetMinimumSize());
  EXPECT_EQ(SizeBounds(child1->GetPreferredSize({}).width(), 10),
            host_->GetAvailableSize(child1));
  EXPECT_EQ(SizeBounds(child2->GetPreferredSize({}).width(), 10),
            host_->GetAvailableSize(child2));
  EXPECT_EQ(SizeBounds(child3->GetMinimumSize().width(), 10),
            host_->GetAvailableSize(child3));
  EXPECT_EQ(SizeBounds(child4->GetMinimumSize().width(), 10),
            host_->GetAvailableSize(child4));
}

// Flex Allocation Order -------------------------------------------------------

TEST_F(FlexLayoutTest, FlexAllocationOrderNormal) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetDefault(kFlexBehaviorKey, kDropOut.WithWeight(0));
  layout_->SetFlexAllocationOrder(FlexAllocationOrder::kNormal);
  View* const v1 = AddChild({10, 10});
  View* const v2 = AddChild({10, 10});
  View* const v3 = AddChild({10, 10});

  host_->SetSize({35, 10});
  EXPECT_TRUE(v1->GetVisible());
  EXPECT_TRUE(v2->GetVisible());
  EXPECT_TRUE(v3->GetVisible());

  host_->SetSize({25, 10});
  EXPECT_TRUE(v1->GetVisible());
  EXPECT_TRUE(v2->GetVisible());
  EXPECT_FALSE(v3->GetVisible());

  host_->SetSize({15, 10});
  EXPECT_TRUE(v1->GetVisible());
  EXPECT_FALSE(v2->GetVisible());
  EXPECT_FALSE(v3->GetVisible());
}

TEST_F(FlexLayoutTest, FlexAllocationOrderReverse) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetDefault(kFlexBehaviorKey, kDropOut.WithWeight(0));
  layout_->SetFlexAllocationOrder(FlexAllocationOrder::kReverse);
  View* const v1 = AddChild({10, 10});
  View* const v2 = AddChild({10, 10});
  View* const v3 = AddChild({10, 10});

  host_->SetSize({35, 10});
  EXPECT_TRUE(v1->GetVisible());
  EXPECT_TRUE(v2->GetVisible());
  EXPECT_TRUE(v3->GetVisible());

  host_->SetSize({25, 10});
  EXPECT_FALSE(v1->GetVisible());
  EXPECT_TRUE(v2->GetVisible());
  EXPECT_TRUE(v3->GetVisible());

  host_->SetSize({15, 10});
  EXPECT_FALSE(v1->GetVisible());
  EXPECT_FALSE(v2->GetVisible());
  EXPECT_TRUE(v3->GetVisible());
}

TEST_F(FlexLayoutTest, FlexAllocationOrderNormalWithExcess) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetDefault(kFlexBehaviorKey, kUnbounded.WithWeight(0));
  layout_->SetFlexAllocationOrder(FlexAllocationOrder::kNormal);
  View* const v1 = AddChild({10, 10});
  View* const v2 = AddChild({10, 10});
  View* const v3 = AddChild({10, 10});

  host_->SetSize({30, 10});
  EXPECT_EQ(gfx::Size(10, 10), v1->size());
  EXPECT_EQ(gfx::Size(10, 10), v2->size());
  EXPECT_EQ(gfx::Size(10, 10), v3->size());

  host_->SetSize({50, 10});
  EXPECT_EQ(gfx::Size(30, 10), v1->size());
  EXPECT_EQ(gfx::Size(10, 10), v2->size());
  EXPECT_EQ(gfx::Size(10, 10), v3->size());
}

TEST_F(FlexLayoutTest, FlexAllocationOrderReverseWithExcess) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetDefault(kFlexBehaviorKey, kUnbounded.WithWeight(0));
  layout_->SetFlexAllocationOrder(FlexAllocationOrder::kReverse);
  View* const v1 = AddChild({10, 10});
  View* const v2 = AddChild({10, 10});
  View* const v3 = AddChild({10, 10});

  host_->SetSize({30, 10});
  EXPECT_EQ(gfx::Size(10, 10), v1->size());
  EXPECT_EQ(gfx::Size(10, 10), v2->size());
  EXPECT_EQ(gfx::Size(10, 10), v3->size());

  host_->SetSize({50, 10});
  EXPECT_EQ(gfx::Size(10, 10), v1->size());
  EXPECT_EQ(gfx::Size(10, 10), v2->size());
  EXPECT_EQ(gfx::Size(30, 10), v3->size());
}

// Specific Regression Cases ---------------------------------------------------

// Test case (and example code) for crbug.com/1012119:
// "FlexLayout ignores custom flex rule if it contradicts preferred size"
TEST_F(FlexLayoutTest, FlexRuleContradictsPreferredSize) {
  const FlexSpecification custom_spec(
      base::BindRepeating([](const View* view, const SizeBounds& maximum_size) {
        return gfx::Size((maximum_size.width() >= 100) ? 100 : 0, 100);
      }));

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  View* const v1 = AddChild(gfx::Size(7, 7));
  View* const v2 = AddChild(gfx::Size(7, 7));
  v1->SetProperty(kFlexBehaviorKey, custom_spec.WithOrder(1));
  v2->SetProperty(kFlexBehaviorKey, kUnboundedScaleToZero);

  host_->SetSize({200, 100});
  std::vector<gfx::Rect> expected = {{0, 0, 100, 100}, {100, 0, 100, 100}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({101, 100});
  expected = {{0, 0, 100, 100}, {100, 0, 1, 100}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({100, 100});
  expected = {{0, 0, 100, 100}, {}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({99, 100});
  expected = {{}, {0, 0, 99, 100}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({1, 100});
  expected = {{}, {0, 0, 1, 100}};
  EXPECT_EQ(expected, GetChildBounds());
}

// Test case (and example code) for crbug.com/1012136:
// "FlexLayout makes children with preferred main axis size 0 invisible even if
//  they are kUnbounded"
TEST_F(FlexLayoutTest, PreferredSizeZeroPreventsFlex_Horizontal) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal);

  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  AddChild(gfx::Size(10, 10));
  View* const v1 = AddChild(gfx::Size(0, 10));
  View* const v2 = AddChild(gfx::Size(0, 10));
  v1->SetProperty(kFlexBehaviorKey, kUnboundedScaleToZero);
  v2->SetProperty(kFlexBehaviorKey, kUnboundedSnapToZero);

  host_->SetSize({30, 15});
  std::vector<gfx::Rect> expected = {
      {0, 0, 10, 10}, {10, 0, 10, 15}, {20, 0, 10, 15}};
  EXPECT_EQ(expected, GetChildBounds());

  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  test::RunScheduledLayout(host_.get());
  expected = {{0, 0, 10, 15}, {10, 0, 10, 15}, {20, 0, 10, 15}};
  EXPECT_EQ(expected, GetChildBounds());
}

// Test case (and example code) for crbug.com/1012136:
// "FlexLayout makes children with preferred main axis size 0 invisible even if
//  they are kUnbounded"
TEST_F(FlexLayoutTest, PreferredSizeZeroPreventsFlex_Vertical) {
  layout_->SetOrientation(LayoutOrientation::kVertical);

  layout_->SetCrossAxisAlignment(LayoutAlignment::kStart);
  AddChild(gfx::Size(10, 10));
  View* const v1 = AddChild(gfx::Size(10, 0));
  View* const v2 = AddChild(gfx::Size(10, 0));
  v1->SetProperty(kFlexBehaviorKey, kUnboundedScaleToZero);
  v2->SetProperty(kFlexBehaviorKey, kUnboundedSnapToZero);

  host_->SetSize({15, 30});
  std::vector<gfx::Rect> expected = {
      {0, 0, 10, 10}, {0, 10, 15, 10}, {0, 20, 15, 10}};
  EXPECT_EQ(expected, GetChildBounds());

  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  test::RunScheduledLayout(host_.get());
  expected = {{0, 0, 15, 10}, {0, 10, 15, 10}, {0, 20, 15, 10}};
  EXPECT_EQ(expected, GetChildBounds());
}

// Test case should be fixed by:
// https://chromium-review.googlesource.com/c/chromium/src/+/2420128
// Specifically, a label in a flex layout should report preferred height of
// |max_lines| * |line_height| when width is zero, which should affect the
// height bound of the layout.
TEST_F(FlexLayoutTest, LabelPreferredHeightChangesWithWidth) {
  // A LayoutProvider must exist in scope in order to create a Label.
  LayoutProvider layout_provider;

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  AddChild(gfx::Size(20, 20));
  Label* const label = host_->AddChildView(std::make_unique<Label>());
  label->SetMultiLine(true);
  label->SetMaxLines(2);
  label->SetLineHeight(20);
  label->SetProperty(kFlexBehaviorKey,
                     FlexSpecification(LayoutOrientation::kHorizontal,
                                       MinimumFlexSizeRule::kScaleToZero,
                                       MaximumFlexSizeRule::kPreferred,
                                       /*use_height_for_width*/ true));
  // Use a long text string with lots of small words that will wrap.
  label->SetText(u"The quick brown fox jumps over the lazy dogs.");

  // Verify that when there is enough space, the label takes up a single line.
  host_->SizeToPreferredSize();
  EXPECT_EQ(20, host_->height());
  EXPECT_EQ(20, label->height());

  // Now shrink the layout enough that the line should wrap.
  const int new_width = host_->width() - 20;
  const int new_height = host_->GetHeightForWidth(new_width);
  EXPECT_EQ(new_height, 40);
  host_->SetSize(gfx::Size(new_width, new_height));
  EXPECT_EQ(40, label->height());
}

// Regression test for crbug.com/1239888:
// A vertical layout nested in a horizontal layout should be laid out properly.
// Specifically, it should get its full height if its child views need to grow
// vertically if they are compressed horizontally.
TEST_F(FlexLayoutTest, VerticalInHorizontalInVertical_HeightForWidth) {
  constexpr gfx::Size kChildSize(10, 10);
  auto* const horizontal = host_->AddChildView(std::make_unique<views::View>());
  auto* view1 = AddChild(horizontal, kChildSize);
  auto* const vertical =
      horizontal->AddChildView(std::make_unique<views::View>());
  auto* const view2 = AddChild(vertical, kChildSize);
  view2->set_size_mode(MockView::SizeMode::kFixedArea);
  auto* const view3 = AddChild(vertical, kChildSize);
  view3->set_size_mode(MockView::SizeMode::kFixedArea);
  auto* const view4 = AddChild(kChildSize);

  layout_->SetOrientation(LayoutOrientation::kVertical);
  horizontal->SetProperty(
      kFlexBehaviorKey,
      FlexSpecification(
          horizontal->SetLayoutManager(std::make_unique<FlexLayout>())
              ->SetOrientation(LayoutOrientation::kHorizontal)
              .SetCrossAxisAlignment(LayoutAlignment::kStart)
              .GetDefaultFlexRule()));
  vertical->SetProperty(
      kFlexBehaviorKey,
      FlexSpecification(
          vertical->SetLayoutManager(std::make_unique<FlexLayout>())
              ->SetOrientation(LayoutOrientation::kVertical)
              .GetDefaultFlexRule()));

  const views::FlexSpecification view_flex(
      views::LayoutOrientation::kVertical,
      views::MinimumFlexSizeRule::kPreferred,
      views::MaximumFlexSizeRule::kPreferred,
      /* adjust_height_for_width = */ true,
      views::MinimumFlexSizeRule::kScaleToZero);
  view2->SetProperty(kFlexBehaviorKey, view_flex);
  view3->SetProperty(kFlexBehaviorKey, view_flex);

  EXPECT_EQ(gfx::Size(20, 30), host_->GetPreferredSize({}));
  EXPECT_EQ(40, horizontal->GetHeightForWidth(15));
  EXPECT_EQ(50, host_->GetHeightForWidth(15));

  host_->SetSize(gfx::Size(15, 50));
  EXPECT_EQ(gfx::Rect(0, 0, 15, 40), horizontal->bounds());
  EXPECT_EQ(gfx::Rect(0, 40, 15, 10), view4->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), view1->bounds());
  EXPECT_EQ(gfx::Rect(10, 0, 5, 40), vertical->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 20), view2->bounds());
  EXPECT_EQ(gfx::Rect(0, 20, 5, 20), view3->bounds());
}

// Pixel-Perfect/Advanced Tests ------------------------------------------------

// NOTE: these are tests ensuring the quasi-multipass behavior of FlexLayout
// (i.e. special-case handling when views do not take up exactly as much space
// as they are offered and need to have the excess redistributed).

namespace {

// Flex rule that steps each dimension by 5.
Size StepwiseFlexRule(int step,
                      const View* view,
                      const SizeBounds& maximum_size) {
  DCHECK_GT(step, 0);
  Size preferred = view->GetPreferredSize({});
  if (maximum_size.width().is_bounded()) {
    const int rounded = step * (maximum_size.width().value() / step);
    preferred.SetToMax({rounded, 0});
  }
  if (maximum_size.height().is_bounded()) {
    const int rounded = step * (maximum_size.height().value() / step);
    preferred.SetToMax({0, rounded});
  }
  return preferred;
}

}  // namespace

// When a view does not take all of the space granted it when excess space is
// being distributed (views flexing above their preferred size) the remaining
// space should be distributed to other views at that flex order.
TEST_F(FlexLayoutTest, Advanced_ViewDoesNotTakeFullExcess_Reallocation) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(LayoutAlignment::kStart)
      .SetCollapseMargins(true)
      .SetDefault(kMarginsKey, gfx::Insets(5));
  View* const v1 = AddChild(gfx::Size(10, 10));
  View* const v2 = AddChild(gfx::Size(10, 10));

  // By putting the view that expands stepwise after the one that can scale
  // smoothly but at the same priority, we test the ability of the layout to
  // detect that the stepwise view isn't using all of its space and
  // redistribute that back to the first view.
  v1->SetProperty(kFlexBehaviorKey,
                  FlexSpecification(MinimumFlexSizeRule::kPreferred,
                                    MaximumFlexSizeRule::kUnbounded));
  v2->SetProperty(kFlexBehaviorKey,
                  FlexSpecification(base::BindRepeating(&StepwiseFlexRule, 5)));

  // All views at preferred size.
  host_->SetSize({35, 20});
  std::vector<gfx::Rect> expected = {{5, 5, 10, 10}, {20, 5, 10, 10}};
  EXPECT_EQ(expected, GetChildBounds());

  // Small increase goes to the first view since the second can't use it.
  host_->SetSize({37, 20});
  expected = {{5, 5, 12, 10}, {22, 5, 10, 10}};
  EXPECT_EQ(expected, GetChildBounds());

  // This would be enough to step up the second view but it needs to be
  // distributed over both views, so the second view still can't use it.
  host_->SetSize({40, 20});
  expected = {{5, 5, 15, 10}, {25, 5, 10, 10}};
  EXPECT_EQ(expected, GetChildBounds());

  // There is finally enough space to allocate to the second view that it can
  // increase its size.
  host_->SetSize({45, 20});
  expected = {{5, 5, 15, 10}, {25, 5, 15, 10}};
  EXPECT_EQ(expected, GetChildBounds());
}

// When preferred size of views is zero and adding in zero-size views in the
// "allocate excess flex space" phase would put us above the total available
// size, none of them should show.
TEST_F(FlexLayoutTest, Advanced_PreferredSizeZero_AllOrNothing) {
  const FlexSpecification spec_scale = FlexSpecification(
      MinimumFlexSizeRule::kScaleToZero, MaximumFlexSizeRule::kUnbounded);

  layout_->SetOrientation(LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(LayoutAlignment::kStart)
      .SetCollapseMargins(true)
      .SetDefault(kFlexBehaviorKey, spec_scale)
      .SetDefault(kMarginsKey, gfx::Insets(5));
  View* const v1 = AddChild(gfx::Size(10, 0));
  View* const v2 = AddChild(gfx::Size(10, 0));

  host_->SetSize({20, 15});
  EXPECT_FALSE(v1->GetVisible());
  EXPECT_FALSE(v2->GetVisible());
  std::vector<gfx::Rect> expected = {{}, {}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({20, 16});
  EXPECT_FALSE(v1->GetVisible());
  EXPECT_FALSE(v2->GetVisible());
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({20, 17});
  EXPECT_TRUE(v1->GetVisible());
  EXPECT_TRUE(v2->GetVisible());
  expected = {{5, 5, 10, 1}, {5, 11, 10, 1}};
  EXPECT_EQ(expected, GetChildBounds());
}

// Individual cross-axis alignment test ----------------------------------------

TEST_F(FlexLayoutTest, IndividualCrossAxisAlignmentInHorizontalLayoutTest) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal)
      .SetDefault(kMarginsKey, gfx::Insets(5));
  View* const v1 = AddChild(gfx::Size(10, 10));
  v1->SetProperty(kCrossAxisAlignmentKey, LayoutAlignment::kStart);
  View* const v2 = AddChild(gfx::Size(10, 10));
  v2->SetProperty(kCrossAxisAlignmentKey, LayoutAlignment::kCenter);
  View* const v3 = AddChild(gfx::Size(10, 10));
  v3->SetProperty(kCrossAxisAlignmentKey, LayoutAlignment::kEnd);
  View* const v4 = AddChild(gfx::Size(10, 10));
  v4->SetProperty(kCrossAxisAlignmentKey, LayoutAlignment::kStretch);
  View* const v5 = AddChild(gfx::Size(10, 10));
  // v5 uses default

  host_->SizeToPreferredSize();
  EXPECT_EQ(5, v1->y());
  EXPECT_EQ(10, v1->height());
  EXPECT_EQ(5, v2->y());
  EXPECT_EQ(10, v2->height());
  EXPECT_EQ(5, v3->y());
  EXPECT_EQ(10, v3->height());
  EXPECT_EQ(5, v4->y());
  EXPECT_EQ(10, v4->height());
  EXPECT_EQ(5, v5->y());
  EXPECT_EQ(10, v5->height());

  // Next try a larger view.
  host_->SetSize(gfx::Size(100, 30));
  EXPECT_EQ(5, v1->y());
  EXPECT_EQ(10, v1->height());
  EXPECT_EQ(10, v2->y());
  EXPECT_EQ(10, v2->height());
  EXPECT_EQ(15, v3->y());
  EXPECT_EQ(10, v3->height());
  EXPECT_EQ(5, v4->y());
  EXPECT_EQ(20, v4->height());
  EXPECT_EQ(5, v5->y());
  EXPECT_EQ(20, v5->height());

  // Move to a smaller view.
  host_->SetSize(gfx::Size(100, 12));
  EXPECT_EQ(5, v1->y());
  EXPECT_EQ(10, v1->height());
  EXPECT_EQ(1, v2->y());
  EXPECT_EQ(10, v2->height());
  EXPECT_EQ(-3, v3->y());
  EXPECT_EQ(10, v3->height());
  EXPECT_EQ(5, v4->y());
  EXPECT_EQ(2, v4->height());
  EXPECT_EQ(5, v5->y());
  EXPECT_EQ(2, v5->height());

  // Change default cross-axis alignment.
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  test::RunScheduledLayout(host_.get());
  // v1-v4 should remain unchanged.
  EXPECT_EQ(5, v1->y());
  EXPECT_EQ(10, v1->height());
  EXPECT_EQ(1, v2->y());
  EXPECT_EQ(10, v2->height());
  EXPECT_EQ(-3, v3->y());
  EXPECT_EQ(10, v3->height());
  EXPECT_EQ(5, v4->y());
  EXPECT_EQ(2, v4->height());
  // Since v5 doesn't have its own alignment set, it should pick up the new
  // default.
  EXPECT_EQ(1, v5->y());
  EXPECT_EQ(10, v5->height());
}

TEST_F(FlexLayoutTest, IndividualCrossAxisAlignmentInVerticalLayoutTest) {
  layout_->SetOrientation(LayoutOrientation::kVertical)
      .SetDefault(kMarginsKey, gfx::Insets(5));
  View* const v1 = AddChild(gfx::Size(10, 10));
  v1->SetProperty(kCrossAxisAlignmentKey, LayoutAlignment::kStart);
  View* const v2 = AddChild(gfx::Size(10, 10));
  v2->SetProperty(kCrossAxisAlignmentKey, LayoutAlignment::kCenter);
  View* const v3 = AddChild(gfx::Size(10, 10));
  v3->SetProperty(kCrossAxisAlignmentKey, LayoutAlignment::kEnd);
  View* const v4 = AddChild(gfx::Size(10, 10));
  v4->SetProperty(kCrossAxisAlignmentKey, LayoutAlignment::kStretch);
  View* const v5 = AddChild(gfx::Size(10, 10));
  // v5 uses default

  host_->SizeToPreferredSize();
  EXPECT_EQ(5, v1->x());
  EXPECT_EQ(10, v1->width());
  EXPECT_EQ(5, v2->x());
  EXPECT_EQ(10, v2->width());
  EXPECT_EQ(5, v3->x());
  EXPECT_EQ(10, v3->width());
  EXPECT_EQ(5, v4->x());
  EXPECT_EQ(10, v4->width());
  EXPECT_EQ(5, v5->x());
  EXPECT_EQ(10, v5->width());

  // Next try a larger view.
  host_->SetSize(gfx::Size(30, 100));
  EXPECT_EQ(5, v1->x());
  EXPECT_EQ(10, v1->width());
  EXPECT_EQ(10, v2->x());
  EXPECT_EQ(10, v2->width());
  EXPECT_EQ(15, v3->x());
  EXPECT_EQ(10, v3->width());
  EXPECT_EQ(5, v4->x());
  EXPECT_EQ(20, v4->width());
  EXPECT_EQ(5, v5->x());
  EXPECT_EQ(20, v5->width());

  // Move to a smaller view.
  host_->SetSize(gfx::Size(12, 100));
  EXPECT_EQ(5, v1->x());
  EXPECT_EQ(10, v1->width());
  EXPECT_EQ(1, v2->x());
  EXPECT_EQ(10, v2->width());
  EXPECT_EQ(-3, v3->x());
  EXPECT_EQ(10, v3->width());
  EXPECT_EQ(5, v4->x());
  EXPECT_EQ(2, v4->width());
  EXPECT_EQ(5, v5->x());
  EXPECT_EQ(2, v5->width());

  // Change default cross-axis alignment.
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  test::RunScheduledLayout(host_.get());
  // v1-v4 should remain unchanged.
  EXPECT_EQ(5, v1->x());
  EXPECT_EQ(10, v1->width());
  EXPECT_EQ(1, v2->x());
  EXPECT_EQ(10, v2->width());
  EXPECT_EQ(-3, v3->x());
  EXPECT_EQ(10, v3->width());
  EXPECT_EQ(5, v4->x());
  EXPECT_EQ(2, v4->width());
  // Since v5 doesn't have its own alignment set, it should pick up the new
  // default.
  EXPECT_EQ(1, v5->x());
  EXPECT_EQ(10, v5->width());
}

TEST_F(FlexLayoutTest, PreferredSizeMutationTest) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetIgnoreDefaultMainAxisMargins(true);

  // We want to specialize the maximum size and have different sizes within the
  // constraints of the scene
  const FlexSpecification custom_spec(
      base::BindRepeating([](const View* view, const SizeBounds& maximum_size) {
        // This custom rule looks strange, but it is constrained by the current
        // multi-line label using GetPreferredSize(const SizeBounds&
        // available_size). eg: HelpBubbleView. This is indeed the case. Here is
        // a simple simulation.
        return gfx::Size(maximum_size.width() >= 300
                             ? 300
                             : maximum_size.width().min_of(250),
                         24);
      }));

  View* const v1 = AddChild(gfx::Size(10, 24));
  v1->SetProperty(kFlexBehaviorKey, custom_spec.WithOrder(2));
  View* const v2 = AddChild(gfx::Size(24, 24));
  v2->SetProperty(kFlexBehaviorKey,
                  kUnbounded.WithAlignment(views::LayoutAlignment::kEnd));

  EXPECT_EQ(gfx::Size(324, 24), host_->GetPreferredSize({}));

  host_->SizeToPreferredSize();
  std::vector<Rect> expected = {{0, 0, 300, 24}, {300, 0, 24, 24}};
  EXPECT_EQ(expected, GetChildBounds());

  host_->SetSize({300, 24});
  expected = {{0, 0, 250, 24}, {276, 0, 24, 24}};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, PreferredSizeMutationTest2) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetIgnoreDefaultMainAxisMargins(true);

  // We want to specialize the maximum size and have different sizes within the
  // constraints of the scene
  const FlexSpecification custom_spec(
      base::BindRepeating([](const View* view, const SizeBounds& maximum_size) {
        // This custom rule looks strange, but it is constrained by the current
        // multi-line label using GetPreferredSize(const SizeBounds&
        // available_size). eg: HelpBubbleView. This is indeed the case. Here is
        // a simple simulation.
        return gfx::Size(maximum_size.width() >= 300
                             ? 300
                             : maximum_size.width().min_of(250),
                         24);
      }));

  View* const v1 = AddChild(gfx::Size(10, 24));
  v1->SetProperty(kFlexBehaviorKey, custom_spec.WithOrder(2));
  View* const v2 = AddChild(gfx::Size(24, 24));
  v2->SetProperty(kFlexBehaviorKey,
                  kUnbounded.WithAlignment(views::LayoutAlignment::kCenter));
  View* const v3 = AddChild(gfx::Size(10, 24));
  v3->SetProperty(kFlexBehaviorKey, custom_spec.WithOrder(2));

  EXPECT_EQ(gfx::Size(624, 24), host_->GetPreferredSize({}));

  host_->SizeToPreferredSize();
  std::vector<Rect> expected = {
      {0, 0, 300, 24}, {300, 0, 24, 24}, {324, 0, 300, 24}};
  EXPECT_EQ(expected, GetChildBounds());

  // Test using critical value 300
  host_->SetSize({300, 24});
  expected = {{0, 0, 138, 24}, {138, 0, 24, 24}, {162, 0, 138, 24}};
  EXPECT_EQ(expected, GetChildBounds());

  // Test using critical value 524, It comes from the critical value 250+250+24
  // Values in [524, 622.5) should behave the same.
  //
  // Why is it 622.5? Because when the space less than 1.5 (300+300+24-1.5) is
  // evenly distributed, the available space of v1 will become 300 due to
  // rounding.
  host_->SetSize({524, 24});
  expected = {{0, 0, 250, 24}, {250, 0, 24, 24}, {274, 0, 250, 24}};
  EXPECT_EQ(expected, GetChildBounds());

  // Test using critical value 623
  host_->SetSize({623, 24});
  expected = {{0, 0, 300, 24}, {324, 0, 24, 24}, {373, 0, 250, 24}};
  EXPECT_EQ(expected, GetChildBounds());
}

TEST_F(FlexLayoutTest, ZeroPreferedSizeView) {
  layout_->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(0, 5))
      .SetIgnoreDefaultMainAxisMargins(true);

  View* const v1 = AddChild(gfx::Size(10, 24));
  View* const v2 = AddChild(gfx::Size(0, 24));
  v2->SetProperty(
      kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kScaleToMaximum));
  View* const v3 = AddChild(gfx::Size(24, 24));

  EXPECT_EQ(gfx::Size(44, 24), host_->GetPreferredSize({}));

  host_->SizeToPreferredSize();
  std::vector<Rect> expected = {{0, 0, 10, 24}, {0, 0, 0, 0}, {20, 0, 24, 24}};
  EXPECT_EQ(expected, GetChildBounds());
  EXPECT_TRUE(v1->GetVisible());
  EXPECT_FALSE(v2->GetVisible());
  EXPECT_TRUE(v3->GetVisible());
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
  static constexpr std::array<gfx::Size, kNumChildren> kChildSizes = {
      {{10, 10}, {10, 10}, {10, 30}}};
  static constexpr std::array<gfx::Insets, kNumChildren> kChildMargins = {
      {gfx::Insets::TLBR(6, 0, 2, 0), gfx::Insets::TLBR(10, 0, 5, 0),
       gfx::Insets::TLBR(6, 0, 2, 0)}};

  std::vector<raw_ptr<View, VectorExperimental>> child_views_;
};

TEST_F(FlexLayoutCrossAxisFitTest, Layout_CrossStretch) {
  layout_->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  test::RunScheduledLayout(host_.get());

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
  test::RunScheduledLayout(host_.get());

  // These should all justify to the leading edge and keep their original size.
  for (size_t i = 0; i < kNumChildren; ++i) {
    EXPECT_EQ(kChildMargins[i].top(), child_views_[i]->origin().y());
    EXPECT_EQ(kChildSizes[i].height(), child_views_[i]->size().height());
  }
}

TEST_F(FlexLayoutCrossAxisFitTest, Layout_CrossCenter) {
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  test::RunScheduledLayout(host_.get());

  // First child view fits entirely in the host view with margins (18 DIPs).
  // The entire height (including margins) will be centered vertically.
  int remain = kHostSize.height() -
               (kChildSizes[0].height() + kChildMargins[0].height());
  int expected = remain / 2 + kChildMargins[0].top();
  EXPECT_EQ(expected, child_views_[0]->origin().y());

  // Second child view is smaller than the host view, but margins don't fit.
  // The margins will be scaled down.
  remain = kHostSize.height() - kChildSizes[0].height();
  expected =
      base::ClampRound(kChildMargins[1].top() * static_cast<float>(remain) /
                       kChildMargins[1].height());
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
  test::RunScheduledLayout(host_.get());

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

  View* AddGrandchild(size_t child_index,
                      const gfx::Size& preferred,
                      const std::optional<gfx::Size>& minimum = std::nullopt) {
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
  std::vector<raw_ptr<FlexLayout, VectorExperimental>> layouts_;
  View::Views children_;
};

TEST_F(NestedFlexLayoutTest, SetVisible_UpdatesLayout) {
  AddChildren(1);
  AddGrandchild(1, gfx::Size(5, 5));
  AddGrandchild(1, gfx::Size(5, 5));

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout(1)->SetOrientation(LayoutOrientation::kHorizontal);
  EXPECT_EQ(gfx::Size(10, 5), host_->GetPreferredSize({}));
  grandchild(1, 1)->SetVisible(false);
  EXPECT_EQ(gfx::Size(5, 5), host_->GetPreferredSize({}));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), grandchild(1, 2)->bounds());
}

TEST_F(NestedFlexLayoutTest, AddChild_UpdatesLayout) {
  AddChildren(1);
  AddGrandchild(1, gfx::Size(5, 5));

  layout_->SetOrientation(LayoutOrientation::kHorizontal);
  layout(1)->SetOrientation(LayoutOrientation::kHorizontal);
  EXPECT_EQ(gfx::Size(5, 5), host_->GetPreferredSize({}));
  AddGrandchild(1, gfx::Size(5, 5));
  EXPECT_EQ(gfx::Size(10, 5), host_->GetPreferredSize({}));
  test::RunScheduledLayout(host_.get());
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
  EXPECT_EQ(gfx::Size(10, 5), host_->GetPreferredSize({}));

  // Remove one grandchild view, avoiding a memory leak since the view is no
  // longer owned.
  View* const to_remove = grandchild(1, 2);
  child(1)->RemoveChildView(to_remove);
  delete to_remove;

  EXPECT_EQ(gfx::Size(5, 5), host_->GetPreferredSize({}));
  test::RunScheduledLayout(host_.get());
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
      .SetDefault(views::kMarginsKey, gfx::Insets::TLBR(2, 3, 4, 5))
      .SetInteriorMargin(gfx::Insets::TLBR(4, 3, 2, 1));

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

  EXPECT_EQ(gfx::Size(39, 29), host_->GetPreferredSize({}));
  host_->SetSize(gfx::Size(50, 30));
  test::RunScheduledLayout(host_.get());
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
      .SetDefault(views::kMarginsKey, gfx::Insets::TLBR(2, 3, 4, 5))
      .SetInteriorMargin(gfx::Insets::TLBR(4, 3, 2, 1));

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

  EXPECT_EQ(gfx::Size(53, 22), host_->GetPreferredSize({}));
  host_->SetSize(gfx::Size(60, 30));
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(gfx::Rect(6, 6, 16, 9), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(30, 6, 17, 10), child(2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 5, 5), grandchild(1, 1)->bounds());
  EXPECT_EQ(gfx::Rect(9, 2, 5, 5), grandchild(1, 2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 6, 6), grandchild(2, 1)->bounds());
  EXPECT_EQ(gfx::Rect(9, 2, 6, 6), grandchild(2, 2)->bounds());
}

TEST_F(NestedFlexLayoutTest, Layout_Flex) {
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
  child(1)->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToZero);
  child(2)->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToZero);

  layout(1)
      ->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(2))
      .SetInteriorMargin(gfx::Insets(2));
  grandchild(1, 1)->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToZero);
  grandchild(1, 2)->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToZero);

  layout(2)
      ->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(2))
      .SetInteriorMargin(gfx::Insets(2));
  grandchild(2, 1)->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToZero);
  grandchild(2, 2)->SetProperty(views::kFlexBehaviorKey, kFlex1ScaleToZero);

  EXPECT_EQ(gfx::Size(40, 14), host_->GetPreferredSize({}));

  host_->SetSize(gfx::Size(26, 15));
  EXPECT_EQ(gfx::Rect(2, 2, 9, 9), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(13, 2, 11, 10), child(2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 2, 5), grandchild(1, 1)->bounds());
  EXPECT_EQ(gfx::Rect(6, 2, 1, 5), grandchild(1, 2)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 3, 6), grandchild(2, 1)->bounds());
  EXPECT_EQ(gfx::Rect(7, 2, 2, 6), grandchild(2, 2)->bounds());

  host_->SetSize(gfx::Size(22, 15));
  EXPECT_EQ(gfx::Rect(2, 2, 7, 9), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(11, 2, 9, 10), child(2)->bounds());
  EXPECT_TRUE(grandchild(1, 1)->GetVisible());
  EXPECT_FALSE(grandchild(1, 2)->GetVisible());
  EXPECT_EQ(gfx::Rect(2, 2, 3, 5), grandchild(1, 1)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 2, 6), grandchild(2, 1)->bounds());
  EXPECT_EQ(gfx::Rect(6, 2, 1, 6), grandchild(2, 2)->bounds());

  host_->SetSize(gfx::Size(20, 15));
  EXPECT_EQ(gfx::Rect(2, 2, 6, 9), child(1)->bounds());
  EXPECT_EQ(gfx::Rect(10, 2, 8, 10), child(2)->bounds());
  EXPECT_TRUE(grandchild(1, 1)->GetVisible());
  EXPECT_FALSE(grandchild(1, 2)->GetVisible());
  EXPECT_EQ(gfx::Rect(2, 2, 2, 5), grandchild(1, 1)->bounds());
  EXPECT_EQ(gfx::Rect(2, 2, 1, 6), grandchild(2, 1)->bounds());
  EXPECT_EQ(gfx::Rect(5, 2, 1, 6), grandchild(2, 2)->bounds());
}

TEST_F(NestedFlexLayoutTest, UsingDefaultFlexRule) {
  AddChildren(2);
  AddGrandchild(1, gfx::Size(5, 5));
  AddGrandchild(2, gfx::Size(5, 5));
  AddGrandchild(2, gfx::Size(5, 5));

  child(1)->SetProperty(
      kFlexBehaviorKey,
      FlexSpecification(layout(1)->GetDefaultFlexRule()).WithOrder(2));
  child(2)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(layout(2)->GetDefaultFlexRule()));
  grandchild(1, 1)->SetProperty(kFlexBehaviorKey, kUnboundedScaleToZero);
  grandchild(2, 1)->SetProperty(kFlexBehaviorKey, kDropOut);
  grandchild(2, 2)->SetProperty(kFlexBehaviorKey, kDropOut);

  // Extra flex space is allocated to the first child view.
  host_->SetSize(gfx::Size(17, 5));
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Size(7, 5), child(1)->size());
  EXPECT_TRUE(grandchild(1, 1)->GetVisible());
  EXPECT_EQ(gfx::Size(7, 5), grandchild(1, 1)->size());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Size(10, 5), child(2)->size());
  EXPECT_TRUE(grandchild(2, 1)->GetVisible());
  EXPECT_EQ(gfx::Size(5, 5), grandchild(2, 1)->size());
  EXPECT_TRUE(grandchild(2, 2)->GetVisible());
  EXPECT_EQ(gfx::Size(5, 5), grandchild(2, 2)->size());

  // Leftover flex space is still allocated to the first child view even after
  // one of the grandchildren drops out.
  host_->SetSize(gfx::Size(8, 5));
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Size(3, 5), child(1)->size());
  EXPECT_TRUE(grandchild(1, 1)->GetVisible());
  EXPECT_EQ(gfx::Size(3, 5), grandchild(1, 1)->size());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Size(5, 5), child(2)->size());
  EXPECT_TRUE(grandchild(2, 1)->GetVisible());
  EXPECT_EQ(gfx::Size(5, 5), grandchild(2, 1)->size());
  EXPECT_FALSE(grandchild(2, 2)->GetVisible());

  // Leftover flex space is still allocated to the first child view even after
  // two of the grandchildren drop out.
  host_->SetSize(gfx::Size(4, 5));
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Size(4, 5), child(1)->size());
  EXPECT_TRUE(grandchild(1, 1)->GetVisible());
  EXPECT_EQ(gfx::Size(4, 5), grandchild(1, 1)->size());
  EXPECT_FALSE(child(2)->GetVisible()) << child(2)->bounds().ToString();

  // If there is no leftover space, the first child view is hidden.
  host_->SetSize(gfx::Size(5, 5));
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Size(5, 5), child(2)->size());
  EXPECT_TRUE(grandchild(2, 1)->GetVisible());
  EXPECT_EQ(gfx::Size(5, 5), grandchild(2, 1)->size());
  EXPECT_FALSE(grandchild(2, 2)->GetVisible());
}

TEST_F(NestedFlexLayoutTest, UnboundedZeroSize) {
  AddChildren(1);
  child(1)->SetProperty(views::kFlexBehaviorKey, kUnboundedScaleToZero);

  AddGrandchild(1, gfx::Size(0, 5));
  grandchild(1, 1)->SetProperty(views::kFlexBehaviorKey, kUnboundedScaleToZero);

  EXPECT_EQ(5, child(1)->GetPreferredSize({}).height());
  host_->SetSize(gfx::Size(100, 5));
  EXPECT_EQ(5, child(1)->GetPreferredSize({}).height());
  test::RunScheduledLayout(host_.get());
  EXPECT_EQ(5, child(1)->height());
}

namespace {

struct DirectionalFlexRuleTestParamRules {
  const char* const name;
  MinimumFlexSizeRule min_main_rule;
  MaximumFlexSizeRule max_main_rule;
  bool use_height_for_width = false;
  MinimumFlexSizeRule min_cross_rule = MinimumFlexSizeRule::kPreferred;
};

struct DirectionalFlexRuleTestParam {
  LayoutOrientation orientation;
  DirectionalFlexRuleTestParamRules rules;
  gfx::Size size;
  std::vector<gfx::Rect> expected;

  std::string ToString() const {
    std::ostringstream oss;
    PrintTo(orientation, &oss);
    oss << " " << rules.name << " " << size.ToString();
    return oss.str();
  }
};

// No flex in cross-axis direction.
static const DirectionalFlexRuleTestParamRules kNoCrossFlex = {
    "Main unbounded scale to minimum snap to zero, cross no flex.",
    MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
    MaximumFlexSizeRule::kUnbounded};

// Drop out on main axis, flex on cross axis.
static const DirectionalFlexRuleTestParamRules kMainDropOutCrossFlex = {
    "Main preferred snap to zero, cross unbounded scale to zero.",
    MinimumFlexSizeRule::kPreferredSnapToZero, MaximumFlexSizeRule::kPreferred,
    false, MinimumFlexSizeRule::kScaleToZero};

// Preferred height-for-width on main axis, scale to minimum snap to zero on
// cross axis. (Note: Vertical only!)
static const DirectionalFlexRuleTestParamRules kFlexUseHeightForWidth = {
    "Cross scale to minimum snap to zero, main use height for width.",
    MinimumFlexSizeRule::kPreferred, MaximumFlexSizeRule::kPreferred, true,
    MinimumFlexSizeRule::kScaleToMinimumSnapToZero};

const DirectionalFlexRuleTestParam DirectionalFlexRuleTestParamList[] = {
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(20, 10),
     {{0, 0, 10, 10}, {10, 0, 10, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(30, 10),
     {{0, 0, 10, 10}, {10, 0, 20, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(16, 10),
     {{0, 0, 10, 10}, {10, 0, 6, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(14, 10),
     {{0, 0, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(20, 20),
     {{0, 5, 10, 10}, {10, 5, 10, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(30, 20),
     {{0, 5, 10, 10}, {10, 5, 20, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(16, 20),
     {{0, 5, 10, 10}, {10, 5, 6, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(14, 20),
     {{0, 5, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(20, 6),
     {{0, -2, 10, 10}, {10, -2, 10, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(30, 6),
     {{0, -2, 10, 10}, {10, -2, 20, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(16, 6),
     {{0, -2, 10, 10}, {10, -2, 6, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(14, 6),
     {{0, -2, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(20, 4),
     {{0, -3, 10, 10}, {10, -3, 10, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(30, 4),
     {{0, -3, 10, 10}, {10, -3, 20, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(16, 4),
     {{0, -3, 10, 10}, {10, -3, 6, 10}}},
    {LayoutOrientation::kHorizontal,
     kNoCrossFlex,
     gfx::Size(14, 4),
     {{0, -3, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(20, 10),
     {{0, 0, 10, 10}, {10, 0, 10, 10}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(30, 10),
     {{0, 0, 10, 10}, {10, 0, 10, 10}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(16, 10),
     {{0, 0, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(14, 10),
     {{0, 0, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(20, 20),
     {{0, 5, 10, 10}, {10, 5, 10, 10}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(30, 20),
     {{0, 5, 10, 10}, {10, 5, 10, 10}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(16, 20),
     {{0, 5, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(14, 20),
     {{0, 5, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(20, 6),
     {{0, -2, 10, 10}, {10, 0, 10, 6}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(30, 6),
     {{0, -2, 10, 10}, {10, 0, 10, 6}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(16, 6),
     {{0, -2, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(14, 6),
     {{0, -2, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(20, 4),
     {{0, -3, 10, 10}, {10, 0, 10, 4}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(30, 4),
     {{0, -3, 10, 10}, {10, 0, 10, 4}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(16, 4),
     {{0, -3, 10, 10}, {}}},
    {LayoutOrientation::kHorizontal,
     kMainDropOutCrossFlex,
     gfx::Size(14, 4),
     {{0, -3, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(10, 20),
     {{0, 0, 10, 10}, {0, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(10, 30),
     {{0, 0, 10, 10}, {0, 10, 10, 20}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(10, 16),
     {{0, 0, 10, 10}, {0, 10, 10, 6}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(10, 14),
     {{0, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(20, 20),
     {{5, 0, 10, 10}, {5, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(20, 30),
     {{5, 0, 10, 10}, {5, 10, 10, 20}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(20, 16),
     {{5, 0, 10, 10}, {5, 10, 10, 6}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(20, 14),
     {{5, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(6, 20),
     {{-2, 0, 10, 10}, {-2, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(6, 30),
     {{-2, 0, 10, 10}, {-2, 10, 10, 20}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(6, 16),
     {{-2, 0, 10, 10}, {-2, 10, 10, 6}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(6, 14),
     {{-2, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(4, 20),
     {{-3, 0, 10, 10}, {-3, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(4, 30),
     {{-3, 0, 10, 10}, {-3, 10, 10, 20}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(4, 16),
     {{-3, 0, 10, 10}, {-3, 10, 10, 6}}},
    {LayoutOrientation::kVertical,
     kNoCrossFlex,
     gfx::Size(4, 14),
     {{-3, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(10, 20),
     {{0, 0, 10, 10}, {0, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(10, 30),
     {{0, 0, 10, 10}, {0, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(10, 16),
     {{0, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(10, 14),
     {{0, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(20, 20),
     {{5, 0, 10, 10}, {5, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(20, 30),
     {{5, 0, 10, 10}, {5, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(20, 16),
     {{5, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(20, 14),
     {{5, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(6, 20),
     {{-2, 0, 10, 10}, {0, 10, 6, 10}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(6, 30),
     {{-2, 0, 10, 10}, {0, 10, 6, 10}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(6, 16),
     {{-2, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(6, 14),
     {{-2, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(4, 20),
     {{-3, 0, 10, 10}, {0, 10, 4, 10}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(4, 30),
     {{-3, 0, 10, 10}, {0, 10, 4, 10}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(4, 16),
     {{-3, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kMainDropOutCrossFlex,
     gfx::Size(4, 14),
     {{-3, 0, 10, 10}, {}}},
    {LayoutOrientation::kVertical,
     kFlexUseHeightForWidth,
     gfx::Size(10, 20),
     {{0, 0, 10, 10}, {0, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kFlexUseHeightForWidth,
     gfx::Size(20, 20),
     {{5, 0, 10, 10}, {5, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kFlexUseHeightForWidth,
     gfx::Size(10, 30),
     {{0, 0, 10, 10}, {0, 10, 10, 10}}},
    {LayoutOrientation::kVertical,
     kFlexUseHeightForWidth,
     gfx::Size(8, 30),
     {{-1, 0, 10, 10}, {0, 10, 8, 12}}},
    {LayoutOrientation::kVertical,
     kFlexUseHeightForWidth,
     gfx::Size(6, 20),
     {{-2, 0, 10, 10}, {0, 10, 6, 16}}},
    {LayoutOrientation::kVertical,
     kFlexUseHeightForWidth,
     gfx::Size(4, 20),
     {{-3, 0, 10, 10}, {2, 10, 0, 20}}},
};

}  // anonymous namespace

class FlexLayoutDirectionalRuleTest
    : public FlexLayoutTest,
      public testing::WithParamInterface<DirectionalFlexRuleTestParam> {};

TEST_P(FlexLayoutDirectionalRuleTest, TestRules) {
  const DirectionalFlexRuleTestParam& param = GetParam();
  constexpr gfx::Size kChildSize(10, 10);
  constexpr gfx::Size kMinimumSize(5, 5);
  layout_->SetOrientation(param.orientation);
  layout_->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout_->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  AddChild(kChildSize);
  MockView* const child2 = AddChild(kChildSize, kMinimumSize);
  child2->SetProperty(
      kFlexBehaviorKey,
      FlexSpecification(param.orientation, param.rules.min_main_rule,
                        param.rules.max_main_rule,
                        param.rules.use_height_for_width,
                        param.rules.min_cross_rule));
  if (param.rules.use_height_for_width)
    child2->set_size_mode(MockView::SizeMode::kFixedArea);

  host_->SetSize(param.size);
  EXPECT_EQ(param.expected, GetChildBounds())
      << " Params: " << param.ToString();
}

INSTANTIATE_TEST_SUITE_P(,
                         FlexLayoutDirectionalRuleTest,
                         testing::ValuesIn(DirectionalFlexRuleTestParamList));

}  // namespace views
