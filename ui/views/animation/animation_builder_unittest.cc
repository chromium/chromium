// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_builder.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
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

 private:
  std::vector<std::unique_ptr<TestAnimatibleLayerOwner>> layer_owners_;
  std::vector<std::unique_ptr<ui::LayerAnimatorTestController>>
      animator_controllers_;
  base::TimeDelta elapsed_;
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

  EXPECT_TRUE(first_animating_view->layer()->GetAnimator()->is_animating());

  EXPECT_TRUE(started);
  Step(kDelay * 2);
  EXPECT_TRUE(ended);
}

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

  // Original value before the animation steps.
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 1.0);
  Step(kDelay);
  // The animation on opacity is not yet started.
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 1.0);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
}

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

  // The animation on opacity is not yet started.
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 1.0);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.4f);
  Step(kDuration);
  EXPECT_FLOAT_EQ(delegate->GetOpacityForAnimation(), 0.9f);
}

}  // namespace views
