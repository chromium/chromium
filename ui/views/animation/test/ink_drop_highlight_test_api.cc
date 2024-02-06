// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_highlight_test_api.h"

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/views/animation/ink_drop_highlight.h"

namespace views::test {

InkDropHighlightTestApi::InkDropHighlightTestApi(
    InkDropHighlight* ink_drop_highlight)
    : ui::test::MultiLayerAnimatorTestController(this),
      ink_drop_highlight_(*ink_drop_highlight) {}

InkDropHighlightTestApi::~InkDropHighlightTestApi() = default;

std::vector<ui::LayerAnimator*> InkDropHighlightTestApi::GetLayerAnimators() {
  std::vector<ui::LayerAnimator*> animators;
  animators.push_back(ink_drop_highlight_->layer_->GetAnimator());
  return animators;
}

gfx::Transform InkDropHighlightTestApi::CalculateTransform() {
  return ink_drop_highlight_->CalculateTransform();
}

}  // namespace views::test
