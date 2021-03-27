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
  if (t <= (keyframes_.front()->Time() * scaled_duration()))
    return keyframes_.front()->Value();

  if (t >= (keyframes_.back()->Time() * scaled_duration()))
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, scaled_duration(),
                               t);
  size_t i = GetActiveKeyframe(keyframes_, scaled_duration(), t);
  double progress =
      TransformedKeyframeProgress(keyframes_, scaled_duration(), t, i);

  return gfx::Tween::ColorValueBetween(progress, keyframes_[i]->Value(),
                                       keyframes_[i + 1]->Value());
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

float KeyframedFloatAnimationCurve::GetValue(base::TimeDelta t) const {
  if (t <= (keyframes_.front()->Time() * scaled_duration()))
    return keyframes_.front()->Value();

  if (t >= (keyframes_.back()->Time() * scaled_duration()))
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, scaled_duration(),
                               t);
  size_t i = GetActiveKeyframe(keyframes_, scaled_duration(), t);
  double progress =
      TransformedKeyframeProgress(keyframes_, scaled_duration(), t, i);

  return keyframes_[i]->Value() +
         (keyframes_[i + 1]->Value() - keyframes_[i]->Value()) * progress;
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
  if (t <= (keyframes_.front()->Time() * scaled_duration()))
    return keyframes_.front()->Value();

  if (t >= (keyframes_.back()->Time() * scaled_duration()))
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, scaled_duration(),
                               t);
  size_t i = GetActiveKeyframe(keyframes_, scaled_duration(), t);
  double progress =
      TransformedKeyframeProgress(keyframes_, scaled_duration(), t, i);

  return keyframes_[i + 1]->Value().Blend(keyframes_[i]->Value(), progress);
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
  if (t <= (keyframes_.front()->Time() * scaled_duration()))
    return keyframes_.front()->Value();

  if (t >= (keyframes_.back()->Time() * scaled_duration()))
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, scaled_duration(),
                               t);
  size_t i = GetActiveKeyframe(keyframes_, scaled_duration(), t);
  double progress =
      TransformedKeyframeProgress(keyframes_, scaled_duration(), t, i);

  return gfx::Tween::SizeFValueBetween(progress, keyframes_[i]->Value(),
                                       keyframes_[i + 1]->Value());
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
  if (t <= (keyframes_.front()->Time() * scaled_duration()))
    return keyframes_.front()->Value();

  if (t >= (keyframes_.back()->Time() * scaled_duration()))
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, scaled_duration(),
                               t);
  size_t i = GetActiveKeyframe(keyframes_, scaled_duration(), t);
  double progress =
      TransformedKeyframeProgress(keyframes_, scaled_duration(), t, i);

  return gfx::Tween::RectValueBetween(progress, keyframes_[i]->Value(),
                                      keyframes_[i + 1]->Value());
}

}  // namespace gfx
