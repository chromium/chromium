// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_MULTI_LAYER_ANIMATOR_TEST_CONTROLLER_H_
#define UI_COMPOSITOR_TEST_MULTI_LAYER_ANIMATOR_TEST_CONTROLLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace ui {
class LayerAnimator;

namespace test {
class MultiLayerAnimatorTestControllerDelegate;

// Test API class to control multiple LayerAnimators.
class MultiLayerAnimatorTestController {
 public:
  explicit MultiLayerAnimatorTestController(
      MultiLayerAnimatorTestControllerDelegate* delegate);

  MultiLayerAnimatorTestController(const MultiLayerAnimatorTestController&) =
      delete;
  MultiLayerAnimatorTestController& operator=(
      const MultiLayerAnimatorTestController&) = delete;

  virtual ~MultiLayerAnimatorTestController();

  // Disables the animation timers when |disable_timers| is true.
  void SetDisableAnimationTimers(bool disable_timers);

  // Returns true if any animations are active.
  bool HasActiveAnimations() const;

  // Completes all running animations.
  void CompleteAnimations();

 private:
  // Get a list of all the LayerAnimator's used.  Delegates to |delegate_|.
  std::vector<LayerAnimator*> GetLayerAnimators();
  std::vector<LayerAnimator*> GetLayerAnimators() const;

  // Progresses all running LayerAnimationSequences by the given |duration|.
  //
  // NOTE: This function will NOT progress LayerAnimationSequences that are
  // queued, only the running ones will be progressed.
  void StepAnimations(const base::TimeDelta& duration);

  raw_ptr<MultiLayerAnimatorTestControllerDelegate> delegate_;
};

}  // namespace test
}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_MULTI_LAYER_ANIMATOR_TEST_CONTROLLER_H_
