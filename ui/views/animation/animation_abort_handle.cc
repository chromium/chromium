// Copyright 2021 The Chromium Authors. All rights reserved.
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

  if (observer_)
    observer_->SetAbortHandle(nullptr);

  if (animation_state_ != AnimationState::kEnded) {
    for (ui::Layer* layer : layers_)
      layer->GetAnimator()->AbortAllAnimations();
  }
}

void AnimationAbortHandle::OnObserverDeleted() {
  observer_ = nullptr;
}

void AnimationAbortHandle::AddLayer(ui::Layer* layer) {
  layers_.insert(layer);
}

void AnimationAbortHandle::OnAnimationStarted() {
  DCHECK_EQ(animation_state_, AnimationState::kNotStarted);
  animation_state_ = AnimationState::kRunning;
}

void AnimationAbortHandle::OnAnimationEnded() {
  DCHECK_EQ(animation_state_, AnimationState::kRunning);
  animation_state_ = AnimationState::kEnded;
}

}  // namespace views
