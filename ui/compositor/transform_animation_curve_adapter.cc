// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/transform_animation_curve_adapter.h"

#include "base/memory/ptr_util.h"

namespace ui {

namespace {

static cc::TransformOperations WrapTransform(const gfx::Transform& transform) {
  cc::TransformOperations operations;
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
      duration_(duration) {
  gfx::DecomposeTransform(&decomposed_initial_value_, initial_value);
  gfx::DecomposeTransform(&decomposed_target_value_, target_value);
}

TransformAnimationCurveAdapter::TransformAnimationCurveAdapter(
    const TransformAnimationCurveAdapter& other) = default;

TransformAnimationCurveAdapter::~TransformAnimationCurveAdapter() {
}

base::TimeDelta TransformAnimationCurveAdapter::Duration() const {
  return duration_;
}

std::unique_ptr<cc::AnimationCurve> TransformAnimationCurveAdapter::Clone()
    const {
  return base::WrapUnique(new TransformAnimationCurveAdapter(
      tween_type_, initial_value_, target_value_, duration_));
}

cc::TransformOperations TransformAnimationCurveAdapter::GetValue(
    base::TimeDelta t) const {
  if (t >= duration_)
    return target_wrapped_value_;
  if (t <= base::TimeDelta())
    return initial_wrapped_value_;

  gfx::DecomposedTransform to_return = gfx::BlendDecomposedTransforms(
      decomposed_target_value_, decomposed_initial_value_,
      gfx::Tween::CalculateValue(tween_type_, t / duration_));
  return WrapTransform(gfx::ComposeTransform(to_return));
}

bool TransformAnimationCurveAdapter::PreservesAxisAlignment() const {
  return (initial_value_.IsIdentity() ||
          initial_value_.IsScaleOrTranslation()) &&
         (target_value_.IsIdentity() || target_value_.IsScaleOrTranslation());
}

bool TransformAnimationCurveAdapter::MaximumScale(float* max_scale) const {
  return false;
}

InverseTransformCurveAdapter::InverseTransformCurveAdapter(
    TransformAnimationCurveAdapter base_curve,
    gfx::Transform initial_value,
    base::TimeDelta duration)
    : base_curve_(base_curve),
      initial_value_(initial_value),
      initial_wrapped_value_(WrapTransform(initial_value)),
      duration_(duration) {
  effective_initial_value_ =
      base_curve_.GetValue(base::TimeDelta()).Apply() * initial_value_;
}

InverseTransformCurveAdapter::~InverseTransformCurveAdapter() {
}

base::TimeDelta InverseTransformCurveAdapter::Duration() const {
  return duration_;
}

std::unique_ptr<cc::AnimationCurve> InverseTransformCurveAdapter::Clone()
    const {
  return base::WrapUnique(
      new InverseTransformCurveAdapter(base_curve_, initial_value_, duration_));
}

cc::TransformOperations InverseTransformCurveAdapter::GetValue(
    base::TimeDelta t) const {
  if (t <= base::TimeDelta())
    return initial_wrapped_value_;

  gfx::Transform base_transform = base_curve_.GetValue(t).Apply();
  // Invert base
  gfx::Transform to_return(gfx::Transform::kSkipInitialization);
  bool is_invertible = base_transform.GetInverse(&to_return);
  DCHECK(is_invertible);

  to_return.PreconcatTransform(effective_initial_value_);

  return WrapTransform(to_return);
}

bool InverseTransformCurveAdapter::PreservesAxisAlignment() const {
  return (initial_value_.IsIdentity() ||
          initial_value_.IsScaleOrTranslation()) &&
         (base_curve_.PreservesAxisAlignment());
}

bool InverseTransformCurveAdapter::MaximumScale(float* max_scale) const {
  return false;
}

}  // namespace ui
