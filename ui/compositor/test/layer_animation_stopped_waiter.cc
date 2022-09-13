// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/layer_animation_stopped_waiter.h"

#include "base/run_loop.h"
#include "ui/compositor/layer.h"

namespace ui {

LayerAnimationStoppedWaiter::LayerAnimationStoppedWaiter() = default;

LayerAnimationStoppedWaiter::~LayerAnimationStoppedWaiter() = default;

void LayerAnimationStoppedWaiter::Wait(Layer* layer) {
  if (!layer->GetAnimator()->is_animating())
    return;

  // Temporarily cache and observe `layer`'s animator.
  layer_animator_ = layer->GetAnimator();
  layer_animator_observer_.Observe(layer_animator_);

  // Loop until the `layer`'s animation is stopped.
  wait_loop_ = std::make_unique<base::RunLoop>();
  wait_loop_->Run();

  // Reset.
  layer_animator_ = nullptr;
  wait_loop_.reset();
}

void LayerAnimationStoppedWaiter::OnLayerAnimationAborted(
    LayerAnimationSequence* sequence) {
  if (!layer_animator_->is_animating()) {
    layer_animator_observer_.Reset();
    wait_loop_->Quit();
  }
}

void LayerAnimationStoppedWaiter::OnLayerAnimationEnded(
    LayerAnimationSequence* sequence) {
  if (!layer_animator_->is_animating()) {
    layer_animator_observer_.Reset();
    wait_loop_->Quit();
  }
}

}  // namespace ui
