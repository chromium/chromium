// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/filtered_gesture_provider.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/motion_event_test_utils.h"

namespace ui {

class FilteredGestureProviderTest : public GestureProviderClient,
                                    public testing::Test {
 public:
  FilteredGestureProviderTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}
  ~FilteredGestureProviderTest() override {}

  // GestureProviderClient implementation.
  void OnGestureEvent(const GestureEventData&) override {}
  bool RequiresDoubleTapGestureEvents() const override { return false; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Single touch drag test: After touch-start, the moved_beyond_slop_region bit
// should stay unset as long as the touch movement is confined to the slop
// region. Once the touch moves beyond the slop region, the bit should remain
// set until (incl) touch-end.
TEST_F(FilteredGestureProviderTest, TouchMovedBeyondSlopRegion_SingleTouch) {
  GestureProvider::Config config;
  FilteredGestureProvider provider(config, this);

  const float kSlopRegion = config.gesture_detector_config.touch_slop;

  test::MockMotionEvent event;

  event.set_event_time(base::TimeTicks::Now());
  event.PressPoint(0, 0);
  auto result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_FALSE(result.moved_beyond_slop_region);

  event.MovePoint(0, kSlopRegion / 2.f, 0);
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_FALSE(result.moved_beyond_slop_region);

  event.MovePoint(0, kSlopRegion * 2.f, 0);
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_TRUE(result.moved_beyond_slop_region);

  event.MovePoint(0, kSlopRegion * 2.f, 0);
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_TRUE(result.moved_beyond_slop_region);

  event.MovePoint(0, 0, 0);
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_TRUE(result.moved_beyond_slop_region);

  event.ReleasePoint();
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_TRUE(result.moved_beyond_slop_region);

  // A new touch sequence should reset the bit.
  base::TimeTicks time = base::TimeTicks::Now();
  event.PressPoint(0, 0);
  event.set_event_time(time);
  result = provider.OnTouchEvent(event);
  ASSERT_TRUE(result.succeeded);
  EXPECT_FALSE(result.moved_beyond_slop_region);

  // A fling should set the bit right away.
  time += base::Milliseconds(10);
  event.MovePoint(0, kSlopRegion * 50, 0);
  event.set_event_time(time);
  result = provider.OnTouchEvent(event);
  ASSERT_TRUE(result.succeeded);
  EXPECT_TRUE(result.moved_beyond_slop_region);

  event.ReleasePoint();
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_TRUE(result.moved_beyond_slop_region);
}

// Multi-touch: The moved_beyond_slop_region bit should stay unset as long as
// all touch-points are stationary, and should be set after (including) the
// first movement in any touch-point.
TEST_F(FilteredGestureProviderTest, TouchMovedBeyondSlopRegion_MultiTouch) {
  GestureProvider::Config config;
  FilteredGestureProvider provider(config, this);

  const float kSlopRegion = config.gesture_detector_config.touch_slop;

  test::MockMotionEvent event;

  float x = 0;
  const float y0 = 0;
  const float y1 = kSlopRegion * 10.f;
  const float y2 = kSlopRegion * 20.f;

  event.set_event_time(base::TimeTicks::Now());
  event.PressPoint(x, y0);
  auto result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_FALSE(result.moved_beyond_slop_region);

  event.PressPoint(x, y1);
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_FALSE(result.moved_beyond_slop_region);

  event.PressPoint(x, y2);
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_FALSE(result.moved_beyond_slop_region);

  for (float multiplier = 0.5f; multiplier < 3.f; multiplier += 2.f) {
    x = kSlopRegion * multiplier;

    event.MovePoint(0, x, y0);
    result = provider.OnTouchEvent(event);
    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.moved_beyond_slop_region);

    event.MovePoint(0, x, y0);
    result = provider.OnTouchEvent(event);
    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.moved_beyond_slop_region);

    event.MovePoint(2, x, y2);
    result = provider.OnTouchEvent(event);
    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.moved_beyond_slop_region);

    event.MovePoint(1, x, y1);
    result = provider.OnTouchEvent(event);
    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.moved_beyond_slop_region);
  }

  for(int i = 0; i < 3; i++) {
    event.ReleasePoint();
    result = provider.OnTouchEvent(event);
    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.moved_beyond_slop_region);
  }
}

// Extra cancel events should be handled gracefully: https://crbug.com/1407442
TEST_F(FilteredGestureProviderTest, ExtraCancel) {
  GestureProvider::Config config;
  FilteredGestureProvider provider(config, this);

  test::MockMotionEvent event(MotionEvent::Action::CANCEL, base::TimeTicks(), 0,
                              0);
  auto result = provider.OnTouchEvent(event);
  EXPECT_FALSE(result.succeeded);
}

}  // namespace ui
