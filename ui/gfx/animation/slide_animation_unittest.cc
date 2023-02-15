// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/slide_animation.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/animation/test_animation_delegate.h"

namespace gfx {

////////////////////////////////////////////////////////////////////////////////
// SlideAnimationTest
class SlideAnimationTest : public testing::Test {
 public:
  void RunAnimationFor(base::TimeDelta duration) {
    base::TimeTicks now = base::TimeTicks::Now();
    animation_api_->SetStartTime(now);
    animation_api_->Step(now + duration);
  }

  SlideAnimation* slide_animation() { return slide_animation_.get(); }

 protected:
  SlideAnimationTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        slide_animation_(std::make_unique<SlideAnimation>(nullptr)),
        animation_api_(
            std::make_unique<AnimationTestApi>(slide_animation_.get())) {}

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  const std::unique_ptr<SlideAnimation> slide_animation_;
  const std::unique_ptr<AnimationTestApi> animation_api_;
};

// Tests animation construction.
TEST_F(SlideAnimationTest, InitialState) {
  // By default, slide animations are 60 Hz, so the timer interval should be
  // 1/60th of a second.
  EXPECT_EQ(1000 / 60, slide_animation()->timer_interval().InMilliseconds());
  // Duration defaults to 120 ms.
  EXPECT_EQ(120, slide_animation()->GetSlideDuration().InMilliseconds());
  // Slide is neither showing nor closing.
  EXPECT_FALSE(slide_animation()->IsShowing());
  EXPECT_FALSE(slide_animation()->IsClosing());
  // Starts at 0.
  EXPECT_EQ(0.0, slide_animation()->GetCurrentValue());
}

TEST_F(SlideAnimationTest, Basics) {
  // Use linear tweening to make the math easier below.
  slide_animation()->SetTweenType(Tween::LINEAR);

  // Duration can be set after construction.
  slide_animation()->SetSlideDuration(base::Milliseconds(100));
  EXPECT_EQ(100, slide_animation()->GetSlideDuration().InMilliseconds());

  // Show toggles the appropriate state.
  slide_animation()->Show();
  EXPECT_TRUE(slide_animation()->IsShowing());
  EXPECT_FALSE(slide_animation()->IsClosing());

  // Simulate running the animation.
  RunAnimationFor(base::Milliseconds(50));
  EXPECT_EQ(0.5, slide_animation()->GetCurrentValue());

  // We can start hiding mid-way through the animation.
  slide_animation()->Hide();
  EXPECT_FALSE(slide_animation()->IsShowing());
  EXPECT_TRUE(slide_animation()->IsClosing());

  // Reset stops the animation.
  slide_animation()->Reset();
  EXPECT_EQ(0.0, slide_animation()->GetCurrentValue());
  EXPECT_FALSE(slide_animation()->IsShowing());
  EXPECT_FALSE(slide_animation()->IsClosing());
}

// Tests that delegate is not notified when animation is running and is deleted.
// (Such a scenario would cause problems for BoundsAnimator).
TEST_F(SlideAnimationTest, DontNotifyOnDelete) {
  TestAnimationDelegate delegate;
  std::unique_ptr<SlideAnimation> animation(new SlideAnimation(&delegate));

  // Start the animation.
  animation->Show();

  // Delete the animation.
  animation.reset();

  // Make sure the delegate wasn't notified.
  EXPECT_FALSE(delegate.finished());
  EXPECT_FALSE(delegate.canceled());
}

// Tests that animations which are started partway and have a dampening factor
// of 1 progress linearly.
TEST_F(SlideAnimationTest,
       AnimationWithPartialProgressAndDefaultDampeningFactor) {
  slide_animation()->SetTweenType(Tween::LINEAR);
  slide_animation()->SetSlideDuration(base::Milliseconds(100));
  slide_animation()->Show();
  EXPECT_EQ(slide_animation()->GetCurrentValue(), 0.0);

  // Advance the animation to halfway done.
  RunAnimationFor(base::Milliseconds(50));
  EXPECT_EQ(0.5, slide_animation()->GetCurrentValue());

  // Reverse the animation and run it for half of the remaining duration.
  slide_animation()->Hide();
  RunAnimationFor(base::Milliseconds(25));
  EXPECT_EQ(0.25, slide_animation()->GetCurrentValue());

  // Reverse the animation again and run it for half of the remaining duration.
  slide_animation()->Show();
  RunAnimationFor(base::Milliseconds(37.5));
  EXPECT_EQ(0.625, slide_animation()->GetCurrentValue());
}

// Tests that animations which are started partway and have a dampening factor
// of >1 progress sub-leanearly.
TEST_F(SlideAnimationTest,
       AnimationWithPartialProgressAndNonDefaultDampeningFactor) {
  slide_animation()->SetTweenType(Tween::LINEAR);
  slide_animation()->SetDampeningValue(2.0);
  slide_animation()->SetSlideDuration(base::Milliseconds(100));
  slide_animation()->Show();
  // Advance the animation to halfway done.
  RunAnimationFor(base::Milliseconds(50));
  EXPECT_EQ(0.5, slide_animation()->GetCurrentValue());

  // Reverse the animation and run it for the same duration, it should be
  // sub-linear with dampening.
  slide_animation()->Hide();
  RunAnimationFor(base::Milliseconds(50));
  EXPECT_GT(slide_animation()->GetCurrentValue(), 0);
}

// Tests that a mostly complete dampened animation takes a sub-linear
// amount of time to complete.
TEST_F(SlideAnimationTest, DampenedAnimationMostlyComplete) {
  slide_animation()->SetTweenType(Tween::LINEAR);
  slide_animation()->SetDampeningValue(2.0);
  slide_animation()->SetSlideDuration(base::Milliseconds(100));
  slide_animation()->Show();
  // Advance the animation to 1/10th of the way done.
  RunAnimationFor(base::Milliseconds(10));
  EXPECT_EQ(0.1, slide_animation()->GetCurrentValue());

  // Reverse the animation and run it for 1/10th of the duration, it should not
  // be complete.
  slide_animation()->Hide();
  RunAnimationFor(base::Milliseconds(10));
  EXPECT_GT(slide_animation()->GetCurrentValue(), 0);

  // Finish the animation and set up the test for a mostly complete show
  // animation.
  RunAnimationFor(base::Milliseconds(100));
  EXPECT_EQ(0, slide_animation()->GetCurrentValue());
  slide_animation()->Show();
  // Advance the animation to 9/10th of the way done.
  RunAnimationFor(base::Milliseconds(90));
  EXPECT_EQ(0.9, slide_animation()->GetCurrentValue());

  // Hide and then Show the animation to force the duration to be recalculated,
  // then show for 1/10th of the duration and test that the animation is not
  // complete.
  slide_animation()->Hide();
  slide_animation()->Show();
  RunAnimationFor(base::Milliseconds(10));
  EXPECT_LT(slide_animation()->GetCurrentValue(), 1);

  RunAnimationFor(base::Milliseconds(40));
  EXPECT_EQ(1, slide_animation()->GetCurrentValue());
}

// Tests that a mostly incomplete dampened animation takes a sub-linear amount
// of time to complete.
TEST_F(SlideAnimationTest, DampenedAnimationMostlyIncomplete) {
  slide_animation()->SetTweenType(Tween::LINEAR);
  slide_animation()->SetDampeningValue(2.0);
  slide_animation()->SetSlideDuration(base::Milliseconds(100));
  slide_animation()->Show();
  // Advance the animation to 1/10th of the way done.
  RunAnimationFor(base::Milliseconds(10));
  EXPECT_EQ(0.1, slide_animation()->GetCurrentValue());

  // Hide and then Show the animation to force the duration to be recalculated,
  // then show for 9/10th of the duration and test that the animation is not
  // complete.
  slide_animation()->Hide();
  slide_animation()->Show();
  RunAnimationFor(base::Milliseconds(90));
  EXPECT_LT(slide_animation()->GetCurrentValue(), 1);

  // Finish the animation and set up the test for a mostly incomplete hide
  // animation.
  RunAnimationFor(base::Milliseconds(100));
  EXPECT_EQ(1, slide_animation()->GetCurrentValue());
  slide_animation()->Hide();
  RunAnimationFor(base::Milliseconds(10));
  EXPECT_EQ(0.9, slide_animation()->GetCurrentValue());

  // Show and then hide the animation to recompute the duration, then run the
  // animation for 9/10ths of the duration and test that the animation is not
  // complete.
  slide_animation()->Show();
  slide_animation()->Hide();
  RunAnimationFor(base::Milliseconds(90));
  EXPECT_GT(slide_animation()->GetCurrentValue(), 0);

  RunAnimationFor(base::Milliseconds(100));
  EXPECT_EQ(0, slide_animation()->GetCurrentValue());
}

TEST_F(SlideAnimationTest, HideFromHalfway) {
  auto container = base::MakeRefCounted<AnimationContainer>();
  AnimationContainerTestApi test_api(container.get());

  slide_animation()->SetContainer(container.get());
  slide_animation()->SetTweenType(Tween::LINEAR);
  slide_animation()->SetSlideDuration(base::Milliseconds(100));

  slide_animation()->Reset(0.5);
  EXPECT_FALSE(slide_animation()->is_animating());
  EXPECT_EQ(0.5, slide_animation()->GetCurrentValue());

  slide_animation()->Hide();
  ASSERT_TRUE(slide_animation()->is_animating());

  test_api.IncrementTime(base::Milliseconds(100));
  EXPECT_EQ(0.0, slide_animation()->GetCurrentValue());
}

}  // namespace gfx
