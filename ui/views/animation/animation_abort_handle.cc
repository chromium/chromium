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

  if (observer_)
    observer_->SetAbortHandle(nullptr);

  if (animation_state_ != AnimationState::kEnded) {
    for (ui::Layer* layer : tracked_layers_) {
      if (deleted_layers_.find(layer) != deleted_layers_.end())
        continue;

      layer->GetAnimator()->AbortAllAnimations();
    }
  }

  // Remove the abort handle itself from the alive tracked layers.
  for (ui::Layer* layer : tracked_layers_) {
    if (deleted_layers_.find(layer) != deleted_layers_.end())
      continue;
    layer->RemoveObserver(this);
  }
}

void AnimationAbortHandle::OnObserverDeleted() {
  observer_ = nullptr;
}

void AnimationAbortHandle::AddLayer(ui::Layer* layer) {
  // Do not allow to add the layer that was deleted before.
  DCHECK(deleted_layers_.find(layer) == deleted_layers_.end());

  bool inserted = tracked_layers_.insert(layer).second;

  // In case that one layer is added to the abort handle multiple times.
  if (inserted)
    layer->AddObserver(this);
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
  layer->RemoveObserver(this);

  // NOTE: layer deletion may be caused by animation abortion. In addition,
  // aborting an animation may lead to multiple layer deletions (for example, a
  // animation abort callback could delete multiple views' layers). Therefore
  // the destroyed layer should not be removed from `tracked_layers_` directly.
  // Otherwise, iterating `tracked_layers_` in the animation abort handle's
  // destructor is risky.
  bool inserted = deleted_layers_.insert(layer).second;
  DCHECK(inserted);
}

}  // namespace views
