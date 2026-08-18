// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/scoped_animation_duration_scale_mode.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

class ScopedAnimationDurationScaleModeTest : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_EQ(ScopedAnimationDurationScaleMode::NORMAL_DURATION,
              ScopedAnimationDurationScaleMode::duration_multiplier());
    EXPECT_FALSE(ScopedAnimationDurationScaleMode::is_zero());
  }

  void TearDown() override {
    EXPECT_EQ(ScopedAnimationDurationScaleMode::NORMAL_DURATION,
              ScopedAnimationDurationScaleMode::duration_multiplier());
    EXPECT_FALSE(ScopedAnimationDurationScaleMode::is_zero());
  }
};

TEST_F(ScopedAnimationDurationScaleModeTest, BasicDisableAnimation) {
  {
    const ScopedAnimationDurationScaleMode disable(
        ScopedAnimationDurationScaleMode::DISABLE_ANIMATION);
    EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
              ScopedAnimationDurationScaleMode::duration_multiplier());
    EXPECT_TRUE(ScopedAnimationDurationScaleMode::is_zero());
  }
  EXPECT_EQ(ScopedAnimationDurationScaleMode::NORMAL_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());
}

TEST_F(ScopedAnimationDurationScaleModeTest, NonLIFODisableAnimations) {
  auto scope_a = std::make_unique<ScopedAnimationDurationScaleMode>(
      ScopedAnimationDurationScaleMode::DISABLE_ANIMATION);
  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());

  auto scope_b = std::make_unique<ScopedAnimationDurationScaleMode>(
      ScopedAnimationDurationScaleMode::DISABLE_ANIMATION);
  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());

  // Destroy scope_a first (non-LIFO).
  scope_a.reset();

  // Multiplier must remain ZERO_DURATION because scope_b is still active.
  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());
  EXPECT_TRUE(ScopedAnimationDurationScaleMode::is_zero());

  // Destroy scope_b.
  scope_b.reset();

  // Multiplier must be restored to NORMAL_DURATION.
  EXPECT_EQ(ScopedAnimationDurationScaleMode::NORMAL_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());
  EXPECT_FALSE(ScopedAnimationDurationScaleMode::is_zero());
}

TEST_F(ScopedAnimationDurationScaleModeTest,
       IgnoreNonDisableWhileInDisableAnimation) {
  const ScopedAnimationDurationScaleMode disable(
      ScopedAnimationDurationScaleMode::DISABLE_ANIMATION);
  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());

  // Attempt to set non-disable duration while disable duration is active.
  {
    const ScopedAnimationDurationScaleMode slow(
        ScopedAnimationDurationScaleMode::SLOW_DURATION);
    // Should be ignored, remaining at ZERO_DURATION.
    EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
              ScopedAnimationDurationScaleMode::duration_multiplier());
    EXPECT_TRUE(ScopedAnimationDurationScaleMode::is_zero());
  }

  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());
}

TEST_F(ScopedAnimationDurationScaleModeTest,
       PreservesNonZeroInitialMultiplier) {
  const ScopedAnimationDurationScaleMode slow(
      ScopedAnimationDurationScaleMode::SLOW_DURATION);
  EXPECT_EQ(ScopedAnimationDurationScaleMode::SLOW_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());

  auto scope_a = std::make_unique<ScopedAnimationDurationScaleMode>(
      ScopedAnimationDurationScaleMode::DISABLE_ANIMATION);
  auto scope_b = std::make_unique<ScopedAnimationDurationScaleMode>(
      ScopedAnimationDurationScaleMode::DISABLE_ANIMATION);

  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());

  // Destroy non-LIFO.
  scope_a.reset();
  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());

  scope_b.reset();
  // Restores back to SLOW_DURATION.
  EXPECT_EQ(ScopedAnimationDurationScaleMode::SLOW_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());
}

TEST_F(ScopedAnimationDurationScaleModeTest,
       NonZeroExitsBeforeDisableAnimationsComplete) {
  // Start with normal duration (1.0).
  auto scope_non_zero = std::make_unique<ScopedAnimationDurationScaleMode>(
      ScopedAnimationDurationScaleMode::SLOW_DURATION);
  EXPECT_EQ(ScopedAnimationDurationScaleMode::SLOW_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());

  auto scope_disable = std::make_unique<ScopedAnimationDurationScaleMode>(
      ScopedAnimationDurationScaleMode::DISABLE_ANIMATION);
  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());

  // Non-zero scope exits while disable duration is still active.
  // It should update the pre-disable value to its old value (NORMAL_DURATION)
  // while keeping the current multiplier at ZERO_DURATION.
  scope_non_zero.reset();
  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());

  // When disable duration completes, it restores to the updated original value
  // (NORMAL_DURATION).
  scope_disable.reset();
  EXPECT_EQ(ScopedAnimationDurationScaleMode::NORMAL_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());
}

TEST_F(ScopedAnimationDurationScaleModeTest,
       ZeroDurationCanBeOverriddenByNestedScope) {
  const ScopedAnimationDurationScaleMode zero(
      ScopedAnimationDurationScaleMode::ZERO_DURATION);
  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());

  // Nested scope overrides ZERO_DURATION (standard test harness behavior).
  {
    const ScopedAnimationDurationScaleMode slow(
        ScopedAnimationDurationScaleMode::SLOW_DURATION);
    EXPECT_EQ(ScopedAnimationDurationScaleMode::SLOW_DURATION,
              ScopedAnimationDurationScaleMode::duration_multiplier());
    EXPECT_FALSE(ScopedAnimationDurationScaleMode::is_zero());
  }

  EXPECT_EQ(ScopedAnimationDurationScaleMode::ZERO_DURATION,
            ScopedAnimationDurationScaleMode::duration_multiplier());
}

}  // namespace gfx
