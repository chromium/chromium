// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_builder.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/views/animation/animation_abort_handle.h"

namespace views {

namespace {

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

// Configures the layer animation on `layer_owner` and returns the builder.
AnimationBuilder BuildLayerOpacityAnimationAndReturnBuilder(
    ui::LayerOwner* layer_owner,
    const base::TimeDelta& duration) {
  EXPECT_NE(0.f, layer_owner->layer()->opacity());
  AnimationBuilder builder;
  builder.Once().SetDuration(duration).SetOpacity(layer_owner, 0.f);
  return builder;
}

}  // namespace

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
  std::optional<int> expected_observers_deleted_;
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
  constexpr auto kDelay = base::Seconds(3);

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

// This test checks that after setting the animation duration scale to be larger
// than 1, animations behave as expected of that scale.
TEST_F(AnimationBuilderTest, ModifiedSlowAnimationDuration) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  TestAnimatibleLayerOwner* second_animating_view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* first_delegate = first_animating_view->delegate();
  ui::LayerAnimationDelegate* second_delegate =
      second_animating_view->delegate();

  gfx::RoundedCornersF rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
  constexpr auto kDelay = base::Seconds(3);

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.4f)
        .SetRoundedCorners(first_animating_view, rounded_corners)
        .Offset(base::TimeDelta())
        .SetDuration(kDelay * 2)
        .SetOpacity(second_animating_view, 0.9f)
        .Then()
        .SetDuration(kDelay)
        .Then()
        .SetDuration(kDelay)
        .SetOpacity(second_animating_view, 0.4f);
  }

  Step(kDelay * ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  EXPECT_FLOAT_EQ(first_delegate->GetOpacityForAnimation(), 0.4f);
  // Sanity check one of the corners.
  EXPECT_FLOAT_EQ(first_delegate->GetRoundedCornersForAnimation().upper_left(),
                  12.0f);
  // This animation should not be finished yet.
  EXPECT_NE(second_delegate->GetOpacityForAnimation(), 0.9f);

  Step(kDelay * ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  EXPECT_FLOAT_EQ(second_delegate->GetOpacityForAnimation(), 0.9f);

  Step(kDelay * 2 * ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  EXPECT_FLOAT_EQ(second_delegate->GetOpacityForAnimation(), 0.4f);
}

// This test checks that after setting the animation duration scale to be
// between 0 and 1, animations behave as expected of that scale.
TEST_F(AnimationBuilderTest, ModifiedFastAnimationDuration) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  TestAnimatibleLayerOwner* second_animating_view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* first_delegate = first_animating_view->delegate();
  ui::LayerAnimationDelegate* second_delegate =
      second_animating_view->delegate();

  gfx::RoundedCornersF rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
  constexpr auto kDelay = base::Seconds(3);

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.4f)
        .SetRoundedCorners(first_animating_view, rounded_corners)
        .Offset(base::TimeDelta())
        .SetDuration(kDelay * 2)
        .SetOpacity(second_animating_view, 0.9f)
        .Then()
        .SetDuration(kDelay)
        .Then()
        .SetDuration(kDelay)
        .SetOpacity(second_animating_view, 0.4f);
  }

  Step(kDelay * ui::ScopedAnimationDurationScaleMode::FAST_DURATION);
  EXPECT_FLOAT_EQ(first_delegate->GetOpacityForAnimation(), 0.4f);
  // Sanity check one of the corners.
  EXPECT_FLOAT_EQ(first_delegate->GetRoundedCornersForAnimation().upper_left(),
                  12.0f);
  // This animation should not be finished yet.
  EXPECT_NE(second_delegate->GetOpacityForAnimation(), 0.9f);

  Step(kDelay * ui::ScopedAnimationDurationScaleMode::FAST_DURATION);
  EXPECT_FLOAT_EQ(second_delegate->GetOpacityForAnimation(), 0.9f);

  Step(kDelay * 2 * ui::ScopedAnimationDurationScaleMode::FAST_DURATION);
  EXPECT_FLOAT_EQ(second_delegate->GetOpacityForAnimation(), 0.4f);
}

// This test checks that after setting the animation duration scale to be 0,
// animations behave as expected of that scale.
TEST_F(AnimationBuilderTest, ModifiedZeroAnimationDuration) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  TestAnimatibleLayerOwner* second_animating_view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* first_delegate = first_animating_view->delegate();
  ui::LayerAnimationDelegate* second_delegate =
      second_animating_view->delegate();

  gfx::RoundedCornersF rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
  constexpr auto kDelay = base::Seconds(3);

  {
    AnimationBuilder()
        .Once()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.4f)
        .SetRoundedCorners(first_animating_view, rounded_corners)
        .Offset(base::TimeDelta())
        .SetDuration(kDelay * 2)
        .SetOpacity(second_animating_view, 0.9f)
        .Then()
        .SetDuration(kDelay)
        .Then()
        .SetDuration(kDelay)
        .SetOpacity(second_animating_view, 0.4f);
  }

  EXPECT_FLOAT_EQ(first_delegate->GetOpacityForAnimation(), 0.4f);
  // Sanity check one of the corners.
  EXPECT_FLOAT_EQ(first_delegate->GetRoundedCornersForAnimation().upper_left(),
                  12.0f);
  EXPECT_FLOAT_EQ(second_delegate->GetOpacityForAnimation(), 0.4f);
}

// This test checks that the callback supplied to .OnEnded is not called before
// all sequences have finished running. This test will crash if .OnEnded is
// called prematurely.
TEST_F(AnimationBuilderTest, ModifiedZeroAnimationDurationWithOnEndedCallback) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto first_animating_view = std::make_unique<TestAnimatibleLayerOwner>();
  auto second_animating_view = std::make_unique<TestAnimatibleLayerOwner>();

  views::AnimationBuilder b;
  b.OnEnded(base::BindRepeating(
                [](TestAnimatibleLayerOwner* layer_owner,
                   TestAnimatibleLayerOwner* second_layer_owner) {
                  delete layer_owner;
                  delete second_layer_owner;
                },
                first_animating_view.get(), second_animating_view.get()))
      .Once()
      .SetDuration(base::Seconds(3))
      .SetOpacity(first_animating_view.get(), 0.4f)
      .SetOpacity(second_animating_view.get(), 0.9f);

  first_animating_view.release();
  second_animating_view.release();
}

TEST_F(AnimationBuilderTest, ZeroDurationBlock) {
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* first_delegate = first_animating_view->delegate();

  gfx::RoundedCornersF first_corners(6.0f, 6.0f, 6.0f, 6.0f);
  gfx::RoundedCornersF second_corners(12.0f, 12.0f, 12.0f, 12.0f);

  constexpr auto kDelay = base::Seconds(3);

  {
    AnimationBuilder()
        .Once()
        .SetDuration(base::TimeDelta())
        .SetRoundedCorners(first_animating_view, first_corners)
        .Then()
        .SetDuration(kDelay)
        .SetRoundedCorners(first_animating_view, second_corners)
        .Then()
        .SetDuration(base::TimeDelta())
        .SetRoundedCorners(first_animating_view, first_corners);
  }

  EXPECT_FLOAT_EQ(first_delegate->GetRoundedCornersForAnimation().upper_left(),
                  6.0f);
  Step(kDelay / 2);
  EXPECT_FLOAT_EQ(first_delegate->GetRoundedCornersForAnimation().upper_left(),
                  9.0f);
  Step(kDelay / 2);
  EXPECT_FLOAT_EQ(first_delegate->GetRoundedCornersForAnimation().upper_left(),
                  6.0f);
}

TEST_F(AnimationBuilderTest, CheckTweenType) {
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  gfx::Tween::Type tween_type = gfx::Tween::EASE_IN;
  constexpr auto kDelay = base::Seconds(4);
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

// Verify that destroying the layers tracked by the animation abort handle
// before the animation ends should not cause any crash.
TEST_F(AnimationBuilderTest, DestroyLayerBeforeAnimationEnd) {
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  TestAnimatibleLayerOwner* second_animating_view = CreateTestLayerOwner();

  std::unique_ptr<AnimationAbortHandle> abort_handle;
  {
    AnimationBuilder builder;
    abort_handle = builder.GetAbortHandle();
    builder.Once()
        .SetDuration(base::Seconds(3))
        .SetOpacity(first_animating_view, 0.5f)
        .SetOpacity(second_animating_view, 0.5f);
  }

  EXPECT_TRUE(first_animating_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(second_animating_view->layer()->GetAnimator()->is_animating());
  first_animating_view->ReleaseLayer();
  second_animating_view->ReleaseLayer();
}

// Verify that destroying layers tracked by the animation abort handle when
// the animation ends should not cause any crash.
TEST_F(AnimationBuilderTest, DestroyLayerWhenAnimationEnd) {
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  TestAnimatibleLayerOwner* second_animating_view = CreateTestLayerOwner();

  auto end_callback = [](TestAnimatibleLayerOwner* first_animating_view,
                         TestAnimatibleLayerOwner* second_animating_view) {
    first_animating_view->ReleaseLayer();
    second_animating_view->ReleaseLayer();
  };

  constexpr auto kDelay = base::Seconds(3);
  std::unique_ptr<AnimationAbortHandle> abort_handle;
  {
    AnimationBuilder builder;
    abort_handle = builder.GetAbortHandle();
    builder
        .OnEnded(base::BindOnce(end_callback, first_animating_view,
                                second_animating_view))
        .Once()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.5f)
        .SetOpacity(second_animating_view, 0.5f);
  }

  EXPECT_TRUE(first_animating_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(second_animating_view->layer()->GetAnimator()->is_animating());

  Step(kDelay * 2);

  // Verify that layers are destroyed when the animation ends.
  EXPECT_FALSE(first_animating_view->layer());
  EXPECT_FALSE(second_animating_view->layer());
}

// Verify that destroying layers tracked by the animation abort handle when
// the animation is aborted should not cause any crash.
TEST_F(AnimationBuilderTest, DestroyLayerWhenAnimationAborted) {
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  TestAnimatibleLayerOwner* second_animating_view = CreateTestLayerOwner();

  auto abort_callback = [](TestAnimatibleLayerOwner* first_animating_view,
                           TestAnimatibleLayerOwner* second_animating_view) {
    first_animating_view->ReleaseLayer();
    second_animating_view->ReleaseLayer();
  };

  constexpr auto kDelay = base::Seconds(3);
  std::unique_ptr<AnimationAbortHandle> abort_handle;
  {
    AnimationBuilder builder;
    abort_handle = builder.GetAbortHandle();
    builder
        .OnAborted(base::BindOnce(abort_callback, first_animating_view,
                                  second_animating_view))
        .Once()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.5f)
        .SetOpacity(second_animating_view, 0.5f);
  }

  Step(0.5 * kDelay);
  EXPECT_TRUE(first_animating_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(second_animating_view->layer()->GetAnimator()->is_animating());

  // Abort the animation in the half way.
  first_animating_view->layer()->GetAnimator()->AbortAllAnimations();

  // Verify that layers are destroyed by the animation abortion callback.
  EXPECT_FALSE(first_animating_view->layer());
  EXPECT_FALSE(second_animating_view->layer());
}

TEST_F(AnimationBuilderTest, CheckStartEndCallbacks) {
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  TestAnimatibleLayerOwner* second_animating_view = CreateTestLayerOwner();

  constexpr auto kDelay = base::Seconds(3);
  bool started = false;
  bool ended = false;

  {
    AnimationBuilder()
        .OnStarted(
            base::BindOnce([](bool* started) { *started = true; }, &started))
        .OnEnded(base::BindOnce([](bool* ended) { *ended = true; }, &ended))
        .Once()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.4f)
        .Offset(base::TimeDelta())
        .SetDuration(kDelay * 2)
        .SetOpacity(second_animating_view, 0.9f)
        .Then()
        .SetDuration(kDelay)
        .SetOpacity(second_animating_view, 0.4f);
  }

  // Only one Observer should have been created in the above block. Make sure
  // it has been cleaned up.
  set_expected_observers_deleted(1);

  EXPECT_TRUE(first_animating_view->layer()->GetAnimator()->is_animating());

  EXPECT_TRUE(started);
  Step(kDelay * 2);
  EXPECT_FALSE(ended);
  Step(kDelay);
  EXPECT_TRUE(ended);
}

// This test checks that repeat callbacks are called after each sequence
// repetition and callbacks from one sequence do not affect calls from another
// sequence.
TEST_F(AnimationBuilderTest, CheckOnWillRepeatCallbacks) {
  int first_repeat_count = 0;
  int second_repeat_count = 0;

  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  constexpr auto kDelay = base::Seconds(3);
  gfx::RoundedCornersF first_rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
  gfx::RoundedCornersF second_rounded_corners(5.0f, 5.0f, 5.0f, 5.0f);

  {
    AnimationBuilder b;
    b.OnWillRepeat(base::BindRepeating([](int& repeat) { repeat = repeat + 1; },
                                       std::ref(first_repeat_count)))
        .Repeatedly()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.4f)
        .Then()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view, 0.9f);

    b.OnWillRepeat(base::BindRepeating([](int& repeat) { repeat = repeat + 1; },
                                       std::ref(second_repeat_count)))
        .Repeatedly()
        .SetDuration(kDelay)
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

  constexpr auto kDelay = base::Seconds(1);
  constexpr auto kDuration = base::Seconds(1);

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

  constexpr auto kDuration = base::Seconds(1);

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

  constexpr auto kDuration = base::Seconds(1);

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
  constexpr auto kDurationShort = base::Seconds(1);
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
  constexpr auto kDelay = base::Seconds(1);
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
  constexpr auto kDelay = base::Seconds(1);
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

  constexpr auto kDuration = base::Seconds(1);

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

  constexpr auto kDuration = base::Seconds(1);

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
  constexpr auto kDuration = base::Seconds(1);

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
  constexpr auto kDelay = base::Seconds(1);
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
  constexpr auto kDelay = base::Seconds(1);
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
  constexpr auto kDurationShort = base::Seconds(1);
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

// Opacity -->|-->|--> with a loop for setting these blocks.
TEST_F(AnimationBuilderTest, RepeatedBlocks) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  constexpr auto kDuration = base::Seconds(1);
  constexpr float kOpacity[] = {0.4f, 0.9f, 0.6f};

  {
    AnimationBuilder builder;
    builder.Repeatedly();
    for (const auto& opacity : kOpacity) {
      builder.GetCurrentSequence()
          .SetDuration(kDuration)
          .SetOpacity(view, opacity)
          .Then();
    }
  }

  for (const auto& opacity : kOpacity) {
    Step(kDuration);
    EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), opacity);
  }
}

TEST_F(AnimationBuilderTest, PreemptionStrategyTest) {
  using ps = ui::LayerAnimator::PreemptionStrategy;
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();

  constexpr auto kStepSize = base::Seconds(1);
  constexpr auto kDuration = base::Seconds(5);

  // Set the initial value to animate.
  delegate->SetBrightnessFromAnimation(
      1.0f, ui::PropertyChangeReason::NOT_FROM_ANIMATION);

  {
    AnimationBuilder().Once().SetDuration(kDuration).SetBrightness(view, 0.0f);
  }

  // The animation hasn't started.
  EXPECT_FLOAT_EQ(delegate->GetBrightnessForAnimation(), 1.0f);
  // Step the animation, but don't complete it.
  Step(kStepSize);
  // Make sure the animation is progressing.
  EXPECT_FLOAT_EQ(delegate->GetBrightnessForAnimation(), 0.8f);
  // Make sure we're still animatiing.
  EXPECT_TRUE(view->layer()->GetAnimator()->is_animating());

  // Now start a new animation to a different target.
  {
    AnimationBuilder()
        .SetPreemptionStrategy(ps::IMMEDIATELY_SET_NEW_TARGET)
        .Once()
        .SetDuration(
            kStepSize)  // We only moved previous animation by kStepSize
        .SetBrightness(view, 1.0f);
  }

  Step(kStepSize);
  // The above animation should have been aborted, and set the brightness to the
  // new target immediately.
  EXPECT_FLOAT_EQ(delegate->GetBrightnessForAnimation(), 1.0f);
  EXPECT_FALSE(view->layer()->GetAnimator()->is_animating());

  // Start another animation which we'll preemtp to test another strategy.
  {
    AnimationBuilder().Once().SetDuration(kDuration).SetBrightness(view, 0.0f);
  }

  // This should start out like the one above.
  EXPECT_FLOAT_EQ(delegate->GetBrightnessForAnimation(), 1.0f);
  Step(kStepSize);
  EXPECT_FLOAT_EQ(delegate->GetBrightnessForAnimation(), 0.8f);
  EXPECT_TRUE(view->layer()->GetAnimator()->is_animating());

  {
    AnimationBuilder()
        .SetPreemptionStrategy(ps::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetDuration(kStepSize)
        .SetBrightness(view, 1.0f);
  }

  // The new animation should pick up where the last one left off.
  EXPECT_FLOAT_EQ(delegate->GetBrightnessForAnimation(), 0.8f);
  Step(kStepSize);
  // The new animation is in force if it steps toward the new target.
  EXPECT_FLOAT_EQ(delegate->GetBrightnessForAnimation(), 1.0f);
  // Make sure the animation is fully complete.
  Step(kStepSize);
  // The animation should be done now.
  EXPECT_FALSE(view->layer()->GetAnimator()->is_animating());
}

TEST_F(AnimationBuilderTest, AbortHandle) {
  TestAnimatibleLayerOwner* view = CreateTestLayerOwner();
  ui::LayerAnimationDelegate* delegate = view->delegate();
  std::unique_ptr<AnimationAbortHandle> abort_handle;

  constexpr auto kStepSize = base::Seconds(1);
  constexpr auto kDuration = kStepSize * 2;

  {
    delegate->SetBrightnessFromAnimation(
        1.0f, ui::PropertyChangeReason::NOT_FROM_ANIMATION);
    delegate->SetOpacityFromAnimation(
        1.0f, ui::PropertyChangeReason::NOT_FROM_ANIMATION);

    {
      AnimationBuilder b;
      abort_handle = b.GetAbortHandle();
      b.Once()
          .SetDuration(kDuration)
          .SetOpacity(view, 0.4f)
          .At(base::TimeDelta())
          .SetDuration(kDuration)
          .SetBrightness(view, 0.4f);
    }

    Step(kStepSize);
    // Destroy abort handle should stop all animations.
    abort_handle.reset();
    EXPECT_FLOAT_EQ(delegate->GetBrightnessForAnimation(), 0.7f);
    EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.7f);
    Step(kStepSize);
    EXPECT_FLOAT_EQ(delegate->GetBrightnessForAnimation(), 0.7f);
    EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.7f);
  }

  // The builder crashes if the handle is destroyed before animation starts.
  {
    delegate->SetBrightnessFromAnimation(
        1.0f, ui::PropertyChangeReason::NOT_FROM_ANIMATION);
    delegate->SetOpacityFromAnimation(
        1.0f, ui::PropertyChangeReason::NOT_FROM_ANIMATION);

    {
      AnimationBuilder b;
      abort_handle = b.GetAbortHandle();
      b.Once()
          .SetDuration(kDuration)
          .SetOpacity(view, 0.4f)
          .At(base::TimeDelta())
          .SetDuration(kDuration)
          .SetBrightness(view, 0.4f);

      // Early destroy should crash the builder.
      EXPECT_DCHECK_DEATH(abort_handle.reset());
    }
  }

  // The handle shouldn't abort animations subsequent to the builder.
  {
    delegate->SetBrightnessFromAnimation(
        1.0f, ui::PropertyChangeReason::NOT_FROM_ANIMATION);
    delegate->SetOpacityFromAnimation(
        1.0f, ui::PropertyChangeReason::NOT_FROM_ANIMATION);

    {
      AnimationBuilder b;
      abort_handle = b.GetAbortHandle();
      b.Once()
          .SetDuration(kDuration)
          .SetOpacity(view, 0.4f)
          .At(base::TimeDelta())
          .SetDuration(kDuration)
          .SetBrightness(view, 0.4f);
    }

    EXPECT_EQ(abort_handle->animation_state(),
              AnimationAbortHandle::AnimationState::kRunning);
    // Step to the end of the animation.
    Step(kDuration);
    EXPECT_EQ(abort_handle->animation_state(),
              AnimationAbortHandle::AnimationState::kEnded);

    {
      AnimationBuilder b;
      b.Once()
          .SetDuration(kDuration)
          .SetOpacity(view, 0.8f)
          .At(base::TimeDelta())
          .SetDuration(kDuration)
          .SetBrightness(view, 0.8f);
    }

    // Destroy the handle on the finihsed animation shouldn't affect other
    // unfinihsed animations.
    abort_handle.reset();
    Step(kDuration);
    EXPECT_FLOAT_EQ(delegate->GetBrightnessForAnimation(), 0.8f);
    EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.8f);
  }
}

// Verifies that configuring layer animations with an animation builder returned
// from a function works as expected.
TEST_F(AnimationBuilderTest, BuildAnimationWithBuilderFromScope) {
  TestAnimatibleLayerOwner* first_animating_view = CreateTestLayerOwner();
  TestAnimatibleLayerOwner* second_animating_view = CreateTestLayerOwner();
  EXPECT_EQ(1.f, first_animating_view->layer()->opacity());
  EXPECT_EQ(1.f, second_animating_view->layer()->opacity());

  constexpr auto kDuration = base::Seconds(3);
  {
    // Build a layer animation on `second_animating_view` with a builder
    // returned from a function.
    AnimationBuilder builder = BuildLayerOpacityAnimationAndReturnBuilder(
        first_animating_view, kDuration);
    builder.GetCurrentSequence().SetOpacity(second_animating_view, 0.f);
  }

  // Verify that both views are under animation.
  EXPECT_TRUE(first_animating_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(second_animating_view->layer()->GetAnimator()->is_animating());

  Step(kDuration);

  // Verify that after `kDuration` time, both layer animations end. In addition,
  // both layers are set with the target opacity.
  EXPECT_FALSE(first_animating_view->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(second_animating_view->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(0.f, first_animating_view->delegate()->GetOpacityForAnimation());
  EXPECT_EQ(0.f, second_animating_view->delegate()->GetOpacityForAnimation());
}

// Verifies that it is disallowed to animate transform using both
// `SetInterpolatedTransform()` and `SetTransform()` in the same block on the
// same target.
TEST_F(AnimationBuilderTest,
       DisallowMultipleSameBlockSameTargetTransformPropertyAnimations) {
  TestAnimatibleLayerOwner* target = CreateTestLayerOwner();
  EXPECT_DCHECK_DEATH_WITH(
      AnimationBuilder()
          .Once()
          .SetInterpolatedTransform(
              target, std::make_unique<ui::InterpolatedMatrixTransform>(
                          /*start_transform=*/gfx::Transform(),
                          /*end_transform=*/gfx::Transform()))
          .SetTransform(target, gfx::Transform()),
      "Animate \\(target, property\\) at most once per block.");
}

// Verifies that transform can be animated using `SetInterpolatedTransform()`.
TEST_F(AnimationBuilderTest, SetInterpolatedTransform) {
  // Create a nested transform. Note the use of irregular `start_time` and
  // `end_time` to verify that out of bounds values are handled.
  auto transform = std::make_unique<ui::InterpolatedScale>(
      /*start_scale=*/0.f, /*end_scale=*/1.f, /*start_time=*/-1.f,
      /*end_time=*/2.f);
  transform->SetChild(std::make_unique<ui::InterpolatedRotation>(
      /*start_degrees=*/0.f, /*end_degrees=*/360.f, /*start_time=*/0.5f,
      /*end_time=*/0.75f));

  // Cache expected transforms at key animation points.
  const gfx::Transform expected_start_transform = transform->Interpolate(0.f);
  const gfx::Transform expected_mid_transform = transform->Interpolate(0.5f);
  const gfx::Transform expected_end_transform = transform->Interpolate(1.f);

  // Verify initial state.
  TestAnimatibleLayerOwner* target = CreateTestLayerOwner();
  EXPECT_EQ(target->delegate()->GetTransformForAnimation(), gfx::Transform());

  // Start animation.
  constexpr auto kDuration = base::Seconds(2);
  AnimationBuilder().Once().SetDuration(kDuration).SetInterpolatedTransform(
      target, std::move(transform));

  // Verify state at animation start.
  EXPECT_TRUE(target->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(target->delegate()->GetTransformForAnimation(),
            expected_start_transform);

  // Verify state at animation midpoint.
  Step(kDuration / 2);
  EXPECT_TRUE(target->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(target->delegate()->GetTransformForAnimation(),
            expected_mid_transform);

  // Verify state at animation end.
  Step(kDuration / 2);
  EXPECT_FALSE(target->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(target->delegate()->GetTransformForAnimation(),
            expected_end_transform);
}

}  // namespace views
