// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/test/animation_utils.h"

#include "base/time/time.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace gfx {

std::unique_ptr<KeyframeModel> CreateTransformAnimation(
    TransformAnimationCurve::Target* target,
    int id,
    int property_id,
    const TransformOperations& from,
    const TransformOperations& to,
    base::TimeDelta duration) {
  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(duration, to, nullptr));
  curve->set_target(target);
  std::unique_ptr<KeyframeModel> keyframe_model(
      KeyframeModel::Create(std::move(curve), id, property_id));
  return keyframe_model;
}

std::unique_ptr<KeyframeModel> CreateSizeAnimation(
    SizeAnimationCurve::Target* target,
    int id,
    int property_id,
    const SizeF& from,
    const SizeF& to,
    base::TimeDelta duration) {
  std::unique_ptr<KeyframedSizeAnimationCurve> curve(
      KeyframedSizeAnimationCurve::Create());
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(SizeKeyframe::Create(duration, to, nullptr));
  curve->set_target(target);
  std::unique_ptr<KeyframeModel> keyframe_model(
      KeyframeModel::Create(std::move(curve), id, property_id));
  return keyframe_model;
}

std::unique_ptr<KeyframeModel> CreateFloatAnimation(
    FloatAnimationCurve::Target* target,
    int id,
    int property_id,
    float from,
    float to,
    base::TimeDelta duration) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(FloatKeyframe::Create(duration, to, nullptr));
  curve->set_target(target);
  std::unique_ptr<KeyframeModel> keyframe_model(
      KeyframeModel::Create(std::move(curve), id, property_id));
  return keyframe_model;
}

std::unique_ptr<KeyframeModel> CreateColorAnimation(
    ColorAnimationCurve::Target* target,
    int id,
    int property_id,
    SkColor from,
    SkColor to,
    base::TimeDelta duration) {
  std::unique_ptr<KeyframedColorAnimationCurve> curve(
      KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(ColorKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(ColorKeyframe::Create(duration, to, nullptr));
  curve->set_target(target);
  std::unique_ptr<KeyframeModel> keyframe_model(
      KeyframeModel::Create(std::move(curve), id, property_id));
  return keyframe_model;
}

base::TimeTicks MicrosecondsToTicks(uint64_t us) {
  base::TimeTicks to_return;
  return base::Microseconds(us) + to_return;
}

base::TimeDelta MicrosecondsToDelta(uint64_t us) {
  return base::Microseconds(us);
}

base::TimeTicks MsToTicks(uint64_t ms) {
  return MicrosecondsToTicks(1000 * ms);
}

base::TimeDelta MsToDelta(uint64_t ms) {
  return MicrosecondsToDelta(1000 * ms);
}

}  // namespace gfx
