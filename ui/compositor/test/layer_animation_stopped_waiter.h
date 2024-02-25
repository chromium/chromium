// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_LAYER_ANIMATION_STOPPED_WAITER_H_
#define UI_COMPOSITOR_TEST_LAYER_ANIMATION_STOPPED_WAITER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"

namespace base {
class RunLoop;
}

namespace ui {

class Layer;
class LayerAnimator;

// A class capable of waiting until a layer has stopped animating. Supports
// animations that delete the layer on completion.
class LayerAnimationStoppedWaiter : public LayerAnimationObserver {
 public:
  LayerAnimationStoppedWaiter();
  LayerAnimationStoppedWaiter(const LayerAnimationStoppedWaiter&) = delete;
  LayerAnimationStoppedWaiter& operator=(const LayerAnimationStoppedWaiter&) =
      delete;
  ~LayerAnimationStoppedWaiter() override;

  // Waits until the specified `layer`'s animation is stopped.
  void Wait(Layer* layer);

 private:
  // LayerAnimationObserver:
  void OnLayerAnimationScheduled(LayerAnimationSequence* sequence) override {}

  void OnLayerAnimationStarted(LayerAnimationSequence* sequence) override {}

  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override;

  void OnLayerAnimationEnded(LayerAnimationSequence* sequence) override;

  raw_ptr<LayerAnimator, AcrossTasksDanglingUntriaged> layer_animator_ =
      nullptr;
  base::ScopedObservation<LayerAnimator, LayerAnimationObserver>
      layer_animator_observer_{this};
  std::unique_ptr<base::RunLoop> wait_loop_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_LAYER_ANIMATION_STOPPED_WAITER_H_
