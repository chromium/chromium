// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/paint_throbber.h"

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

namespace {
constexpr base::TimeDelta kEpsilon = base::Seconds(0.0001);

// Interval for one keyframe of the sweep animation.
constexpr base::TimeDelta kSweepAnimationInterval = base::Seconds(2.0 / 3.0);

// Interval for a quarter rotation of the spinner's start angle.
constexpr base::TimeDelta kSpinnerAnimationInterval = base::Milliseconds(392);
}  // namespace

TEST(PaintThrobberTest, ThrobberSpinningStateSweepAngles) {
  static constexpr struct {
    base::TimeDelta elapsed_time;
    int64_t expected_sweep;
  } test_cases[] = {{base::TimeDelta(), -270},
                    {kSweepAnimationInterval - kEpsilon, -5},
                    {kSweepAnimationInterval, 5},
                    {2 * kSweepAnimationInterval - kEpsilon, 270},
                    {2 * kSweepAnimationInterval, -270}};

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        base::ClampRound(
            CalculateThrobberSpinningState(test_case.elapsed_time).sweep_angle),
        test_case.expected_sweep)
        << "Elapsed time: " << test_case.elapsed_time;
  }
}

TEST(PaintThrobberTest, ThrobberSpinningStateSweepAnglesWithKeyframeOffset) {
  static constexpr struct {
    base::TimeDelta elapsed_time;
    int64_t expected_sweep;
  } test_cases[] = {{base::TimeDelta(), 5},  // Minimum angle is 5.
                    {kSweepAnimationInterval - kEpsilon, 270},
                    {kSweepAnimationInterval, -270},
                    {2 * kSweepAnimationInterval - kEpsilon, -5},
                    {2 * kSweepAnimationInterval, 5}};

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(base::ClampRound(
                  CalculateThrobberSpinningState(test_case.elapsed_time, 1)
                      .sweep_angle),
              test_case.expected_sweep)
        << "Elapsed time: " << test_case.elapsed_time;
  }
}

TEST(PaintThrobberTest, ThrobberSpinningStateStartAngle) {
  static constexpr struct {
    base::TimeDelta elapsed_time;
    int64_t expected_start_angle;
  } test_cases[] = {
      {base::TimeDelta(), 270},              // Starts at 270.
      {kSpinnerAnimationInterval, 360},      // Quarter rotation.
      {2 * kSpinnerAnimationInterval, 450},  // Quarter rotation.
      {3 * kSpinnerAnimationInterval, 540},  // Three quarter rotation.
      // Full rotation at the end of an arc period.
      {4 * kSpinnerAnimationInterval, 900},
      // Sweep angle should be 0 here, but is floored at 5
      // degrees, pushing the start angle back by 5.
      {5 * kSpinnerAnimationInterval, 985},
      {6 * kSpinnerAnimationInterval, 1080},  // Quarter rotation.
      // Full rotation at the end of an arc period.
      {7 * kSpinnerAnimationInterval, 1440},   // Quarter rotation.
      {8 * kSpinnerAnimationInterval, 1530}};  // Quarter rotation.

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        CalculateThrobberSpinningState(test_case.elapsed_time).start_angle,
        test_case.expected_start_angle)
        << "Elapsed time: " << test_case.elapsed_time;
  }
}

TEST(PaintThrobberTest, ThrobberSpinningStateStartAngleWithKeyframeOffset) {
  static constexpr struct {
    base::TimeDelta elapsed_time;
    int64_t expected_start_angle;
  } test_cases[] = {
      // Sweep angle should be 0 here, but is floored at 5 degrees, pushing
      // the start angle back by 5.
      {base::TimeDelta(), 265},
      {kSpinnerAnimationInterval, 360},
      // Full rotation at the end of an arc period.
      {2 * kSpinnerAnimationInterval, 720},
      {3 * kSpinnerAnimationInterval, 810},  // Quarter rotation.
      {4 * kSpinnerAnimationInterval, 900},  // Quarter rotation.
      {5 * kSpinnerAnimationInterval, 990},  // Quarter rotation.
      // Full rotation at the end of an arc period.
      {6 * kSpinnerAnimationInterval, 1350},
      {7 * kSpinnerAnimationInterval, 1440},   // Quarter rotation.
      {8 * kSpinnerAnimationInterval, 1530}};  // Quarter rotation.

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        CalculateThrobberSpinningState(test_case.elapsed_time, 1).start_angle,
        test_case.expected_start_angle)
        << "Elapsed time: " << test_case.elapsed_time;
  }
}

}  // namespace gfx
