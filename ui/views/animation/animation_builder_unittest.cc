// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_builder.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace views {

class AnimationBuilderTest : public ViewsTestBase {
 public:
  AnimationBuilderTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    // Animation durations are set to zero in ViewsTestBase::SetUp. Set to
    // normal duration so we can test animation progression.
    animation_duration_scale_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  }

 private:
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode>
      animation_duration_scale_;
};

// This test builds two animation sequences and checks that the properties are
// animated in the specified durations.

TEST_F(AnimationBuilderTest, SimpleAnimation) {
  auto first_delegate = std::make_unique<ui::TestLayerAnimationDelegate>();
  auto second_delegate = std::make_unique<ui::TestLayerAnimationDelegate>();
  auto first_animating_view = std::make_unique<View>();
  auto second_animating_view = std::make_unique<View>();

  // Animation Builder will paint to layer automatically, but since we need to
  // access the layer, paint first.
  first_animating_view->SetPaintToLayer();
  second_animating_view->SetPaintToLayer();

  ui::LayerAnimator* first_layer_animator =
      first_animating_view->layer()->GetAnimator();
  first_layer_animator->set_disable_timer_for_test(true);
  first_layer_animator->SetDelegate(first_delegate.get());

  ui::LayerAnimator* second_layer_animator =
      second_animating_view->layer()->GetAnimator();
  second_layer_animator->set_disable_timer_for_test(true);
  second_layer_animator->SetDelegate(second_delegate.get());

  ui::LayerAnimatorTestController first_test_controller(first_layer_animator);
  ui::LayerAnimatorTestController second_test_controller(second_layer_animator);

  gfx::RoundedCornersF rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
  constexpr auto kDelay = base::TimeDelta::FromSeconds(3);

  {
    AnimationBuilder b;
    b.Once()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view.get(), 0.4f)
        .SetRoundedCorners(first_animating_view.get(), rounded_corners)
        .Offset(base::TimeDelta())
        .SetDuration(kDelay * 2)
        .SetOpacity(second_animating_view.get(), 0.9f);
  }

  // Original value before the animation steps.
  EXPECT_TRUE(first_layer_animator->is_animating());
  EXPECT_TRUE(second_layer_animator->is_animating());
  EXPECT_FLOAT_EQ(first_delegate->GetOpacityForAnimation(), 1.0);
  EXPECT_FLOAT_EQ(first_delegate->GetRoundedCornersForAnimation().upper_left(),
                  0.0);
  EXPECT_FLOAT_EQ(second_delegate->GetOpacityForAnimation(), 1.0);

  first_test_controller.StartThreadedAnimationsIfNeeded();
  second_test_controller.StartThreadedAnimationsIfNeeded();
  first_animating_view->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                                     kDelay);
  second_animating_view->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                                      kDelay);
  EXPECT_FLOAT_EQ(first_delegate->GetOpacityForAnimation(), 0.4f);
  // Sanity check one of the corners.
  EXPECT_FLOAT_EQ(first_delegate->GetRoundedCornersForAnimation().upper_left(),
                  12.0f);
  // This animation should not be finished yet. The specific value can be tested
  // more extensively after tween support is added.
  EXPECT_NE(second_delegate->GetOpacityForAnimation(), 0.9f);
  base::TimeTicks last_step_time = second_layer_animator->last_step_time();
  second_animating_view->layer()->GetAnimator()->Step(last_step_time + kDelay);
  EXPECT_FLOAT_EQ(second_delegate->GetOpacityForAnimation(), 0.9f);
}

TEST_F(AnimationBuilderTest, CheckStartEndCallbacks) {
  auto first_animating_view = std::make_unique<View>();
  auto second_animating_view = std::make_unique<View>();

  first_animating_view->SetPaintToLayer();
  second_animating_view->SetPaintToLayer();

  ui::LayerAnimator* first_layer_animator =
      first_animating_view->layer()->GetAnimator();
  // TODO(kylixrd): Consider adding more test support to AnimationBuilder to
  // avoid reaching behind the curtain for these things.
  first_layer_animator->set_disable_timer_for_test(true);

  ui::LayerAnimator* second_layer_animator =
      second_animating_view->layer()->GetAnimator();
  second_layer_animator->set_disable_timer_for_test(true);

  ui::LayerAnimatorTestController first_test_controller(first_layer_animator);
  ui::LayerAnimatorTestController second_test_controller(second_layer_animator);

  constexpr auto kDelay = base::TimeDelta::FromSeconds(3);
  bool started = false;
  bool ended = false;

  {
    AnimationBuilder b;
    b.OnStarted(
         base::BindOnce([](bool* started) { *started = true; }, &started))
        .OnEnded(base::BindOnce([](bool* ended) { *ended = true; }, &ended))
        .Once()
        .SetDuration(kDelay)
        .SetOpacity(first_animating_view.get(), 0.4f)
        .Offset(base::TimeDelta())
        .SetDuration(kDelay * 2)
        .SetOpacity(second_animating_view.get(), 0.9f);
  }

  first_test_controller.StartThreadedAnimationsIfNeeded();
  second_test_controller.StartThreadedAnimationsIfNeeded();

  EXPECT_TRUE(started);

  first_animating_view->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                                     kDelay * 2);
  second_animating_view->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                                      kDelay * 2);

  EXPECT_TRUE(ended);
}

}  // namespace views
