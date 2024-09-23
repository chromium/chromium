// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_FLOAT_ANIMATION_CURVE_ADAPTER_H_
#define UI_COMPOSITOR_FLOAT_ANIMATION_CURVE_ADAPTER_H_

#include <memory>

#include "base/time/time.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/tween.h"

namespace ui {

class FloatAnimationCurveAdapter : public gfx::FloatAnimationCurve {
 public:
  FloatAnimationCurveAdapter(gfx::Tween::Type tween_type,
                             float initial_value,
                             float target_value,
                             base::TimeDelta duration);

  ~FloatAnimationCurveAdapter() override {}

  // FloatAnimationCurve implementation.
  base::TimeDelta Duration() const override;
  std::unique_ptr<gfx::AnimationCurve> Clone() const override;
  float GetValue(base::TimeDelta t) const override;
  float GetTransformedValue(base::TimeDelta t,
                            gfx::TimingFunction::LimitDirection) const override;

 private:
  gfx::Tween::Type tween_type_;
  float initial_value_;
  float target_value_;
  base::TimeDelta duration_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_FLOAT_ANIMATION_CURVE_ADAPTER_H_
