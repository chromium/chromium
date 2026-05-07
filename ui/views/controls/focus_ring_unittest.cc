// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focus_ring.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

using FocusRingTest = ViewsTestBase;

// Ensure that the focus ring tracks its parent View's bounds, even if the
// parent has its own layout manager.
TEST_F(FocusRingTest, MatchesParentBounds) {
  const auto widget = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* const contents = widget->SetContentsView(std::make_unique<View>());
  auto* const child1 = contents->AddChildView(
      std::make_unique<StaticSizedView>(gfx::Size(20, 20)));
  child1->set_minimum_size(gfx::Size(10, 10));
  auto* const child2 = contents->AddChildView(
      std::make_unique<StaticSizedView>(gfx::Size(20, 20)));
  child2->set_minimum_size(gfx::Size(10, 10));
  auto* const layout =
      contents->SetLayoutManager(std::make_unique<FlexLayout>());
  layout->SetDefault(kFlexBehaviorKey,
                     FlexSpecification(LayoutOrientation::kHorizontal,
                                       MinimumFlexSizeRule::kScaleToMinimum,
                                       MaximumFlexSizeRule::kPreferred));
  FocusRing::Install(contents);
  auto* const focus_ring = FocusRing::Get(contents);

  // Start near the default size for the layout.
  widget->SetBounds({20, 20, 40, 20});
  widget->LayoutRootViewIfNecessary();

  // Ensure that the focus ring has been laid out.
  EXPECT_FALSE(focus_ring->needs_layout());

  // Expect the focus ring to be larger than its parent.
  EXPECT_LE(focus_ring->origin().x(), 0);
  EXPECT_LE(focus_ring->origin().y(), 0);

  // Calculate the focus ring insets (this might be different on different
  // platforms, so use the actual value).
  const auto insets =
      gfx::Insets::VH(focus_ring->origin().x(), focus_ring->origin().y());

  // Ensure that the ring has the appropriate bounds based on the insets and its
  // parent's size.
  gfx::Rect expected_bounds({0, 0}, contents->size());
  expected_bounds.Inset(insets);
  EXPECT_EQ(expected_bounds, focus_ring->bounds());

  // Shrink the widget and ensure the focus ring shrinks too.
  widget->SetBounds({20, 20, 20, 20});
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(focus_ring->needs_layout());

  expected_bounds = gfx::Rect({0, 0}, contents->size());
  expected_bounds.Inset(insets);
  EXPECT_EQ(expected_bounds, focus_ring->bounds());
}

TEST_F(FocusRingTest, FocusRingShouldPaintWithoutParent) {
  const auto widget = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* const contents = widget->SetContentsView(std::make_unique<View>());
  FocusRing::Install(contents);
  auto* const focus_ring = FocusRing::Get(contents);

  // Detach from parent (parent() becomes nullptr).
  // The new `parent()` check should safely catch this and return false.
  auto owned_focus_ring = contents->RemoveChildViewT(focus_ring);
  EXPECT_FALSE(owned_focus_ring->ShouldPaintForTesting());
}

TEST_F(FocusRingTest, FocusRingLayerWithPredicate) {
  const auto widget = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* const contents = widget->SetContentsView(std::make_unique<View>());
  FocusRing::Install(contents);
  auto* const focus_ring = FocusRing::Get(contents);

  bool predicate_state = false;
  focus_ring->SetHasFocusPredicate(base::BindRepeating(
      [](bool* state, const View* view) { return *state; }, &predicate_state));

  // Refresh() updates the layer state when the predicate values itself changes.
  focus_ring->Refresh();

  EXPECT_FALSE(focus_ring->layer());

  predicate_state = true;
  focus_ring->Refresh();
  EXPECT_TRUE(focus_ring->layer());

  predicate_state = false;
  focus_ring->Refresh();
  EXPECT_FALSE(focus_ring->layer());
}

TEST_F(FocusRingTest, FocusRingShouldPaintWithPredicate) {
  const auto widget = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* const contents = widget->SetContentsView(std::make_unique<View>());
  FocusRing::Install(contents);
  auto* const focus_ring = FocusRing::Get(contents);

  // By default, no predicate and no focus.
  EXPECT_FALSE(focus_ring->ShouldPaintForTesting());

  bool predicate_state = false;
  focus_ring->SetHasFocusPredicate(base::BindRepeating(
      [](bool* state, const View* view) { return *state; }, &predicate_state));

  EXPECT_FALSE(focus_ring->ShouldPaintForTesting());

  predicate_state = true;
  EXPECT_TRUE(focus_ring->ShouldPaintForTesting());

  predicate_state = false;
  EXPECT_FALSE(focus_ring->ShouldPaintForTesting());
}

TEST_F(FocusRingTest, FocusRingLayerWithNativeFocus) {
  const auto widget = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* const contents = widget->SetContentsView(std::make_unique<View>());
  contents->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  FocusRing::Install(contents);
  auto* const focus_ring = FocusRing::Get(contents);

  EXPECT_FALSE(focus_ring->layer());

  // The layer exists only when the parent has focus.
  contents->RequestFocus();
  EXPECT_TRUE(focus_ring->layer());

  contents->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(focus_ring->layer());
}

TEST_F(FocusRingTest, FocusRingShouldPaintWithNativeFocus) {
  const auto widget = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* const contents = widget->SetContentsView(std::make_unique<View>());
  contents->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  FocusRing::Install(contents);
  auto* const focus_ring = FocusRing::Get(contents);

  EXPECT_FALSE(focus_ring->ShouldPaintForTesting());

  // Expect ShouldPaint() be true only when the parent has focus.
  contents->RequestFocus();
  EXPECT_TRUE(focus_ring->ShouldPaintForTesting());

  contents->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(focus_ring->ShouldPaintForTesting());
}

}  // namespace views
