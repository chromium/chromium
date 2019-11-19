// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/flood_fill_ink_drop_ripple_test_api.h"

#include <vector>

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/views/animation/ink_drop_ripple.h"

namespace views {
namespace test {

FloodFillInkDropRippleTestApi::FloodFillInkDropRippleTestApi(
    FloodFillInkDropRipple* ink_drop_ripple)
    : InkDropRippleTestApi(ink_drop_ripple) {}

FloodFillInkDropRippleTestApi::~FloodFillInkDropRippleTestApi() = default;

void FloodFillInkDropRippleTestApi::TransformPoint(float radius,
                                                   gfx::Point3F* point) {
  ink_drop_ripple()->CalculateTransform(radius).TransformPoint(point);
}

gfx::Point FloodFillInkDropRippleTestApi::GetDrawnCenterPoint() const {
  return ToRoundedPoint(PointAtOffsetFromOrigin(
      ink_drop_ripple()->circle_layer_delegate_.GetCenteringOffset()));
}

float FloodFillInkDropRippleTestApi::MaxDistanceToCorners(
    const gfx::Point& point) const {
  return ink_drop_ripple()->MaxDistanceToCorners(point);
}

gfx::Transform FloodFillInkDropRippleTestApi::GetPaintedLayerTransform() const {
  return ink_drop_ripple()->painted_layer_.transform();
}

float FloodFillInkDropRippleTestApi::GetCurrentOpacity() const {
  return ink_drop_ripple()->root_layer_.opacity();
}

std::vector<ui::LayerAnimator*>
FloodFillInkDropRippleTestApi::GetLayerAnimators() {
  std::vector<ui::LayerAnimator*> animators =
      InkDropRippleTestApi::GetLayerAnimators();
  animators.push_back(ink_drop_ripple()->GetRootLayer()->GetAnimator());
  animators.push_back(ink_drop_ripple()->painted_layer_.GetAnimator());
  return animators;
}

}  // namespace test
}  // namespace views
