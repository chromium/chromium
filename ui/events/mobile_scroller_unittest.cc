// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/mobile_scroller.h"

namespace ui {
namespace {

const float kDefaultStartX = 7.f;
const float kDefaultStartY = 25.f;
const float kDefaultDeltaX = -20.f;
const float kDefaultDeltaY = 73.f;
const float kDefaultVelocityX = -350.f;
const float kDefaultVelocityY = 220.f;
const float kEpsilon = 1e-3f;

MobileScroller::Config DefaultConfig() {
  return MobileScroller::Config();
}

}  // namespace

using MobileScrollerTest = testing::Test;

TEST_F(MobileScrollerTest, Scroll) {
  MobileScroller scroller(DefaultConfig());
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Start a scroll and verify initialized values.
  scroller.StartScroll(kDefaultStartX, kDefaultStartY, kDefaultDeltaX,
                       kDefaultDeltaY, start_time);

  EXPECT_EQ(kDefaultStartX, scroller.GetStartX());
  EXPECT_EQ(kDefaultStartY, scroller.GetStartY());
  EXPECT_EQ(kDefaultStartX, scroller.GetCurrX());
  EXPECT_EQ(kDefaultStartY, scroller.GetCurrY());
  EXPECT_EQ(kDefaultStartX + kDefaultDeltaX, scroller.GetFinalX());
  EXPECT_EQ(kDefaultStartY + kDefaultDeltaY, scroller.GetFinalY());
  EXPECT_FALSE(scroller.IsFinished());
  EXPECT_EQ(base::TimeDelta(), scroller.GetTimePassed());

  // Advance halfway through the scroll.
  const base::TimeDelta scroll_duration = scroller.GetDuration();
  gfx::Vector2dF offset, velocity;
  EXPECT_TRUE(scroller.ComputeScrollOffset(start_time + scroll_duration / 2,
                                           &offset, &velocity));

  // Ensure we've moved in the direction of the delta, but have yet to reach
  // the target.
  EXPECT_GT(kDefaultStartX, offset.x());
  EXPECT_LT(kDefaultStartY, offset.y());
  EXPECT_LT(scroller.GetFinalX(), offset.x());
  EXPECT_GT(scroller.GetFinalY(), offset.y());
  EXPECT_FALSE(scroller.IsFinished());

  // Ensure our velocity is non-zero and in the same direction as the delta.
  EXPECT_GT(0.f, velocity.x() * kDefaultDeltaX);
  EXPECT_GT(0.f, velocity.y() * kDefaultDeltaY);
  EXPECT_TRUE(scroller.IsScrollingInDirection(kDefaultDeltaX, kDefaultDeltaY));

  // Repeated offset computations at the same timestamp should yield identical
  // results.
  float curr_x = offset.x();
  float curr_y = offset.y();
  float curr_velocity_x = velocity.x();
  float curr_velocity_y = velocity.y();
  EXPECT_TRUE(scroller.ComputeScrollOffset(start_time + scroll_duration / 2,
                                           &offset, &velocity));
  EXPECT_EQ(curr_x, offset.x());
  EXPECT_EQ(curr_y, offset.y());
  EXPECT_EQ(curr_velocity_x, velocity.x());
  EXPECT_EQ(curr_velocity_y, velocity.y());

  // Advance to the end.
  EXPECT_FALSE(scroller.ComputeScrollOffset(start_time + scroll_duration,
                                            &offset, &velocity));
  EXPECT_EQ(scroller.GetFinalX(), offset.x());
  EXPECT_EQ(scroller.GetFinalY(), offset.y());
  EXPECT_TRUE(scroller.IsFinished());
  EXPECT_EQ(scroll_duration, scroller.GetTimePassed());
  EXPECT_NEAR(0.f, velocity.x(), kEpsilon);
  EXPECT_NEAR(0.f, velocity.y(), kEpsilon);

  // Try to advance further; nothing should change.
  EXPECT_FALSE(scroller.ComputeScrollOffset(start_time + scroll_duration * 2,
                                            &offset, &velocity));
  EXPECT_EQ(scroller.GetFinalX(), offset.x());
  EXPECT_EQ(scroller.GetFinalY(), offset.y());
  EXPECT_TRUE(scroller.IsFinished());
  EXPECT_EQ(scroll_duration, scroller.GetTimePassed());
}

TEST_F(MobileScrollerTest, Fling) {
  MobileScroller scroller(DefaultConfig());
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Start a fling and verify initialized values.
  scroller.Fling(kDefaultStartX, kDefaultStartY, kDefaultVelocityX,
                 kDefaultVelocityY, INT_MIN, static_cast<float>(INT_MAX),
                 INT_MIN, static_cast<float>(INT_MAX), start_time);

  EXPECT_EQ(kDefaultStartX, scroller.GetStartX());
  EXPECT_EQ(kDefaultStartY, scroller.GetStartY());
  EXPECT_EQ(kDefaultStartX, scroller.GetCurrX());
  EXPECT_EQ(kDefaultStartY, scroller.GetCurrY());
  EXPECT_GT(kDefaultStartX, scroller.GetFinalX());
  EXPECT_LT(kDefaultStartY, scroller.GetFinalY());
  EXPECT_FALSE(scroller.IsFinished());
  EXPECT_EQ(base::TimeDelta(), scroller.GetTimePassed());

  // Advance halfway through the fling.
  const base::TimeDelta scroll_duration = scroller.GetDuration();
  gfx::Vector2dF offset, velocity;
  scroller.ComputeScrollOffset(start_time + scroll_duration / 2, &offset,
                               &velocity);

  // Ensure we've moved in the direction of the velocity, but have yet to reach
  // the target.
  EXPECT_GT(kDefaultStartX, offset.x());
  EXPECT_LT(kDefaultStartY, offset.y());
  EXPECT_LT(scroller.GetFinalX(), offset.x());
  EXPECT_GT(scroller.GetFinalY(), offset.y());
  EXPECT_FALSE(scroller.IsFinished());

  // Ensure our velocity is non-zero and in the same direction as the original
  // velocity.
  EXPECT_LT(0.f, velocity.x() * kDefaultVelocityX);
  EXPECT_LT(0.f, velocity.y() * kDefaultVelocityY);
  EXPECT_TRUE(
      scroller.IsScrollingInDirection(kDefaultVelocityX, kDefaultVelocityY));

  // Repeated offset computations at the same timestamp should yield identical
  // results.
  float curr_x = offset.x();
  float curr_y = offset.y();
  float curr_velocity_x = velocity.x();
  float curr_velocity_y = velocity.y();
  EXPECT_TRUE(scroller.ComputeScrollOffset(start_time + scroll_duration / 2,
                                           &offset, &velocity));
  EXPECT_EQ(curr_x, offset.x());
  EXPECT_EQ(curr_y, offset.y());
  EXPECT_EQ(curr_velocity_x, velocity.x());
  EXPECT_EQ(curr_velocity_y, velocity.y());

  // Advance to the end.
  EXPECT_FALSE(scroller.ComputeScrollOffset(start_time + scroll_duration,
                                            &offset, &velocity));
  EXPECT_EQ(scroller.GetFinalX(), offset.x());
  EXPECT_EQ(scroller.GetFinalY(), offset.y());
  EXPECT_TRUE(scroller.IsFinished());
  EXPECT_EQ(scroll_duration, scroller.GetTimePassed());
  EXPECT_NEAR(0.f, velocity.x(), kEpsilon);
  EXPECT_NEAR(0.f, velocity.y(), kEpsilon);

  // Try to advance further; nothing should change.
  EXPECT_FALSE(scroller.ComputeScrollOffset(start_time + scroll_duration * 2,
                                            &offset, &velocity));
  EXPECT_EQ(scroller.GetFinalX(), offset.x());
  EXPECT_EQ(scroller.GetFinalY(), offset.y());
  EXPECT_TRUE(scroller.IsFinished());
  EXPECT_EQ(scroll_duration, scroller.GetTimePassed());
}

}  // namespace ui
