// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/snap_scroll_controller.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/motion_event_test_utils.h"

using base::TimeTicks;
using ui::test::MockMotionEvent;

namespace ui {
namespace {

const float kSnapBound = 8.f;

gfx::SizeF GetDisplayBounds() {
  return gfx::SizeF(640, 480);
}

}  // namespace

TEST(SnapScrollControllerTest, Basic) {
  SnapScrollController controller(GetDisplayBounds());
  EXPECT_FALSE(controller.IsSnappingScrolls());
  EXPECT_FALSE(controller.IsSnapHorizontal());
  EXPECT_FALSE(controller.IsSnapVertical());

  // Test basic horizontal snapping.
  MockMotionEvent event;
  event.PressPoint(0, 0);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  event.MovePoint(0, kSnapBound * 2, 0.f);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_TRUE(controller.IsSnappingScrolls());
  EXPECT_TRUE(controller.IsSnapHorizontal());
  EXPECT_FALSE(controller.IsSnapVertical());

  event.ReleasePoint();
  controller.SetSnapScrollMode(event, false, kSnapBound);

  // Test basic vertical snapping.
  event.PressPoint(0, 0);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  event.MovePoint(0, 0.f, kSnapBound * 2);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_TRUE(controller.IsSnappingScrolls());
  EXPECT_TRUE(controller.IsSnapVertical());
  EXPECT_FALSE(controller.IsSnapHorizontal());
}

TEST(SnapScrollControllerTest, VerticalScroll) {
  SnapScrollController controller(GetDisplayBounds());
  EXPECT_FALSE(controller.IsSnappingScrolls());

  MockMotionEvent event;
  event.PressPoint(0, 0);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  event.MovePoint(0, 0.f, -kSnapBound / 2.f);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  event.MovePoint(0, kSnapBound / 2.f, -kSnapBound * 2.f);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_TRUE(controller.IsSnapVertical());
  EXPECT_FALSE(controller.IsSnapHorizontal());

  // Initial scrolling should be snapped.
  float delta_x = event.GetX(0);
  float delta_y = event.GetY(0);
  controller.UpdateSnapScrollMode(delta_x, delta_y, kSnapBound);
  EXPECT_TRUE(controller.IsSnapVertical());
  EXPECT_FALSE(controller.IsSnapHorizontal());

  // Subsequent scrolling should be snapped as long as it's within the rails.
  delta_x = 5;
  delta_y = 10;
  controller.UpdateSnapScrollMode(delta_x, delta_y, kSnapBound);
  EXPECT_TRUE(controller.IsSnapVertical());
  EXPECT_FALSE(controller.IsSnapHorizontal());

  // Large horizontal movement should end snapping.
  delta_x = 100;
  delta_y = 10;
  controller.UpdateSnapScrollMode(delta_x, delta_y, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());
}

TEST(SnapScrollControllerTest, HorizontalScroll) {
  SnapScrollController controller(GetDisplayBounds());
  EXPECT_FALSE(controller.IsSnappingScrolls());

  MockMotionEvent event;
  event.PressPoint(0, 0);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  event.MovePoint(0, -kSnapBound / 2.f, 0.f);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  event.MovePoint(0, kSnapBound * 2.f, kSnapBound / 2.f);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_TRUE(controller.IsSnapHorizontal());
  EXPECT_FALSE(controller.IsSnapVertical());

  // Initial scrolling should be snapped.
  float delta_x = event.GetX(0);
  float delta_y = event.GetY(0);
  controller.UpdateSnapScrollMode(delta_x, delta_y, kSnapBound);
  EXPECT_TRUE(controller.IsSnapHorizontal());
  EXPECT_FALSE(controller.IsSnapVertical());

  // Subsequent scrolling should be snapped as long as it's within the rails.
  delta_x = 10;
  delta_y = 5;
  controller.UpdateSnapScrollMode(delta_x, delta_y, kSnapBound);
  EXPECT_TRUE(controller.IsSnapHorizontal());
  EXPECT_FALSE(controller.IsSnapVertical());

  // Large vertical movement should end snapping.
  delta_x = 10;
  delta_y = 100;
  controller.UpdateSnapScrollMode(delta_x, delta_y, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());
}

TEST(SnapScrollControllerTest, Diagonal) {
  SnapScrollController controller(GetDisplayBounds());
  EXPECT_FALSE(controller.IsSnappingScrolls());

  MockMotionEvent event;
  event.PressPoint(0, 0);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  // Sufficient initial diagonal motion will prevent any future snapping.
  event.MovePoint(0, kSnapBound * 3.f, -kSnapBound * 3.f);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  float delta_x = event.GetX(0);
  float delta_y = event.GetY(0);
  controller.UpdateSnapScrollMode(delta_x, delta_y, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  event.MovePoint(0, 0, 0);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  event.MovePoint(0, kSnapBound * 5, 0);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());

  event.MovePoint(0, 0, -kSnapBound * 5);
  controller.SetSnapScrollMode(event, false, kSnapBound);
  EXPECT_FALSE(controller.IsSnappingScrolls());
}

}  // namespace ui
