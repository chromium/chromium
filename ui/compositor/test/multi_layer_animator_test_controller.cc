// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/multi_layer_animator_test_controller.h"

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/test/multi_layer_animator_test_controller_delegate.h"

namespace ui {
namespace test {

MultiLayerAnimatorTestController::MultiLayerAnimatorTestController(
    MultiLayerAnimatorTestControllerDelegate* delegate)
    : delegate_(delegate) {}

MultiLayerAnimatorTestController::~MultiLayerAnimatorTestController() {}

void MultiLayerAnimatorTestController::SetDisableAnimationTimers(
    bool disable_timers) {
  for (LayerAnimator* animator : GetLayerAnimators())
    animator->set_disable_timer_for_test(disable_timers);
}

bool MultiLayerAnimatorTestController::HasActiveAnimations() const {
  for (LayerAnimator* animator : GetLayerAnimators()) {
    if (animator->is_animating())
      return true;
  }
  return false;
}

void MultiLayerAnimatorTestController::CompleteAnimations() {
  while (HasActiveAnimations()) {
    // StepAnimations() will only progress the current running animations. Thus
    // each queued animation will require at least one 'Step' call and we cannot
    // just use a large duration here.
    StepAnimations(base::Milliseconds(20));
  }
}

std::vector<LayerAnimator*>
MultiLayerAnimatorTestController::GetLayerAnimators() {
  return static_cast<const MultiLayerAnimatorTestController*>(this)
      ->GetLayerAnimators();
}

std::vector<LayerAnimator*>
MultiLayerAnimatorTestController::GetLayerAnimators() const {
  return delegate_->GetLayerAnimators();
}

void MultiLayerAnimatorTestController::StepAnimations(
    const base::TimeDelta& duration) {
  for (ui::LayerAnimator* animator : GetLayerAnimators()) {
    LayerAnimatorTestController controller(animator);
    controller.StartThreadedAnimationsIfNeeded();
    controller.Step(duration);
  }
}

}  // namespace test
}  // namespace ui
