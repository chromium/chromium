// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

#include <memory>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_gesture_curve.h"
#include "ui/events/gestures/fling_curve.h"

using blink::WebGestureCurve;

namespace ui {
TEST(WebGestureCurveImplTest, Basic) {
  gfx::Vector2dF velocity(5000, 0);
  gfx::Vector2dF offset;
  base::TimeTicks time;
  auto curve = WebGestureCurveImpl::CreateFromUICurveForTesting(
      std::unique_ptr<ui::GestureCurve>(new ui::FlingCurve(velocity, time)),
      offset);

  gfx::Vector2dF current_velocity;
  gfx::Vector2dF current_scroll_delta;
  gfx::Vector2dF cumulative_delta;
  EXPECT_TRUE(curve->Advance(0, current_velocity, current_scroll_delta));
  cumulative_delta += current_scroll_delta;
  EXPECT_TRUE(curve->Advance(0.25, current_velocity, current_scroll_delta));
  cumulative_delta += current_scroll_delta;
  EXPECT_NEAR(current_velocity.x(), 1878, 1);
  EXPECT_EQ(current_velocity.y(), 0);
  EXPECT_GT(cumulative_delta.x(), 0);
  EXPECT_TRUE(
      curve->Advance(0.45, current_velocity,
                     current_scroll_delta));  // Use non-uniform tick spacing.
  cumulative_delta += current_scroll_delta;

  // Ensure fling persists even if successive timestamps are identical.
  gfx::Vector2dF old_velocity = current_velocity;
  EXPECT_TRUE(curve->Advance(0.45, current_velocity, current_scroll_delta));
  EXPECT_EQ(gfx::Vector2dF(), current_scroll_delta);
  EXPECT_EQ(old_velocity, current_velocity);

  EXPECT_TRUE(curve->Advance(0.75, current_velocity, current_scroll_delta));
  cumulative_delta += current_scroll_delta;
  EXPECT_FALSE(curve->Advance(1.5, current_velocity, current_scroll_delta));
  cumulative_delta += current_scroll_delta;
  EXPECT_NEAR(cumulative_delta.x(), 1193, 1);
  EXPECT_EQ(cumulative_delta.y(), 0);
  EXPECT_EQ(current_velocity.x(), 0);
  EXPECT_EQ(current_velocity.y(), 0);
}

}  // namespace ui
