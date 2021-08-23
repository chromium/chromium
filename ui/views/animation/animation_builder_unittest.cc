// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_builder.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace views {

class TestAnimatibleLayerOwner : public ui::LayerOwner {
 public:
  TestAnimatibleLayerOwner() : ui::LayerOwner(std::make_unique<ui::Layer>()) {
    layer()->GetAnimator()->set_disable_timer_for_test(true);
    layer()->GetAnimator()->SetDelegate(&delegate_);
  }

  ui::LayerAnimationDelegate* delegate() { return &delegate_; }

 private:
  ui::TestLayerAnimationDelegate delegate_;
};

class AnimationBuilderTest : public testing::Test {
 public:
  AnimationBuilderTest() = default;
  TestAnimatibleLayerOwner* CreateTestLayerOwner() {
    layer_owners_.push_back(std::make_unique<TestAnimatibleLayerOwner>());

    animator_controllers_.push_back(
        std::make_unique<ui::LayerAnimatorTestController>(
            layer_owners_.back()->layer()->GetAnimator()));

    return layer_owners_.back().get();
  }

  void Step(const base::TimeDelta& duration) {
    DCHECK_GT(duration, base::TimeDelta());
    for (const auto& controller : animator_controllers_) {
      controller->StartThreadedAnimationsIfNeeded(
          controller->animator()->last_step_time());
      controller->Step(duration);
    }
    elapsed_ += duration;
  }

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    AnimationBuilder::SetObserverDeletedCallbackForTesting(base::BindRepeating(
        [](int* deleted_count) { ++(*deleted_count); }, &deleted_observers_));
  }

  void TearDown() override {
    testing::Test::TearDown();
    // Delete the layer owners and animator controllers here to ensure any
    // lingering animations are aborted and all the observers are destroyed.
    layer_owners_.clear();
    animator_controllers_.clear();
    AnimationBuilder::SetObserverDeletedCallbackForTesting(
        base::NullCallback());
    if (expected_observers_deleted_)
      EXPECT_EQ(expected_observers_deleted_.value(), deleted_observers_);
  }

  // Call this function to also ensure any implicitly created observers have
  // also been properly cleaned up. One observer is created per
  // AnimationSequenceBlock which sets callbacks.
  void set_expected_observers_deleted(int expected_observers_deleted) {
    expected_observers_deleted_ = expected_observers_deleted;
  }

 private:
  std::vector<std::unique_ptr<TestAnimatibleLayerOwner>> layer_owners_;
  std::vector<std::unique_ptr<ui::LayerAnimatorTestController>>
      animator_controllers_;
  base::TimeDelta elapsed_;
  absl::optional<int> expected_observers_deleted_;
  int deleted_observers_ = 0;
};

// This test builds two animation sequences and checks that the properties are
// animated in the specified durations.

TEST_F(AnimationBuilderTest, SimpleAnimation) {
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  TestAnimatibleLayerOwner* second_animating_view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* first_delegate = first_animating_view->delegate();
  ui::LayerAnimationDelegate* second_delegate =
      second_animating_view->delegate();

  gfx::RoundedCornersF rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
  constexpr auto kDelay = base::TimeDelta::FromSeconds(3);

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.4f)
        .SetRoundedCorners(first_animating_view, rounded_corners)
        .Offset(base::TimeDelta())
        .SetDuration(kDelay * 2)
        .SetOpacity(second_animating_view, 0.9f);
  }

  // Original value before the animation steps.
  EXPECT_TRUE(first_animating_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(second_animating_view->layer()->GetAnimator()->is_animating());
  EXPECT_FLOAT_EQ(first_delegate->GetOpacityForAnimation(), 1.0);
  EXPECT_FLOAT_EQ(first_delegate->GetRoundedCornersForAnimation().upper_left(),
                  0.0);
  EXPECT_FLOAT_EQ(second_delegate->GetOpacityForAnimation(), 1.0);

  Step(kDelay);

  EXPECT_FLOAT_EQ(first_delegate->GetOpacityForAnimation(), 0.4f);
  // Sanity check one of the corners.
  EXPECT_FLOAT_EQ(first_delegate->GetRoundedCornersForAnimation().upper_left(),
                  12.0f);
  // This animation should not be finished yet.
  EXPECT_NE(second_delegate->GetOpacityForAnimation(), 0.9f);
  Step(kDelay);
  EXPECT_FLOAT_EQ(second_delegate->GetOpacityForAnimation(), 0.9f);
}

TEST_F(AnimationBuilderTest, CheckTweenType) {
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  gfx::Tween::Type tween_type = gfx::Tween::EASE_IN;
  constexpr auto kDelay = base::TimeDelta::FromSeconds(4);
  // Set initial opacity.
  first_animating_view->delegate()->SetOpacityFromAnimation(
      0.0f, ui::PropertyChangeReason::NOT_FROM_ANIMATION);

  constexpr float opacity_end_val = 0.5f;
  {
    AnimationBuilder().Once().SetDuration(kDelay).SetOpacity(
        first_animating_view, opacity_end_val, tween_type);
  }
  EXPECT_TRUE(first_animating_view->layer()->GetAnimator()->is_animating());
  Step(kDelay / 2);
  // Force an update to the delegate by aborting the animation.
  first_animating_view->layer()->GetAnimator()->AbortAllAnimations();
  // Values at intermediate steps may not be exact.
  EXPECT_NEAR(gfx::Tween::CalculateValue(tween_type, 0.5) * opacity_end_val,
              first_animating_view->delegate()->GetOpacityForAnimation(),
              0.001f);
}

TEST_F(AnimationBuilderTest, CheckStartEndCallbacks) {
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  TestAnimatibleLayerOwner* second_animating_view = CreateTestLayerOwner();

  constexpr auto kDelay = base::TimeDelta::FromSeconds(3);
  bool started = false;
  bool ended = false;

  {
    AnimationBuilder()
        .Once()
        .OnStarted(
            base::BindOnce([](bool* started) { *started = true; }, &started))
        .OnEnded(base::BindOnce([](bool* ended) { *ended = true; }, &ended))
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.4f)
        .Offset(base::TimeDelta())
        .SetDuration(kDelay * 2)
        .SetOpacity(second_animating_view, 0.9f);
  }

  // Only one Observer should have been created in the above block. Make sure
  // it has been cleaned up.
  set_expected_observers_deleted(1);

  EXPECT_TRUE(first_animating_view->layer()->GetAnimator()->is_animating());

  EXPECT_TRUE(started);
  Step(kDelay * 2);
  EXPECT_TRUE(ended);
}

// This test checks that repeat callbacks are called after each sequence
// repetition and callbacks from one sequence do not affect calls from another
// sequence.
TEST_F(AnimationBuilderTest, CheckOnWillRepeatCallbacks) {
  int first_repeat_count = 0;
  int second_repeat_count = 0;

  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  constexpr auto kDelay = base::TimeDelta::FromSeconds(3);
  gfx::RoundedCornersF first_rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
  gfx::RoundedCornersF second_rounded_corners(5.0f, 5.0f, 5.0f, 5.0f);

  {
    AnimationBuilder b;
    b.Repeatedly()
        .SetDuration(kDelay)
        .OnWillRepeat(
            base::BindRepeating([](int& repeat) { repeat = repeat + 1; },
                                std::ref(first_repeat_count)))
        .SetOpacity(first_animating_view, 0.4f)
        .Then()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.9f);

    b.Repeatedly()
        .SetDuration(kDelay)
        .OnWillRepeat(
            base::BindRepeating([](int& repeat) { repeat = repeat + 1; },
                                std::ref(second_repeat_count)))
        .SetRoundedCorners(first_animating_view, first_rounded_corners)
        .Then()
        .SetDuration(kDelay)
        .SetRoundedCorners(first_animating_view, second_rounded_corners);
  }

  set_expected_observers_deleted(2);

  Step(kDelay * 2);
  EXPECT_EQ(first_repeat_count, 1);
  EXPECT_EQ(second_repeat_count, 1);
  Step(kDelay * 2);
  EXPECT_EQ(first_repeat_count, 2);
  EXPECT_EQ(second_repeat_count, 2);
}

// We use these notations to illustrate the tested timeline,
// Pause:    ---|
// KeyFrame: -->|
// Repeat:  [...]
//
// Opacity ---|-->|
TEST_F(AnimationBuilderTest, DelayedStart) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  constexpr auto kDelay = base::TimeDelta::FromSeconds(1);
  constexpr auto kDuration = base::TimeDelta::FromSeconds(1);

  {
    // clang-format off
    AnimationBuilder()
      .Once()
      .At(kDelay)
      .SetDuration(kDuration)
      .SetOpacity(view, 0.4f);
    // clang-format on
  }

  Step(kDelay);
  // The animation on opacity is not yet started.
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 1.0);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
}

// Opacity -->|-->|
TEST_F(AnimationBuilderTest, TwoKeyFrame) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  constexpr auto kDuration = base::TimeDelta::FromSeconds(1);

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.4f)
        .Then()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.9f);
  }

  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
}

// Opacity -->|---|-->|
TEST_F(AnimationBuilderTest, PauseInTheMiddle) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  constexpr auto kDuration = base::TimeDelta::FromSeconds(1);

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.4f)
        .Then()
        .Offset(kDuration)
        .SetDuration(kDuration)
        .SetOpacity(view, 0.9f);
  }

  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
}

// Opacity        -->|
// RoundedCorners ----->|
TEST_F(AnimationBuilderTest, TwoPropertiesOfDifferentDuration) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  gfx::RoundedCornersF rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
  // Make sure that the opacity keyframe finishes at the middle of the rounded
  // corners keyframe.
  constexpr auto kDurationShort = base::TimeDelta::FromSeconds(1);
  constexpr auto kDurationLong = kDurationShort * 2;

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDurationShort)
        .SetOpacity(view, 0.4f)
        .At(base::TimeDelta())
        .SetDuration(kDurationLong)
        .SetRoundedCorners(view, rounded_corners);
  }

  Step(kDurationShort);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 6.0f);
  Step(kDurationLong - kDurationShort);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(),
                  12.0f);
}

// Opacity        ----->|
// RoundedCorners    ----->|
TEST_F(AnimationBuilderTest, TwoPropertiesOfDifferentStartTime) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  gfx::RoundedCornersF rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
  // Make sure that the opacity keyframe finishes at the middle of the rounded
  // corners keyframe.
  constexpr auto kDelay = base::TimeDelta::FromSeconds(1);
  constexpr auto kDuration = kDelay * 2;

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.4f)
        .At(kDelay)
        .SetDuration(kDuration)
        .SetRoundedCorners(view, rounded_corners);
  }

  Step(kDelay);
  // Unfortunately, we can't test threaded animations in the midst of a frame
  // because they don't update LayerAnimationDelegate in OnProgress().
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 0.0);
  Step(kDuration - kDelay);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 6.0f);
  Step(kDelay);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(),
                  12.0f);
}

// Opacity        ----->|---|-->|
// RoundedCorners     ----->|-->|
TEST_F(AnimationBuilderTest, ThenAddsImplicitPause) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  gfx::RoundedCornersF rounded_corners1(12.0f, 12.0f, 12.0f, 12.0f);
  gfx::RoundedCornersF rounded_corners2(5.0f, 5.0f, 5.0f, 5.0f);
  // Make sure that the first opacity keyframe finishes at the middle of the
  // first rounded corners keyframe.
  constexpr auto kDelay = base::TimeDelta::FromSeconds(1);
  constexpr auto kDuration = kDelay * 2;

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.4f)
        .At(kDelay)
        .SetDuration(kDuration)
        .SetRoundedCorners(view, rounded_corners1)
        .Then()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.9f)
        .SetRoundedCorners(view, rounded_corners2);
  }

  Step(kDelay);
  // Unfortunately, we can't test threaded animations in the midst of a frame
  // because they don't update LayerAnimationDelegate in OnProgress().
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 0.0);
  Step(kDuration - kDelay);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 6.0f);
  Step(kDelay);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(),
                  12.0f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 5.0f);
}

// Opacity [-->|-->]
TEST_F(AnimationBuilderTest, Repeat) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  constexpr auto kDuration = base::TimeDelta::FromSeconds(1);

  {
    AnimationBuilder()
        .Repeatedly()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.4f)
        .Then()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.9f);
  }

  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
}

// Opacity [-->|-->|   ]
TEST_F(AnimationBuilderTest, RepeatWithExplicitTrailingPause) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  constexpr auto kDuration = base::TimeDelta::FromSeconds(1);

  {
    AnimationBuilder()
        .Repeatedly()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.4f)
        .Then()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.9f)
        .Then()
        .SetDuration(kDuration);
  }

  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
}

// Opacity        [-->|-->]
// RoundedCorners [-->|-->]
TEST_F(AnimationBuilderTest, RepeatTwoProperties) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  gfx::RoundedCornersF rounded_corners1(12.0f, 12.0f, 12.0f, 12.0f);
  gfx::RoundedCornersF rounded_corners2(5.0f, 5.0f, 5.0f, 5.0f);
  constexpr auto kDuration = base::TimeDelta::FromSeconds(1);

  {
    AnimationBuilder()
        .Repeatedly()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.4f)
        .SetRoundedCorners(view, rounded_corners1)
        .Then()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.9f)
        .SetRoundedCorners(view, rounded_corners2);
  }

  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 12.0);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 5.0);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 12.0);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 5.0);
}

// Opacity        -->|-->|
// RoundedCorners   -->|-->|
TEST_F(AnimationBuilderTest, AtCanSkipThenBlock) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  gfx::RoundedCornersF rounded_corners1(12.0f, 12.0f, 12.0f, 12.0f);
  gfx::RoundedCornersF rounded_corners2(4.0f, 4.0f, 4.0f, 4.0f);
  // Make sure that the first opacity keyframe finishes at the middle of the
  // first rounded corners keyframe.
  constexpr auto kDelay = base::TimeDelta::FromSeconds(1);
  constexpr auto kDuration = kDelay * 2;

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.4f)
        .Then()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.9f)
        .At(kDelay)
        .SetDuration(kDuration)
        .SetRoundedCorners(view, rounded_corners1)
        .Then()
        .SetDuration(kDuration)
        .SetRoundedCorners(view, rounded_corners2);
  }

  Step(kDelay);
  // Unfortunately, we can't test threaded animations in the midst of a frame
  // because they don't update LayerAnimationDelegate in OnProgress().
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 0.0);
  Step(kDuration - kDelay);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 6.0);
  Step(kDelay);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 12.0);
  Step(kDuration - kDelay);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 8.0);
  Step(kDelay);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 4.0);
}

// Opacity        -->|-->|
// RoundedCorners   -->|
TEST_F(AnimationBuilderTest, OffsetCanRewindTime) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  gfx::RoundedCornersF rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
  // Make sure that the first opacity keyframe finishes at the middle of the
  // first rounded corners keyframe.
  constexpr auto kDelay = base::TimeDelta::FromSeconds(1);
  constexpr auto kDuration = kDelay * 2;

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.4f)
        .Then()
        .SetDuration(kDuration)
        .SetOpacity(view, 0.9f)
        .Offset(kDelay - kDuration)
        .SetDuration(kDuration)
        .SetRoundedCorners(view, rounded_corners);
  }

  Step(kDelay);
  // Unfortunately, we can't test threaded animations in the midst of a frame
  // because they don't update LayerAnimationDelegate in OnProgress().
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 0.0);
  Step(kDuration - kDelay);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 6.0);
  Step(kDelay);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 12.0);
  Step(kDuration - kDelay);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 12.0);
}

// Opacity        [-->|-->  ]
// RoundedCorners [-->|---->]
TEST_F(AnimationBuilderTest, RepeatedlyImplicitlyAppendsTrailingPause) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  gfx::RoundedCornersF rounded_corners1(12.0f, 12.0f, 12.0f, 12.0f);
  gfx::RoundedCornersF rounded_corners2(4.0f, 4.0f, 4.0f, 4.0f);
  // Make sure that the second opacity keyframe finishes at the middle of the
  // second rounded corners keyframe.
  constexpr auto kDurationShort = base::TimeDelta::FromSeconds(1);
  constexpr auto kDurationLong = kDurationShort * 2;

  {
    AnimationBuilder()
        .Repeatedly()
        .SetDuration(kDurationShort)
        .SetOpacity(view, 0.4f)
        .SetRoundedCorners(view, rounded_corners1)
        .Then()
        .SetDuration(kDurationShort)
        .SetOpacity(view, 0.9f)
        .Offset(base::TimeDelta())
        .SetDuration(kDurationLong)
        .SetRoundedCorners(view, rounded_corners2);
  }

  Step(kDurationShort);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 12.0);
  Step(kDurationShort);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 8.0);
  Step(kDurationLong - kDurationShort);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 4.0);
  // Repeat
  Step(kDurationShort);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 12.0);
  Step(kDurationShort);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 8.0);
  Step(kDurationLong - kDurationShort);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
  EXPECT_FLOAT_EQ(delegate->GetRoundedCornersForAnimation().upper_left(), 4.0);
}

}  // namespace views
