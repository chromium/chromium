// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_MULTI_LAYER_ANIMATOR_TEST_CONTROLLER_DELEGATE_H_
#define UI_COMPOSITOR_TEST_MULTI_LAYER_ANIMATOR_TEST_CONTROLLER_DELEGATE_H_

#include <vector>

#include "base/macros.h"

namespace ui {
class LayerAnimator;

namespace test {

class MultiLayerAnimatorTestControllerDelegate {
 public:
  // Get a list of all the LayerAnimator's used by the animation.
  virtual std::vector<ui::LayerAnimator*> GetLayerAnimators() = 0;

 protected:
  MultiLayerAnimatorTestControllerDelegate() {}
  virtual ~MultiLayerAnimatorTestControllerDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiLayerAnimatorTestControllerDelegate);
};

}  // namespace test
}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_MULTI_LAYER_ANIMATOR_TEST_CONTROLLER_DELEGATE_H_
