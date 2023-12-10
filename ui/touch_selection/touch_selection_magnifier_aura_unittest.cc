// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_magnifier_aura.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

namespace {

class TouchSelectionMagnifierAuraTest : public testing::Test {
 public:
  TouchSelectionMagnifierAuraTest()
      : disable_animations_(ScopedAnimationDurationScaleMode::ZERO_DURATION) {}

  TouchSelectionMagnifierAuraTest(const TouchSelectionMagnifierAuraTest&) =
      delete;
  TouchSelectionMagnifierAuraTest& operator=(
      const TouchSelectionMagnifierAuraTest&) = delete;

  ~TouchSelectionMagnifierAuraTest() override = default;

 private:
  ScopedAnimationDurationScaleMode disable_animations_;
};

// Tests that the magnifier is horizontally centered above a vertical caret.
TEST_F(TouchSelectionMagnifierAuraTest, BoundsForVerticalCaret) {
  TouchSelectionMagnifierAura magnifier;
  Layer magnifier_parent;
  magnifier_parent.SetBounds(gfx::Rect(500, 400));

  // Show the magnifier at a vertical caret.
  constexpr gfx::Point kCaretTop(300, 200);
  constexpr gfx::Point kCaretBottom(300, 210);
  magnifier.ShowFocusBound(&magnifier_parent, kCaretTop, kCaretBottom);

  // Magnifier should be horizontally centered above the caret.
  const gfx::Rect magnifier_bounds = magnifier.GetMagnifierBoundsForTesting();
  EXPECT_EQ(magnifier_bounds.CenterPoint().x(), kCaretTop.x());
  EXPECT_LT(magnifier_bounds.bottom(), kCaretBottom.y());

  // Zoomed content bounds should be centered on the caret.
  const gfx::Rect zoomed_contents_bounds =
      magnifier.GetZoomedContentsBoundsForTesting();
  EXPECT_EQ(zoomed_contents_bounds.CenterPoint().x(), kCaretTop.x());
  EXPECT_EQ(zoomed_contents_bounds.CenterPoint().y(),
            (kCaretTop.y() + kCaretBottom.y()) / 2);
}

// Tests that the magnifier bounds are updated as a caret moves.
TEST_F(TouchSelectionMagnifierAuraTest, BoundsUpdate) {
  TouchSelectionMagnifierAura magnifier;
  Layer magnifier_parent;
  magnifier_parent.SetBounds(gfx::Rect(500, 400));

  // Show the magnifier at a caret.
  constexpr gfx::Point kCaretTop(300, 200);
  constexpr gfx::Point kCaretBottom(300, 210);
  magnifier.ShowFocusBound(&magnifier_parent, kCaretTop, kCaretBottom);
  // Move and resize the caret.
  constexpr gfx::Point kUpdatedCaretTop(310, 190);
  constexpr gfx::Point kUpdatedCaretBottom(310, 220);
  magnifier.ShowFocusBound(&magnifier_parent, kUpdatedCaretTop,
                           kUpdatedCaretBottom);

  // Magnifier should be horizontally centered above the caret.
  const gfx::Rect magnifier_bounds = magnifier.GetMagnifierBoundsForTesting();
  EXPECT_EQ(magnifier_bounds.CenterPoint().x(), kUpdatedCaretTop.x());
  EXPECT_LT(magnifier_bounds.bottom(), kUpdatedCaretBottom.y());

  // Zoomed content bounds should be centered on the caret.
  const gfx::Rect zoomed_contents_bounds =
      magnifier.GetZoomedContentsBoundsForTesting();
  EXPECT_EQ(zoomed_contents_bounds.CenterPoint().x(), kUpdatedCaretTop.x());
  EXPECT_EQ(zoomed_contents_bounds.CenterPoint().y(),
            (kUpdatedCaretTop.y() + kUpdatedCaretBottom.y()) / 2);
}

// Tests that the magnifier is adjusted to stay inside the parent layer when
// showing a caret close to the left edge of the parent.
TEST_F(TouchSelectionMagnifierAuraTest, StaysInsideParentLeftEdge) {
  TouchSelectionMagnifierAura magnifier;
  Layer magnifier_parent;
  constexpr gfx::Rect kParentBounds(500, 400);
  magnifier_parent.SetBounds(kParentBounds);

  // Show the magnifier at a caret near the left edge of the parent.
  magnifier.ShowFocusBound(&magnifier_parent, gfx::Point(10, 200),
                           gfx::Point(10, 210));

  // Magnifier (and what's zoomed) should be contained in the parent bounds.
  EXPECT_TRUE(kParentBounds.Contains(magnifier.GetMagnifierBoundsForTesting()));
  EXPECT_TRUE(
      kParentBounds.Contains(magnifier.GetZoomedContentsBoundsForTesting()));
}

// Tests that the magnifier is adjusted to stay inside the parent layer when
// showing a caret close to the right edge of the parent.
TEST_F(TouchSelectionMagnifierAuraTest, StaysInsideParentRightEdge) {
  TouchSelectionMagnifierAura magnifier;
  Layer magnifier_parent;
  constexpr gfx::Rect kParentBounds(500, 400);
  magnifier_parent.SetBounds(kParentBounds);

  // Show the magnifier at a caret near the right edge of the parent.
  magnifier.ShowFocusBound(&magnifier_parent, gfx::Point(495, 200),
                           gfx::Point(495, 210));

  // Magnifier (and what's zoomed) should be contained in the parent bounds.
  EXPECT_TRUE(kParentBounds.Contains(magnifier.GetMagnifierBoundsForTesting()));
  EXPECT_TRUE(
      kParentBounds.Contains(magnifier.GetZoomedContentsBoundsForTesting()));
}

// Tests that the magnifier is adjusted to stay inside the parent layer when
// showing a caret close to the top edge of the parent.
TEST_F(TouchSelectionMagnifierAuraTest, StaysInsideParentTopEdge) {
  TouchSelectionMagnifierAura magnifier;
  Layer magnifier_parent;
  constexpr gfx::Rect kParentBounds(500, 400);
  magnifier_parent.SetBounds(kParentBounds);

  // Show the magnifier at a caret near the top edge of the parent.
  magnifier.ShowFocusBound(&magnifier_parent, gfx::Point(200, 2),
                           gfx::Point(200, 12));

  // Magnifier (and what's zoomed) should be contained in the parent bounds.
  EXPECT_TRUE(kParentBounds.Contains(magnifier.GetMagnifierBoundsForTesting()));
  EXPECT_TRUE(
      kParentBounds.Contains(magnifier.GetZoomedContentsBoundsForTesting()));
}

// Tests that the magnifier remains the same size even at the edge of the
// parent layer.
TEST_F(TouchSelectionMagnifierAuraTest, Size) {
  TouchSelectionMagnifierAura magnifier;
  Layer magnifier_parent;
  magnifier_parent.SetBounds(gfx::Rect(500, 400));

  // Show magnifier.
  magnifier.ShowFocusBound(&magnifier_parent, gfx::Point(300, 200),
                           gfx::Point(300, 210));
  const gfx::Size magnifier_size =
      magnifier.GetMagnifierBoundsForTesting().size();
  const gfx::Size zoom_content_size =
      magnifier.GetZoomedContentsBoundsForTesting().size();
  // Move the caret near the edge of the parent container.
  magnifier.ShowFocusBound(&magnifier_parent, gfx::Point(10, 3),
                           gfx::Point(10, 13));

  // Magnifier should remain the same size.
  EXPECT_EQ(magnifier.GetMagnifierBoundsForTesting().size(), magnifier_size);
  EXPECT_EQ(magnifier.GetZoomedContentsBoundsForTesting().size(),
            zoom_content_size);
}

// Tests that the magnifier can be reparented to a different layer if needed.
TEST_F(TouchSelectionMagnifierAuraTest, SwitchesParentLayer) {
  TouchSelectionMagnifierAura magnifier;

  Layer magnifier_parent;
  magnifier_parent.SetBounds(gfx::Rect(500, 400));
  magnifier.ShowFocusBound(&magnifier_parent, gfx::Point(10, 20),
                           gfx::Point(10, 30));
  // Reparent the magnifier.
  Layer new_parent;
  new_parent.SetBounds(gfx::Rect(600, 400));
  magnifier.ShowFocusBound(&new_parent, gfx::Point(200, 20),
                           gfx::Point(200, 30));

  // Magnifier should have the updated parent.
  EXPECT_EQ(magnifier.GetMagnifierParentForTesting(), &new_parent);
}

}  // namespace

}  // namespace ui
