// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_HIGHLIGHT_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_HIGHLIGHT_TEST_API_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "ui/compositor/test/multi_layer_animator_test_controller.h"
#include "ui/compositor/test/multi_layer_animator_test_controller_delegate.h"
#include "ui/gfx/geometry/transform.h"

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

  InkDropHighlightTestApi(const InkDropHighlightTestApi&) = delete;
  InkDropHighlightTestApi& operator=(const InkDropHighlightTestApi&) = delete;

  ~InkDropHighlightTestApi() override;

  // MultiLayerAnimatorTestControllerDelegate:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

  gfx::Transform CalculateTransform();

 private:
  // The InkDropHighlight to provide internal access to.
  const raw_ref<InkDropHighlight> ink_drop_highlight_;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_HIGHLIGHT_TEST_API_H_
