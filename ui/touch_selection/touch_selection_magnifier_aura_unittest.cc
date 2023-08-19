// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_magnifier_aura.h"

#include <memory>

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
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}

  TouchSelectionMagnifierAuraTest(const TouchSelectionMagnifierAuraTest&) =
      delete;
  TouchSelectionMagnifierAuraTest& operator=(
      const TouchSelectionMagnifierAuraTest&) = delete;

  ~TouchSelectionMagnifierAuraTest() override = default;

  void SetUp() override {
    magnifier_ = std::make_unique<TouchSelectionMagnifierAura>();
  }

  // Returns the bounds of the magnified area in coordinates of the magnifier's
  // parent layer.
  gfx::Rect GetMagnifiedAreaBounds() const {
    return magnifier_->GetMagnifiedAreaBoundsForTesting();
  }

  const Layer* GetMagnifierParent() const {
    return magnifier_->GetMagnifierParentForTesting();
  }

  void ShowMagnifier(Layer* parent,
                     const gfx::Point& caret_top,
                     const gfx::Point& caret_bottom) {
    magnifier_->ShowFocusBound(parent, caret_top, caret_bottom);
  }

 private:
  std::unique_ptr<TouchSelectionMagnifierAura> magnifier_;
  ui::ScopedAnimationDurationScaleMode disable_animations_;
};

// Tests that the magnifier is horizontally centered above a vertical caret.
TEST_F(TouchSelectionMagnifierAuraTest, BoundsForVerticalCaret) {
  auto magnifier_parent = std::make_unique<Layer>();
  magnifier_parent->SetBounds(gfx::Rect(500, 400));

  gfx::Point caret_top(300, 200);
  gfx::Point caret_bottom(300, 210);
  ShowMagnifier(magnifier_parent.get(), caret_top, caret_bottom);
  gfx::Rect magnified_area_bounds = GetMagnifiedAreaBounds();
  EXPECT_EQ(magnified_area_bounds.CenterPoint().x(), caret_top.x());
  EXPECT_LT(magnified_area_bounds.bottom(), caret_top.y());

  // Move the caret.
  caret_top.Offset(10, -5);
  caret_bottom.Offset(10, -5);
  ShowMagnifier(magnifier_parent.get(), caret_top, caret_bottom);
  magnified_area_bounds = GetMagnifiedAreaBounds();
  EXPECT_EQ(magnified_area_bounds.CenterPoint().x(), caret_top.x());
  EXPECT_LT(magnified_area_bounds.bottom(), caret_top.y());

  // Show a differently sized caret.
  caret_bottom.Offset(0, 5);
  ShowMagnifier(magnifier_parent.get(), caret_top, caret_bottom);
  magnified_area_bounds = GetMagnifiedAreaBounds();
  EXPECT_EQ(magnified_area_bounds.CenterPoint().x(), caret_top.x());
  EXPECT_LT(magnified_area_bounds.bottom(), caret_top.y());
}

// Tests that the magnifier stays inside the parent layer when showing a caret
// close to the edge of the parent layer.
TEST_F(TouchSelectionMagnifierAuraTest, StaysInsideParentLayer) {
  auto magnifier_parent = std::make_unique<Layer>();
  constexpr gfx::Rect kParentBounds(500, 400);
  magnifier_parent->SetBounds(kParentBounds);

  // Left edge.
  ShowMagnifier(magnifier_parent.get(), gfx::Point(10, 200),
                gfx::Point(10, 210));
  EXPECT_TRUE(kParentBounds.Contains(GetMagnifiedAreaBounds()));

  // Top edge.
  ShowMagnifier(magnifier_parent.get(), gfx::Point(200, 2),
                gfx::Point(200, 12));
  EXPECT_TRUE(kParentBounds.Contains(GetMagnifiedAreaBounds()));

  // Right edge.
  ShowMagnifier(magnifier_parent.get(), gfx::Point(495, 200),
                gfx::Point(495, 210));
  EXPECT_TRUE(kParentBounds.Contains(GetMagnifiedAreaBounds()));
}

// Tests that the magnifier remains the same size even at the edge of the
// parent layer.
TEST_F(TouchSelectionMagnifierAuraTest, Size) {
  auto magnifier_parent = std::make_unique<Layer>();
  magnifier_parent->SetBounds(gfx::Rect(500, 400));

  ShowMagnifier(magnifier_parent.get(), gfx::Point(300, 200),
                gfx::Point(300, 210));
  const gfx::Size magnifier_layer_size = GetMagnifiedAreaBounds().size();

  // Move the caret near the edge of the parent container.
  ShowMagnifier(magnifier_parent.get(), gfx::Point(10, 3), gfx::Point(10, 13));
  EXPECT_EQ(GetMagnifiedAreaBounds().size(), magnifier_layer_size);
}

// Tests that the magnifier can be reparented to a different layer if needed.
TEST_F(TouchSelectionMagnifierAuraTest, SwitchesParentLayer) {
  auto magnifier_parent = std::make_unique<Layer>();
  magnifier_parent->SetBounds(gfx::Rect(500, 400));

  // Check that the magnifier is correctly parented.
  ShowMagnifier(magnifier_parent.get(), gfx::Point(10, 20), gfx::Point(10, 30));
  EXPECT_EQ(GetMagnifierParent(), magnifier_parent.get());

  // Check that the magnifier is correctly reparented.
  auto new_parent = std::make_unique<Layer>();
  new_parent->SetBounds(gfx::Rect(600, 400));
  ShowMagnifier(new_parent.get(), gfx::Point(200, 20), gfx::Point(200, 30));
  EXPECT_EQ(GetMagnifierParent(), new_parent.get());
}

}  // namespace

}  // namespace ui
