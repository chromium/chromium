// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_MULTI_LAYER_ANIMATOR_TEST_CONTROLLER_DELEGATE_H_
#define UI_COMPOSITOR_TEST_MULTI_LAYER_ANIMATOR_TEST_CONTROLLER_DELEGATE_H_

#include <vector>

namespace ui {
class LayerAnimator;

namespace test {

class MultiLayerAnimatorTestControllerDelegate {
 public:
  MultiLayerAnimatorTestControllerDelegate(
      const MultiLayerAnimatorTestControllerDelegate&) = delete;
  MultiLayerAnimatorTestControllerDelegate& operator=(
      const MultiLayerAnimatorTestControllerDelegate&) = delete;

  // Get a list of all the LayerAnimator's used by the animation.
  virtual std::vector<ui::LayerAnimator*> GetLayerAnimators() = 0;

 protected:
  MultiLayerAnimatorTestControllerDelegate() {}
  virtual ~MultiLayerAnimatorTestControllerDelegate() {}
};

}  // namespace test
}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_MULTI_LAYER_ANIMATOR_TEST_CONTROLLER_DELEGATE_H_
