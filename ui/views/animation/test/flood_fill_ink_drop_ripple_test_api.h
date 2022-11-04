// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_FLOOD_FILL_INK_DROP_RIPPLE_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_FLOOD_FILL_INK_DROP_RIPPLE_TEST_API_H_

#include <vector>

#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/test/ink_drop_ripple_test_api.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views::test {

// Test API to provide internal access to a FloodFillInkDropRipple.
class FloodFillInkDropRippleTestApi : public InkDropRippleTestApi {
 public:
  explicit FloodFillInkDropRippleTestApi(
      FloodFillInkDropRipple* ink_drop_ripple);

  FloodFillInkDropRippleTestApi(const FloodFillInkDropRippleTestApi&) = delete;
  FloodFillInkDropRippleTestApi& operator=(
      const FloodFillInkDropRippleTestApi&) = delete;

  ~FloodFillInkDropRippleTestApi() override;

  // Transforms |point| into the FloodFillInkDropRipples clip layer coordinate
  // space for the given radius.
  gfx::Point3F MapPoint(float radius, const gfx::Point3F& point);

  // Returns the center point that the ripple is drawn at in the original Canvas
  // coordinate space.
  gfx::Point GetDrawnCenterPoint() const;

  // Wrapper for FloodFillInkDropRipple::MaxDistanceToCorners().
  float MaxDistanceToCorners(const gfx::Point& point) const;

  // Gets the transform currently applied to the painted layer of the ripple.
  gfx::Transform GetPaintedLayerTransform() const;

  // InkDropRippleTestApi:
  float GetCurrentOpacity() const override;

 protected:
  // InkDropRippleTestApi:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 private:
  FloodFillInkDropRipple* ink_drop_ripple() {
    return static_cast<const FloodFillInkDropRippleTestApi*>(this)
        ->ink_drop_ripple();
  }

  FloodFillInkDropRipple* ink_drop_ripple() const {
    return static_cast<FloodFillInkDropRipple*>(
        InkDropRippleTestApi::ink_drop_ripple());
  }
};

}  // namespace views::test

#endif  // UI_VIEWS_ANIMATION_TEST_FLOOD_FILL_INK_DROP_RIPPLE_TEST_API_H_
