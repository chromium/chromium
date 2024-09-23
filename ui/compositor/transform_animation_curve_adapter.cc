// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/transform_animation_curve_adapter.h"

#include "base/memory/ptr_util.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/transform_util.h"

namespace ui {

namespace {

static gfx::TransformOperations WrapTransform(const gfx::Transform& transform) {
  gfx::TransformOperations operations;
  operations.AppendMatrix(transform);
  return operations;
}

}  // namespace

TransformAnimationCurveAdapter::TransformAnimationCurveAdapter(
    gfx::Tween::Type tween_type,
    gfx::Transform initial_value,
    gfx::Transform target_value,
    base::TimeDelta duration)
    : tween_type_(tween_type),
      initial_value_(initial_value),
      initial_wrapped_value_(WrapTransform(initial_value)),
      target_value_(target_value),
      target_wrapped_value_(WrapTransform(target_value)),
      decomposed_initial_value_(
          initial_value.Decompose().value_or(gfx::DecomposedTransform())),
      decomposed_target_value_(
          target_value.Decompose().value_or(gfx::DecomposedTransform())),
      duration_(duration) {}

TransformAnimationCurveAdapter::TransformAnimationCurveAdapter(
    const TransformAnimationCurveAdapter& other) = default;

TransformAnimationCurveAdapter::~TransformAnimationCurveAdapter() {
}

base::TimeDelta TransformAnimationCurveAdapter::Duration() const {
  return duration_;
}

std::unique_ptr<gfx::AnimationCurve> TransformAnimationCurveAdapter::Clone()
    const {
  return base::WrapUnique(new TransformAnimationCurveAdapter(
      tween_type_, initial_value_, target_value_, duration_));
}

gfx::TransformOperations TransformAnimationCurveAdapter::GetValue(
    base::TimeDelta t) const {
  if (t >= duration_)
    return target_wrapped_value_;
  if (t <= base::TimeDelta())
    return initial_wrapped_value_;

  gfx::DecomposedTransform to_return = gfx::BlendDecomposedTransforms(
      decomposed_target_value_, decomposed_initial_value_,
      gfx::Tween::CalculateValue(tween_type_, t / duration_));
  return WrapTransform(gfx::Transform::Compose(to_return));
}

gfx::TransformOperations TransformAnimationCurveAdapter::GetTransformedValue(
    base::TimeDelta t,
    gfx::TimingFunction::LimitDirection) const {
  return GetValue(t);
}

bool TransformAnimationCurveAdapter::PreservesAxisAlignment() const {
  return (initial_value_.IsIdentity() ||
          initial_value_.IsScaleOrTranslation()) &&
         (target_value_.IsIdentity() || target_value_.IsScaleOrTranslation());
}

bool TransformAnimationCurveAdapter::MaximumScale(float* max_scale) const {
  constexpr float kInvalidScale = 0.f;
  gfx::Vector2dF initial_scales =
      gfx::ComputeTransform2dScaleComponents(initial_value_, kInvalidScale);
  gfx::Vector2dF target_scales =
      gfx::ComputeTransform2dScaleComponents(target_value_, kInvalidScale);
  *max_scale = std::max({initial_scales.x(), initial_scales.y(),
                         target_scales.x(), target_scales.y()});
  return *max_scale != kInvalidScale;
}

}  // namespace ui
