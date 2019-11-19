// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/square_ink_drop_ripple_test_api.h"

#include <vector>

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/views/animation/ink_drop_ripple.h"

namespace views {
namespace test {

SquareInkDropRippleTestApi::SquareInkDropRippleTestApi(
    SquareInkDropRipple* ink_drop_ripple)
    : InkDropRippleTestApi(ink_drop_ripple) {}

SquareInkDropRippleTestApi::~SquareInkDropRippleTestApi() = default;

void SquareInkDropRippleTestApi::CalculateCircleTransforms(
    const gfx::Size& size,
    InkDropTransforms* transforms_out) const {
  ink_drop_ripple()->CalculateCircleTransforms(size, transforms_out);
}
void SquareInkDropRippleTestApi::CalculateRectTransforms(
    const gfx::Size& size,
    float corner_radius,
    InkDropTransforms* transforms_out) const {
  ink_drop_ripple()->CalculateRectTransforms(size, corner_radius,
                                             transforms_out);
}

float SquareInkDropRippleTestApi::GetCurrentOpacity() const {
  return ink_drop_ripple()->GetCurrentOpacity();
}

std::vector<ui::LayerAnimator*>
SquareInkDropRippleTestApi::GetLayerAnimators() {
  std::vector<ui::LayerAnimator*> animators =
      InkDropRippleTestApi::GetLayerAnimators();
  animators.push_back(ink_drop_ripple()->GetRootLayer()->GetAnimator());
  for (auto& painted_layer : ink_drop_ripple()->painted_layers_)
    animators.push_back(painted_layer->GetAnimator());
  return animators;
}

}  // namespace test
}  // namespace views
