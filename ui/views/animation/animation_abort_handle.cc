// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_abort_handle.h"

#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"

namespace views {

AnimationAbortHandle::AnimationAbortHandle(AnimationBuilder::Observer* observer)
    : observer_(observer) {
  observer_->SetAbortHandle(this);
}

AnimationAbortHandle::~AnimationAbortHandle() {
  DCHECK_NE(animation_state_, AnimationState::kNotStarted)
      << "You can't destroy the handle before the animation starts.";

  if (observer_) {
    observer_->SetAbortHandle(nullptr);
  }

  if (animation_state_ != AnimationState::kEnded) {
    while (layer_observations_.IsObservingAnySource()) {
      ui::Layer* const layer = layer_observations_.sources().begin()->get();
      layer_observations_.RemoveObservation(layer);

      layer->GetAnimator()->AbortAllAnimations();
    }
  }

  // `layer_observations_` removes `this` from every layer that is still being
  // observed when it is destroyed.
}

void AnimationAbortHandle::OnObserverDeleted() {
  observer_ = nullptr;
}

void AnimationAbortHandle::AddLayer(ui::Layer* layer) {
  // In case that one layer is added to the abort handle multiple times.
  if (!layer_observations_.IsObservingSource(layer)) {
    layer_observations_.AddObservation(layer);
  }
}

void AnimationAbortHandle::OnAnimationStarted() {
  DCHECK_EQ(animation_state_, AnimationState::kNotStarted);
  animation_state_ = AnimationState::kRunning;
}

void AnimationAbortHandle::OnAnimationEnded() {
  DCHECK_EQ(animation_state_, AnimationState::kRunning);
  animation_state_ = AnimationState::kEnded;
}

void AnimationAbortHandle::LayerDestroyed(ui::Layer* layer) {
  // Stop observing `layer` before it goes away: `layer_observations_` holds a
  // raw_ptr to each observed source and must not outlive it.
  layer_observations_.RemoveObservation(layer);
}

}  // namespace views
