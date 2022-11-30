// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/physics_based_fling_curve.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

const float kDefaultPixelsPerInch = 96.f;
const float kBoostMultiplierUnboosted = 1.f;

TEST(PhysicsBasedFlingCurveTest, BasicFlingTestVelocityY) {
  const gfx::Vector2dF fling_velocity(0, 5000);
  base::TimeTicks now = base::TimeTicks::Now();
  const gfx::Vector2dF pixels_per_inch(kDefaultPixelsPerInch,
                                       kDefaultPixelsPerInch);
  const gfx::Size viewport(1920, 1080);

  PhysicsBasedFlingCurve curve(fling_velocity, now, pixels_per_inch,
                               kBoostMultiplierUnboosted, viewport);

  gfx::Vector2dF offset;
  gfx::Vector2dF velocity;
  gfx::Vector2dF delta;
  gfx::Vector2dF cumulative_scroll;

  EXPECT_TRUE(curve.ComputeScrollOffset(now - base::Milliseconds(20), &offset,
                                        &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_EQ(0, delta.x());
  EXPECT_EQ(0, delta.y());
  cumulative_scroll = offset;

  EXPECT_TRUE(curve.ComputeScrollOffset(now + base::Milliseconds(20), &offset,
                                        &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_EQ(0, delta.x());
  EXPECT_NEAR(delta.y(), 98, 1);
  cumulative_scroll = offset;

  EXPECT_TRUE(curve.ComputeScrollOffset(now + base::Milliseconds(250), &offset,
                                        &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_EQ(0, delta.x());
  EXPECT_NEAR(delta.y(), 923, 1);
  cumulative_scroll = offset;

  EXPECT_FALSE(curve.ComputeScrollOffset(
      now + base::Seconds(curve.curve_duration()), &offset, &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_EQ(0, delta.x());
  EXPECT_NEAR(delta.y(), 2218, 1);
  cumulative_scroll = offset;

  EXPECT_FALSE(
      curve.ComputeScrollOffset(now + base::Seconds(10), &offset, &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_TRUE(delta.IsZero());
}

TEST(PhysicsBasedFlingCurveTest, BasicFlingTestVelocityX) {
  const gfx::Vector2dF fling_velocity(5000, 0);
  base::TimeTicks now = base::TimeTicks::Now();
  const gfx::Vector2dF pixels_per_inch(kDefaultPixelsPerInch,
                                       kDefaultPixelsPerInch);
  const gfx::Size viewport(1920, 1080);

  PhysicsBasedFlingCurve curve(fling_velocity, now, pixels_per_inch,
                               kBoostMultiplierUnboosted, viewport);

  gfx::Vector2dF offset;
  gfx::Vector2dF velocity;
  gfx::Vector2dF delta;
  gfx::Vector2dF cumulative_scroll;

  EXPECT_TRUE(curve.ComputeScrollOffset(now - base::Milliseconds(20), &offset,
                                        &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_EQ(0, delta.x());
  EXPECT_EQ(0, delta.y());
  cumulative_scroll = offset;

  EXPECT_TRUE(curve.ComputeScrollOffset(now + base::Milliseconds(20), &offset,
                                        &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_NEAR(delta.x(), 99, 1);
  EXPECT_EQ(0, delta.y());
  cumulative_scroll = offset;

  EXPECT_TRUE(curve.ComputeScrollOffset(now + base::Milliseconds(250), &offset,
                                        &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_NEAR(delta.x(), 1054, 1);
  EXPECT_EQ(0, delta.y());
  cumulative_scroll = offset;

  EXPECT_FALSE(
      curve.ComputeScrollOffset(now + base::Seconds(10), &offset, &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_NEAR(delta.x(), 3571, 1);
  EXPECT_EQ(0, delta.y());
  cumulative_scroll = offset;

  EXPECT_FALSE(
      curve.ComputeScrollOffset(now + base::Seconds(20), &offset, &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_TRUE(delta.IsZero());
}

TEST(PhysicsBasedFlingCurveTest, BasicFlingTestVelocityXAndY) {
  const gfx::Vector2dF fling_velocity(2000, 4000);
  base::TimeTicks now = base::TimeTicks::Now();
  const gfx::Vector2dF pixels_per_inch(kDefaultPixelsPerInch,
                                       kDefaultPixelsPerInch);
  const gfx::Size viewport(1920, 1080);

  PhysicsBasedFlingCurve curve(fling_velocity, now, pixels_per_inch,
                               kBoostMultiplierUnboosted, viewport);

  gfx::Vector2dF offset;
  gfx::Vector2dF velocity;
  gfx::Vector2dF delta;
  gfx::Vector2dF cumulative_scroll;

  EXPECT_TRUE(curve.ComputeScrollOffset(now - base::Milliseconds(20), &offset,
                                        &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_EQ(0, delta.x());
  EXPECT_EQ(0, delta.y());
  cumulative_scroll = offset;

  EXPECT_TRUE(curve.ComputeScrollOffset(now + base::Milliseconds(20), &offset,
                                        &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_NEAR(delta.x(), 19, 1);
  EXPECT_NEAR(delta.y(), 79, 1);
  cumulative_scroll = offset;

  EXPECT_TRUE(curve.ComputeScrollOffset(now + base::Milliseconds(250), &offset,
                                        &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_NEAR(delta.x(), 200, 1);
  EXPECT_NEAR(delta.y(), 803, 1);
  cumulative_scroll = offset;

  EXPECT_FALSE(
      curve.ComputeScrollOffset(now + base::Seconds(10), &offset, &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_NEAR(delta.x(), 535, 1);
  EXPECT_NEAR(delta.y(), 2141, 1);
  cumulative_scroll = offset;

  EXPECT_FALSE(
      curve.ComputeScrollOffset(now + base::Seconds(20), &offset, &velocity));
  delta = offset - cumulative_scroll;
  EXPECT_TRUE(delta.IsZero());
}

TEST(PhysicsBasedFlingCurveTest, ControlPointsWithSlopeLessThan1) {
  const gfx::Vector2dF velocity(5000, 0);
  base::TimeTicks now = base::TimeTicks::Now();
  const gfx::Vector2dF pixels_per_inch(kDefaultPixelsPerInch,
                                       kDefaultPixelsPerInch);
  const gfx::Size viewport(1920, 1080);

  PhysicsBasedFlingCurve curve(velocity, now, pixels_per_inch,
                               kBoostMultiplierUnboosted, viewport);

  EXPECT_EQ(0.20f, curve.p1_for_testing().x());
  EXPECT_NEAR(curve.p1_for_testing().y(), 0.43f, 0.01f);
  EXPECT_EQ(0.55f, curve.p2_for_testing().x());
  EXPECT_EQ(1.0f, curve.p2_for_testing().y());
}

TEST(PhysicsBasedFlingCurveTest, ControlPointsWithSlopeGreaterThan1) {
  const gfx::Vector2dF velocity(15000, 0);
  base::TimeTicks now = base::TimeTicks::Now();
  const gfx::Vector2dF pixels_per_inch(kDefaultPixelsPerInch,
                                       kDefaultPixelsPerInch);
  const gfx::Size viewport(1920, 1080);

  PhysicsBasedFlingCurve curve(velocity, now, pixels_per_inch,
                               kBoostMultiplierUnboosted, viewport);

  EXPECT_NEAR(curve.p1_for_testing().x(), 0.19f, 0.01f);
  EXPECT_EQ(curve.p1_for_testing().y(), 1.0f);
  EXPECT_EQ(0.55f, curve.p2_for_testing().x());
  EXPECT_EQ(1.0f, curve.p2_for_testing().y());
}

}  // namespace ui
