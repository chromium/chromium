// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_HIGHLIGHT_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_HIGHLIGHT_TEST_API_H_

#include <vector>

#include "base/macros.h"
#include "ui/compositor/test/multi_layer_animator_test_controller.h"
#include "ui/compositor/test/multi_layer_animator_test_controller_delegate.h"
#include "ui/gfx/transform.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
class InkDropHighlight;

namespace test {

// Test API to provide internal access to an InkDropHighlight instance. This can
// also be used to control the animations via the
// ui::test::MultiLayerAnimatorTestController API.
class InkDropHighlightTestApi
    : public ui::test::MultiLayerAnimatorTestController,
      public ui::test::MultiLayerAnimatorTestControllerDelegate {
 public:
  explicit InkDropHighlightTestApi(InkDropHighlight* ink_drop_highlight);
  ~InkDropHighlightTestApi() override;

  // MultiLayerAnimatorTestControllerDelegate:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

  gfx::Transform CalculateTransform();

 protected:
  InkDropHighlight* ink_drop_highlight() {
    return static_cast<const InkDropHighlightTestApi*>(this)
        ->ink_drop_highlight();
  }

  InkDropHighlight* ink_drop_highlight() const { return ink_drop_highlight_; }

 private:
  // The InkDropHighlight to provide internal access to.
  InkDropHighlight* ink_drop_highlight_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHighlightTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_HIGHLIGHT_TEST_API_H_
