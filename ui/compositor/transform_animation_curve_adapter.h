// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TRANSFORM_ANIMATION_CURVE_ADAPTER_H_
#define UI_COMPOSITOR_TRANSFORM_ANIMATION_CURVE_ADAPTER_H_

#include <memory>

#include "base/time/time.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_operations.h"

namespace ui {

class COMPOSITOR_EXPORT TransformAnimationCurveAdapter
    : public gfx::TransformAnimationCurve {
 public:
  TransformAnimationCurveAdapter(gfx::Tween::Type tween_type,
                                 gfx::Transform intial_value,
                                 gfx::Transform target_value,
                                 base::TimeDelta duration);

  TransformAnimationCurveAdapter(const TransformAnimationCurveAdapter& other);

  TransformAnimationCurveAdapter& operator=(
      const TransformAnimationCurveAdapter&) = delete;

  ~TransformAnimationCurveAdapter() override;

  // TransformAnimationCurve implementation.
  base::TimeDelta Duration() const override;
  std::unique_ptr<gfx::AnimationCurve> Clone() const override;
  gfx::TransformOperations GetValue(base::TimeDelta t) const override;
  gfx::TransformOperations GetTransformedValue(
      base::TimeDelta t,
      gfx::TimingFunction::LimitDirection) const override;
  bool PreservesAxisAlignment() const override;
  bool MaximumScale(float* max_scale) const override;

 private:
  gfx::Tween::Type tween_type_;
  gfx::Transform initial_value_;
  gfx::TransformOperations initial_wrapped_value_;
  gfx::Transform target_value_;
  gfx::TransformOperations target_wrapped_value_;
  gfx::DecomposedTransform decomposed_initial_value_;
  gfx::DecomposedTransform decomposed_target_value_;
  base::TimeDelta duration_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TRANSFORM_ANIMATION_CURVE_ADAPTER_H_

