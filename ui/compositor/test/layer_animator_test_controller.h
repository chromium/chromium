// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_LAYER_ANIMATOR_TEST_CONTROLLER_H_
#define UI_COMPOSITOR_TEST_LAYER_ANIMATOR_TEST_CONTROLLER_H_

#include "ui/compositor/layer_animator.h"

namespace ui {

// Allows tests to access sequences owned by the animator.
class LayerAnimatorTestController {
 public:
  LayerAnimatorTestController(scoped_refptr<LayerAnimator> animator);

  ~LayerAnimatorTestController();

  LayerAnimator* animator() { return animator_.get(); }

  // Returns the running sequence animating the given property, if any.
  LayerAnimationSequence* GetRunningSequence(
      LayerAnimationElement::AnimatableProperty property);

  // Starts threaded animations that are waiting for an effective start time.
  void StartThreadedAnimationsIfNeeded(
      base::TimeTicks started_time = base::TimeTicks::Now());

  // Progresses all running animations by the given |duration|.
  void Step(const base::TimeDelta& duration);

 private:
  scoped_refptr<LayerAnimator> animator_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_LAYER_ANIMATOR_TEST_CONTROLLER_H_
