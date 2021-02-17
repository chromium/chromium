// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve-inl.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/box_f.h"

namespace gfx {

KeyframeAnimationTimeValues ComputeKeyframeAnimationTimeValues(
    base::TimeDelta start_time,
    base::TimeDelta end_time,
    double scaled_duration,
    base::TimeDelta time) {
  KeyframeAnimationTimeValues values;
  values.start_time = start_time * scaled_duration;
  values.duration = (end_time * scaled_duration) - values.start_time;
  const base::TimeDelta elapsed = time - values.start_time;
  values.progress = (elapsed.is_inf() || values.duration.is_zero())
                        ? 1.0
                        : (elapsed / values.duration);
  return values;
}

base::TimeDelta ComputeTransformedAnimationTime(
    base::TimeDelta start_time,
    base::TimeDelta end_time,
    const std::unique_ptr<TimingFunction>& timing_function,
    double scaled_duration,
    base::TimeDelta time) {
  if (timing_function) {
    const auto values = ComputeKeyframeAnimationTimeValues(
        start_time, end_time, scaled_duration, time);
    time = (values.duration * timing_function->GetValue(values.progress)) +
           values.start_time;
  }
  return time;
}

Keyframe::Keyframe(base::TimeDelta time,
                   std::unique_ptr<TimingFunction> timing_function)
    : time_(time), timing_function_(std::move(timing_function)) {}

Keyframe::~Keyframe() = default;

base::TimeDelta Keyframe::Time() const {
  return time_;
}

KEYFRAMED_ANIMATION_CURVE_DEFINITION(SkColor, Color, Tween::ColorValueBetween)
KEYFRAMED_ANIMATION_CURVE_DEFINITION(float, Float, Tween::FloatValueBetween)
KEYFRAMED_ANIMATION_CURVE_DEFINITION(SizeF, Size, Tween::SizeFValueBetween)
KEYFRAMED_ANIMATION_CURVE_DEFINITION(TransformOperations,
                                     Transform,
                                     Tween::TransformOperationsValueBetween)

bool KeyframedTransformAnimationCurve::PreservesAxisAlignment() const {
  for (const auto& keyframe : keyframes_) {
    if (!keyframe->Value().PreservesAxisAlignment())
      return false;
  }
  return true;
}

bool KeyframedTransformAnimationCurve::MaximumScale(float* max_scale) const {
  DCHECK_GE(keyframes_.size(), 2ul);
  *max_scale = 0.f;
  for (auto& keyframe : keyframes_) {
    float keyframe_scale = 0.f;
    if (!keyframe->Value().ScaleComponent(&keyframe_scale))
      continue;
    *max_scale = std::max(*max_scale, keyframe_scale);
  }
  return *max_scale > 0.f;
}

}  // namespace gfx
