// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop_animator.h"

#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/installable_ink_drop_painter.h"

namespace views {

namespace {

class InstallableInkDropAnimatorTest : public ::testing::Test {
 public:
  InstallableInkDropAnimatorTest()
      : animation_container_(base::MakeRefCounted<gfx::AnimationContainer>()),
        animation_tester_(animation_container_.get()),
        callback_(base::BindRepeating(
            [](bool* callback_called) { *callback_called = true; },
            &callback_called_)) {}

 protected:
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<gfx::AnimationContainer> animation_container_;
  gfx::AnimationContainerTestApi animation_tester_;

  InstallableInkDropPainter::State visual_state_;

  bool callback_called_ = false;
  base::RepeatingClosure callback_;
};

}  // namespace

TEST_F(InstallableInkDropAnimatorTest, UpdatesTargetState) {
  InstallableInkDropAnimator animator(gfx::Size(2, 2), &visual_state_,
                                      animation_container_.get(),
                                      base::DoNothing());
  EXPECT_EQ(InkDropState::HIDDEN, animator.target_state());

  animator.AnimateToState(InkDropState::ACTIVATED);
  EXPECT_EQ(InkDropState::ACTIVATED, animator.target_state());
}

TEST_F(InstallableInkDropAnimatorTest, AnimateToTriggeredFromHidden) {
  InstallableInkDropAnimator animator(gfx::Size(10, 10), &visual_state_,
                                      animation_container_.get(), callback_);
  EXPECT_EQ(0.0f, visual_state_.flood_fill_progress);

  animator.SetLastEventLocation(gfx::Point(5, 5));
  animator.AnimateToState(InkDropState::ACTION_TRIGGERED);
  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, animator.target_state());
  EXPECT_GT(1.0f, visual_state_.flood_fill_progress);

  callback_called_ = false;
  animation_tester_.IncrementTime(
      InstallableInkDropAnimator::GetSubAnimationDurationForTesting(
          InstallableInkDropAnimator::SubAnimation::kActionPendingFloodFill));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, animator.target_state());
  EXPECT_EQ(1.0f, visual_state_.flood_fill_progress);
  EXPECT_TRUE(callback_called_);

  callback_called_ = false;
  animation_tester_.IncrementTime(
      InstallableInkDropAnimator::GetSubAnimationDurationForTesting(
          InstallableInkDropAnimator::SubAnimation::kActionTriggeredFadeOut));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::HIDDEN, animator.target_state());
  EXPECT_EQ(0.0f, visual_state_.flood_fill_progress);
  EXPECT_TRUE(callback_called_);
}

TEST_F(InstallableInkDropAnimatorTest,
       AnimateToPendingThenTriggeredFromHidden) {
  InstallableInkDropAnimator animator(gfx::Size(10, 10), &visual_state_,
                                      animation_container_.get(), callback_);

  animator.SetLastEventLocation(gfx::Point(5, 5));
  animator.AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_EQ(InkDropState::ACTION_PENDING, animator.target_state());
  EXPECT_GT(1.0f, visual_state_.flood_fill_progress);

  callback_called_ = false;
  animation_tester_.IncrementTime(
      InstallableInkDropAnimator::GetSubAnimationDurationForTesting(
          InstallableInkDropAnimator::SubAnimation::kActionPendingFloodFill));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::ACTION_PENDING, animator.target_state());
  EXPECT_EQ(1.0f, visual_state_.flood_fill_progress);
  EXPECT_TRUE(callback_called_);

  // The animation should be finished now and the visual state should *not*
  // change; ACTION_PENDING lasts indefinitely.
  animation_tester_.IncrementTime(
      InstallableInkDropAnimator::GetSubAnimationDurationForTesting(
          InstallableInkDropAnimator::SubAnimation::kActionTriggeredFadeOut));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::ACTION_PENDING, animator.target_state());
  EXPECT_EQ(1.0f, visual_state_.flood_fill_progress);

  // Animating to ACTION_TRIGGERED from ACTION_PENDING should not repeat the
  // flood-fill animation. Instead, it should just fade out.
  animator.AnimateToState(InkDropState::ACTION_TRIGGERED);
  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, animator.target_state());
  EXPECT_EQ(1.0f, visual_state_.flood_fill_progress);

  animation_tester_.IncrementTime(
      InstallableInkDropAnimator::GetSubAnimationDurationForTesting(
          InstallableInkDropAnimator::SubAnimation::kActionTriggeredFadeOut));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::HIDDEN, animator.target_state());
  EXPECT_EQ(0.0f, visual_state_.flood_fill_progress);
}

TEST_F(InstallableInkDropAnimatorTest,
       AnimateToPendingWhileAnimatingToTriggered) {
  const base::TimeDelta kPendingAnimationDuration =
      InstallableInkDropAnimator::GetSubAnimationDurationForTesting(
          InstallableInkDropAnimator::SubAnimation::kActionPendingFloodFill);
  const base::TimeDelta kHalfAnimationDuration = kPendingAnimationDuration / 2;
  const base::TimeDelta kRemainingAnimationDuration =
      kPendingAnimationDuration - kHalfAnimationDuration;

  InstallableInkDropAnimator animator(gfx::Size(10, 10), &visual_state_,
                                      animation_container_.get(), callback_);

  animator.SetLastEventLocation(gfx::Point(5, 5));
  animator.AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_EQ(InkDropState::ACTION_PENDING, animator.target_state());
  EXPECT_GT(1.0f, visual_state_.flood_fill_progress);

  callback_called_ = false;
  animation_tester_.IncrementTime(kHalfAnimationDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::ACTION_PENDING, animator.target_state());
  EXPECT_LT(0.0f, visual_state_.flood_fill_progress);
  EXPECT_GT(1.0f, visual_state_.flood_fill_progress);
  EXPECT_TRUE(callback_called_);

  // Switching to ACTION_TRIGGERED should not restart the animation. Instead, it
  // should just add a transition to HIDDEN after the flood-fill is done.
  animator.AnimateToState(InkDropState::ACTION_TRIGGERED);
  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, animator.target_state());
  EXPECT_LT(0.0f, visual_state_.flood_fill_progress);
  EXPECT_GT(1.0f, visual_state_.flood_fill_progress);

  callback_called_ = false;
  animation_tester_.IncrementTime(kRemainingAnimationDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, animator.target_state());
  EXPECT_EQ(1.0f, visual_state_.flood_fill_progress);
  EXPECT_TRUE(callback_called_);
}

TEST_F(InstallableInkDropAnimatorTest, AnimateToActivatedThenDeactivated) {
  InstallableInkDropAnimator animator(gfx::Size(10, 10), &visual_state_,
                                      animation_container_.get(), callback_);

  animator.SetLastEventLocation(gfx::Point(5, 5));
  animator.AnimateToState(InkDropState::ACTIVATED);
  EXPECT_EQ(InkDropState::ACTIVATED, animator.target_state());
  EXPECT_GT(1.0f, visual_state_.flood_fill_progress);

  callback_called_ = false;
  animation_tester_.IncrementTime(
      InstallableInkDropAnimator::GetSubAnimationDurationForTesting(
          InstallableInkDropAnimator::SubAnimation::kActivatedFloodFill));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::ACTIVATED, animator.target_state());
  EXPECT_EQ(1.0f, visual_state_.flood_fill_progress);
  EXPECT_TRUE(callback_called_);

  // The state should stay the same indefinitely.
  animation_tester_.IncrementTime(
      InstallableInkDropAnimator::GetSubAnimationDurationForTesting(
          InstallableInkDropAnimator::SubAnimation::kDeactivatedFadeOut));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::ACTIVATED, animator.target_state());
  EXPECT_EQ(1.0f, visual_state_.flood_fill_progress);

  // Animating to DEACTIVATED should fade out and transition to HIDDEN.
  animator.AnimateToState(InkDropState::DEACTIVATED);
  EXPECT_EQ(InkDropState::DEACTIVATED, animator.target_state());
  EXPECT_EQ(1.0f, visual_state_.flood_fill_progress);

  callback_called_ = false;
  animation_tester_.IncrementTime(
      InstallableInkDropAnimator::GetSubAnimationDurationForTesting(
          InstallableInkDropAnimator::SubAnimation::kDeactivatedFadeOut));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::HIDDEN, animator.target_state());
  EXPECT_EQ(0.0f, visual_state_.flood_fill_progress);
}

TEST_F(InstallableInkDropAnimatorTest,
       FloodFillAnimationExpandsFromEventLocation) {
  constexpr gfx::Point kEventLocation(3, 7);

  const base::TimeDelta kActivatedAnimationDuration =
      InstallableInkDropAnimator::GetSubAnimationDurationForTesting(
          InstallableInkDropAnimator::SubAnimation::kActivatedFloodFill);
  // Split |kActivatedAnimationDuration| into three chunks.
  const base::TimeDelta kFirstDuration = kActivatedAnimationDuration / 3;
  const base::TimeDelta kSecondDuration = kFirstDuration;
  const base::TimeDelta kLastDuration =
      kActivatedAnimationDuration - kFirstDuration - kSecondDuration;

  InstallableInkDropAnimator animator(gfx::Size(10, 10), &visual_state_,
                                      animation_container_.get(), callback_);

  animator.SetLastEventLocation(kEventLocation);
  animator.AnimateToState(InkDropState::ACTIVATED);

  callback_called_ = false;
  animation_tester_.IncrementTime(kFirstDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_LT(0.0f, visual_state_.flood_fill_progress);
  EXPECT_GT(1.0f, visual_state_.flood_fill_progress);

  const float first_progress = visual_state_.flood_fill_progress;

  callback_called_ = false;
  animation_tester_.IncrementTime(kSecondDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_LT(first_progress, visual_state_.flood_fill_progress);
  EXPECT_GT(1.0f, visual_state_.flood_fill_progress);

  callback_called_ = false;
  animation_tester_.IncrementTime(kLastDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(1.0f, visual_state_.flood_fill_progress);
}

TEST_F(InstallableInkDropAnimatorTest, HighlightAnimationFadesInAndOut) {
  InstallableInkDropAnimator animator(gfx::Size(2, 2), &visual_state_,
                                      animation_container_.get(), callback_);
  EXPECT_EQ(0.0f, visual_state_.highlighted_ratio);
  EXPECT_FALSE(callback_called_);

  animator.AnimateHighlight(true);
  EXPECT_EQ(0.0f, visual_state_.highlighted_ratio);

  callback_called_ = false;
  animation_tester_.IncrementTime(
      InstallableInkDropAnimator::kHighlightAnimationDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1.0f, visual_state_.highlighted_ratio);
  EXPECT_TRUE(callback_called_);

  animator.AnimateHighlight(false);
  EXPECT_EQ(1.0f, visual_state_.highlighted_ratio);

  callback_called_ = false;
  animation_tester_.IncrementTime(
      InstallableInkDropAnimator::kHighlightAnimationDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0.0f, visual_state_.highlighted_ratio);
  EXPECT_TRUE(callback_called_);
}

}  // namespace views
