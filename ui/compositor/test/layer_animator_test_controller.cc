// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>

#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

LayerAnimatorTestController::LayerAnimatorTestController(
    scoped_refptr<LayerAnimator> animator)
    : animator_(std::move(animator)) {}

LayerAnimatorTestController::~LayerAnimatorTestController() {
}

LayerAnimationSequence* LayerAnimatorTestController::GetRunningSequence(
    LayerAnimationElement::AnimatableProperty property) {
  LayerAnimator::RunningAnimation* running_animation =
      animator_->GetRunningAnimation(property);
  if (running_animation)
    return running_animation->sequence();
  else
    return NULL;
}

void LayerAnimatorTestController::StartThreadedAnimationsIfNeeded(
    base::TimeTicks started_time) {
  std::vector<cc::TargetProperty::Type> threaded_properties;
  threaded_properties.push_back(cc::TargetProperty::OPACITY);
  threaded_properties.push_back(cc::TargetProperty::TRANSFORM);

  for (size_t i = 0; i < threaded_properties.size(); i++) {
    LayerAnimationElement::AnimatableProperty animatable_property =
        LayerAnimationElement::ToAnimatableProperty(threaded_properties[i]);
    LayerAnimationSequence* sequence = GetRunningSequence(animatable_property);
    if (!sequence)
      continue;

    LayerAnimationElement* element = sequence->CurrentElement();
    if (!(element->properties() & animatable_property))
      continue;

    if (!element->Started() ||
        element->effective_start_time() != base::TimeTicks())
      continue;

    animator_->OnThreadedAnimationStarted(started_time, threaded_properties[i],
                                          element->animation_group_id());
  }
}

void LayerAnimatorTestController::Step(const base::TimeDelta& duration) {
  animator_->Step(animator_->last_step_time() + duration);
}

}  // namespace ui
