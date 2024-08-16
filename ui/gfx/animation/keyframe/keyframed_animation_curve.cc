// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve-inl.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/box_f.h"

namespace gfx {
namespace {

static constexpr float kTolerance = 1e-5f;

template <typename KeyframeType, typename ValueType, typename TargetType>
std::unique_ptr<AnimationCurve> RetargettedCurve(
    std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    base::TimeDelta t,
    const ValueType& value_at_t,
    const ValueType& new_target_value,
    double scaled_duration,
    TargetType* target,
    const TimingFunction* timing_function) {
  if (SufficientlyEqual(keyframes.back()->Value(), new_target_value))
    return nullptr;

  DCHECK_GE(keyframes.size(), 2u);
  DCHECK_GT(scaled_duration, 0.f);

  // If we haven't progressed to animating between the last 2 keyframes, simply
  // clobber the value for the last keyframe.
  const bool at_last_keyframe =
      (keyframes[keyframes.size() - 2]->Time() * scaled_duration) <= t;
  if (!at_last_keyframe) {
    auto& last_keyframe = keyframes.back();
    auto* keyframe_timing_function = last_keyframe->timing_function();
    last_keyframe = KeyframeType::Create(
        last_keyframe->Time(), new_target_value,
        keyframe_timing_function ? keyframe_timing_function->Clone() : nullptr);
    return nullptr;
  }

  // Ensure that `t` happens between the last two keyframes.
  DCHECK_GE(keyframes[keyframes.size() - 1]->Time() * scaled_duration, t);

  // TODO(crbug.com/40177284): This can be changed to a different / special
  // interpolation curve type to maintain c2 continuity.
  auto curve = AnimationTraits<ValueType>::KeyframedCurveType::Create();
  curve->set_scaled_duration(scaled_duration);
  curve->set_target(target);

  auto generate_timing_function =
      [timing_function]() -> std::unique_ptr<gfx::TimingFunction> {
    if (timing_function)
      return timing_function->Clone();
    return nullptr;
  };

  // Keep the curve duration the same by adding the same first frame.
  curve->AddKeyframe(KeyframeType::Create(keyframes.front()->Time(),
                                          keyframes.front()->Value(),
                                          generate_timing_function()));

  // Snap the current value at `t` so that the current value stays the same.
  curve->AddKeyframe(KeyframeType::Create(t / scaled_duration, value_at_t,
                                          generate_timing_function()));

  // Add a new target at the same time as the last frame.
  curve->AddKeyframe(KeyframeType::Create(
      keyframes.back()->Time(), new_target_value, generate_timing_function()));

  return curve;
}

}  // namespace

Keyframe::Keyframe(base::TimeDelta time,
                   std::unique_ptr<TimingFunction> timing_function)
    : time_(time), timing_function_(std::move(timing_function)) {}

Keyframe::~Keyframe() = default;

base::TimeDelta Keyframe::Time() const {
  return time_;
}

std::unique_ptr<ColorKeyframe> ColorKeyframe::Create(
    base::TimeDelta time,
    SkColor value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new ColorKeyframe(time, value, std::move(timing_function)));
}

ColorKeyframe::ColorKeyframe(base::TimeDelta time,
                             SkColor value,
                             std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

ColorKeyframe::~ColorKeyframe() = default;

SkColor ColorKeyframe::Value() const {
  return value_;
}

std::unique_ptr<ColorKeyframe> ColorKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return ColorKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<FloatKeyframe> FloatKeyframe::Create(
    base::TimeDelta time,
    float value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new FloatKeyframe(time, value, std::move(timing_function)));
}

FloatKeyframe::FloatKeyframe(base::TimeDelta time,
                             float value,
                             std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

FloatKeyframe::~FloatKeyframe() = default;

float FloatKeyframe::Value() const {
  return value_;
}

std::unique_ptr<FloatKeyframe> FloatKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return FloatKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<TransformKeyframe> TransformKeyframe::Create(
    base::TimeDelta time,
    const gfx::TransformOperations& value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new TransformKeyframe(time, value, std::move(timing_function)));
}

TransformKeyframe::TransformKeyframe(
    base::TimeDelta time,
    const gfx::TransformOperations& value,
    std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

TransformKeyframe::~TransformKeyframe() = default;

const gfx::TransformOperations& TransformKeyframe::Value() const {
  return value_;
}

std::unique_ptr<TransformKeyframe> TransformKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return TransformKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<SizeKeyframe> SizeKeyframe::Create(
    base::TimeDelta time,
    const gfx::SizeF& value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new SizeKeyframe(time, value, std::move(timing_function)));
}

SizeKeyframe::SizeKeyframe(base::TimeDelta time,
                           const gfx::SizeF& value,
                           std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

SizeKeyframe::~SizeKeyframe() = default;

const gfx::SizeF& SizeKeyframe::Value() const {
  return value_;
}

std::unique_ptr<SizeKeyframe> SizeKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return SizeKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<RectKeyframe> RectKeyframe::Create(
    base::TimeDelta time,
    const gfx::Rect& value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new RectKeyframe(time, value, std::move(timing_function)));
}

RectKeyframe::RectKeyframe(base::TimeDelta time,
                           const gfx::Rect& value,
                           std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

RectKeyframe::~RectKeyframe() = default;

const gfx::Rect& RectKeyframe::Value() const {
  return value_;
}

std::unique_ptr<RectKeyframe> RectKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return RectKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<KeyframedColorAnimationCurve>
KeyframedColorAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedColorAnimationCurve);
}

KeyframedColorAnimationCurve::KeyframedColorAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedColorAnimationCurve::~KeyframedColorAnimationCurve() = default;

void KeyframedColorAnimationCurve::AddKeyframe(
    std::unique_ptr<ColorKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedColorAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

base::TimeDelta KeyframedColorAnimationCurve::TickInterval() const {
  return ComputeTickInterval(timing_function_, scaled_duration(), keyframes_);
}

std::unique_ptr<AnimationCurve> KeyframedColorAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedColorAnimationCurve> to_return =
      KeyframedColorAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

SkColor KeyframedColorAnimationCurve::GetValue(base::TimeDelta t) const {
  // Use GetTransformedValue instead.
  NOTREACHED();
}

SkColor KeyframedColorAnimationCurve::GetTransformedValue(
    base::TimeDelta t,
    gfx::TimingFunction::LimitDirection limit_direction) const {
  KeyframesAndProgress values = GetKeyframesAndProgress(
      keyframes_, timing_function_, scaled_duration(), t, limit_direction);
  return gfx::Tween::ColorValueBetween(values.progress,
                                       keyframes_[values.from]->Value(),
                                       keyframes_[values.to]->Value());
}

std::unique_ptr<AnimationCurve> KeyframedColorAnimationCurve::Retarget(
    base::TimeDelta t,
    SkColor new_target) {
  DCHECK(!keyframes_.empty());
  return RetargettedCurve(
      keyframes_, t,
      GetTransformedValue(t, gfx::TimingFunction::LimitDirection::RIGHT),
      new_target, scaled_duration(), target(), timing_function_.get());
}

std::unique_ptr<KeyframedFloatAnimationCurve>
KeyframedFloatAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedFloatAnimationCurve);
}

KeyframedFloatAnimationCurve::KeyframedFloatAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedFloatAnimationCurve::~KeyframedFloatAnimationCurve() = default;

void KeyframedFloatAnimationCurve::AddKeyframe(
    std::unique_ptr<FloatKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedFloatAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

base::TimeDelta KeyframedFloatAnimationCurve::TickInterval() const {
  return ComputeTickInterval(timing_function_, scaled_duration(), keyframes_);
}

std::unique_ptr<AnimationCurve> KeyframedFloatAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedFloatAnimationCurve> to_return =
      KeyframedFloatAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

std::unique_ptr<AnimationCurve> KeyframedFloatAnimationCurve::Retarget(
    base::TimeDelta t,
    float new_target) {
  DCHECK(!keyframes_.empty());
  return RetargettedCurve(
      keyframes_, t,
      GetTransformedValue(t, gfx::TimingFunction::LimitDirection::RIGHT),
      new_target, scaled_duration(), target(), timing_function_.get());
}

float KeyframedFloatAnimationCurve::GetValue(base::TimeDelta t) const {
  // Use GetTransformedValue instead.
  NOTREACHED();
}

float KeyframedFloatAnimationCurve::GetTransformedValue(
    base::TimeDelta t,
    gfx::TimingFunction::LimitDirection limit_direction) const {
  KeyframesAndProgress values = GetKeyframesAndProgress(
      keyframes_, timing_function_, scaled_duration(), t, limit_direction);
  double from = keyframes_[values.from]->Value();
  double to = keyframes_[values.to]->Value();
  return from + (to - from) * values.progress;
}

std::unique_ptr<KeyframedTransformAnimationCurve>
KeyframedTransformAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedTransformAnimationCurve);
}

KeyframedTransformAnimationCurve::KeyframedTransformAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedTransformAnimationCurve::~KeyframedTransformAnimationCurve() = default;

void KeyframedTransformAnimationCurve::AddKeyframe(
    std::unique_ptr<TransformKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedTransformAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

base::TimeDelta KeyframedTransformAnimationCurve::TickInterval() const {
  return ComputeTickInterval(timing_function_, scaled_duration(), keyframes_);
}

std::unique_ptr<AnimationCurve> KeyframedTransformAnimationCurve::Clone()
    const {
  std::unique_ptr<KeyframedTransformAnimationCurve> to_return =
      KeyframedTransformAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

gfx::TransformOperations KeyframedTransformAnimationCurve::GetValue(
    base::TimeDelta t) const {
  // Use GetTransformedValue instead.
  NOTREACHED();
}

gfx::TransformOperations KeyframedTransformAnimationCurve::GetTransformedValue(
    base::TimeDelta t,
    gfx::TimingFunction::LimitDirection limit_direction) const {
  KeyframesAndProgress values = GetKeyframesAndProgress(
      keyframes_, timing_function_, scaled_duration(), t, limit_direction);
  return keyframes_[values.to]->Value().Blend(keyframes_[values.from]->Value(),
                                              values.progress);
}

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

std::unique_ptr<AnimationCurve> KeyframedTransformAnimationCurve::Retarget(
    base::TimeDelta t,
    const gfx::TransformOperations& new_target) {
  DCHECK(!keyframes_.empty());
  return RetargettedCurve(
      keyframes_, t,
      GetTransformedValue(t, gfx::TimingFunction::LimitDirection::RIGHT),
      new_target, scaled_duration(), target(), timing_function_.get());
}

std::unique_ptr<KeyframedSizeAnimationCurve>
KeyframedSizeAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedSizeAnimationCurve);
}

KeyframedSizeAnimationCurve::KeyframedSizeAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedSizeAnimationCurve::~KeyframedSizeAnimationCurve() = default;

void KeyframedSizeAnimationCurve::AddKeyframe(
    std::unique_ptr<SizeKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedSizeAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

base::TimeDelta KeyframedSizeAnimationCurve::TickInterval() const {
  return ComputeTickInterval(timing_function_, scaled_duration(), keyframes_);
}

std::unique_ptr<AnimationCurve> KeyframedSizeAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedSizeAnimationCurve> to_return =
      KeyframedSizeAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

gfx::SizeF KeyframedSizeAnimationCurve::GetValue(base::TimeDelta t) const {
  // Use GetTransformedValue instead.
  NOTREACHED();
}

gfx::SizeF KeyframedSizeAnimationCurve::GetTransformedValue(
    base::TimeDelta t,
    gfx::TimingFunction::LimitDirection limit_direction) const {
  KeyframesAndProgress values = GetKeyframesAndProgress(
      keyframes_, timing_function_, scaled_duration(), t, limit_direction);
  return gfx::Tween::SizeFValueBetween(values.progress,
                                       keyframes_[values.from]->Value(),
                                       keyframes_[values.to]->Value());
}

std::unique_ptr<AnimationCurve> KeyframedSizeAnimationCurve::Retarget(
    base::TimeDelta t,
    const gfx::SizeF& new_target) {
  DCHECK(!keyframes_.empty());
  return RetargettedCurve(
      keyframes_, t,
      GetTransformedValue(t, gfx::TimingFunction::LimitDirection::RIGHT),
      new_target, scaled_duration(), target(), timing_function_.get());
}

std::unique_ptr<KeyframedRectAnimationCurve>
KeyframedRectAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedRectAnimationCurve);
}

KeyframedRectAnimationCurve::KeyframedRectAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedRectAnimationCurve::~KeyframedRectAnimationCurve() = default;

void KeyframedRectAnimationCurve::AddKeyframe(
    std::unique_ptr<RectKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedRectAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

base::TimeDelta KeyframedRectAnimationCurve::TickInterval() const {
  return ComputeTickInterval(timing_function_, scaled_duration(), keyframes_);
}

std::unique_ptr<AnimationCurve> KeyframedRectAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedRectAnimationCurve> to_return =
      KeyframedRectAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

gfx::Rect KeyframedRectAnimationCurve::GetValue(base::TimeDelta t) const {
  // Use GetTransformedValue instead.
  NOTREACHED();
}

gfx::Rect KeyframedRectAnimationCurve::GetTransformedValue(
    base::TimeDelta t,
    gfx::TimingFunction::LimitDirection limit_direction) const {
  KeyframesAndProgress values = GetKeyframesAndProgress(
      keyframes_, timing_function_, scaled_duration(), t, limit_direction);
  return gfx::Tween::RectValueBetween(values.progress,
                                      keyframes_[values.from]->Value(),
                                      keyframes_[values.to]->Value());
}

std::unique_ptr<AnimationCurve> KeyframedRectAnimationCurve::Retarget(
    base::TimeDelta t,
    const gfx::Rect& new_target) {
  DCHECK(!keyframes_.empty());
  return RetargettedCurve(
      keyframes_, t,
      GetTransformedValue(t, gfx::TimingFunction::LimitDirection::RIGHT),
      new_target, scaled_duration(), target(), timing_function_.get());
}

bool SufficientlyEqual(float lhs, float rhs) {
  return base::IsApproximatelyEqual(lhs, rhs, kTolerance);
}

bool SufficientlyEqual(const TransformOperations& lhs,
                       const TransformOperations& rhs) {
  return lhs.Apply().ApproximatelyEqual(rhs.Apply(), kTolerance);
}

bool SufficientlyEqual(const SizeF& lhs, const SizeF& rhs) {
  return base::IsApproximatelyEqual(lhs.width(), rhs.width(), kTolerance) &&
         base::IsApproximatelyEqual(lhs.height(), rhs.height(), kTolerance);
}

bool SufficientlyEqual(SkColor lhs, SkColor rhs) {
  return lhs == rhs;
}

bool SufficientlyEqual(const Rect& lhs, const Rect& rhs) {
  return lhs == rhs;
}

}  // namespace gfx
