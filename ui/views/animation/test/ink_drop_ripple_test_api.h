// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_RIPPLE_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_RIPPLE_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/compositor/test/multi_layer_animator_test_controller.h"
#include "ui/compositor/test/multi_layer_animator_test_controller_delegate.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
class InkDropRipple;

namespace test {

// Test API to provide internal access to an InkDropRipple instance. This can
// also be used to control the animations via the
// ui::test::MultiLayerAnimatorTestController API.
class InkDropRippleTestApi
    : public ui::test::MultiLayerAnimatorTestController,
      public ui::test::MultiLayerAnimatorTestControllerDelegate {
 public:
  explicit InkDropRippleTestApi(InkDropRipple* ink_drop_ripple);
  ~InkDropRippleTestApi() override;

  // Gets the opacity of the ink drop.
  virtual float GetCurrentOpacity() const = 0;

  // MultiLayerAnimatorTestControllerDelegate:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 protected:
  InkDropRipple* ink_drop_ripple() {
    return static_cast<const InkDropRippleTestApi*>(this)->ink_drop_ripple();
  }

  InkDropRipple* ink_drop_ripple() const { return ink_drop_ripple_; }

 private:
  // The InkDropedRipple to provide internal access to.
  InkDropRipple* ink_drop_ripple_;

  DISALLOW_COPY_AND_ASSIGN(InkDropRippleTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_RIPPLE_TEST_API_H_
