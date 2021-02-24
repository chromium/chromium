// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/keyframe_effect.h"

#include <algorithm>

#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace gfx {

namespace {

static constexpr float kTolerance = 1e-5f;

static int s_next_keyframe_model_id = 1;
static int s_next_group_id = 1;

void ReverseKeyframeModel(base::TimeTicks monotonic_time,
                          KeyframeModel* keyframe_model) {
  keyframe_model->set_direction(keyframe_model->direction() ==
                                        KeyframeModel::Direction::NORMAL
                                    ? KeyframeModel::Direction::REVERSE
                                    : KeyframeModel::Direction::NORMAL);
  // Our goal here is to reverse the given keyframe_model. That is, if
  // we're 20% of the way through the keyframe_model in the forward direction,
  // we'd like to be 80% of the way of the reversed keyframe model (so it will
  // end quickly).
  //
  // We can modify our "progress" through an animation by modifying the "time
  // offset", a value added to the current time by the animation system before
  // applying any other adjustments.
  //
  // Let our start time be s, our current time be t, and our final time (or
  // duration) be d. After reversing the keyframe_model, we would like to start
  // sampling from d - t as depicted below.
  //
  //  Forward:
  //  s    t                         d
  //  |----|-------------------------|
  //
  //  Reversed:
  //  s                         t    d
  //  |----|--------------------|----|
  //       -----time-offset----->
  //
  // Now, if we let o represent our desired offset, we need to ensure that
  //   t = d - (o + t)
  //
  // That is, sampling at the current time in either the forward or reverse
  // curves must result in the same value, otherwise we'll get jank.
  //
  // This implies that,
  //   0 = d - o - 2t
  //   o = d - 2t
  //
  // Now if there was a previous offset, we must adjust d by that offset before
  // performing this computation, so it becomes d - o_old - 2t:
  keyframe_model->set_time_offset(
      keyframe_model->curve()->Duration() - keyframe_model->time_offset() -
      (2 * (monotonic_time - keyframe_model->start_time())));
}

std::unique_ptr<CubicBezierTimingFunction> CreateTransitionTimingFunction() {
  return CubicBezierTimingFunction::CreatePreset(
      CubicBezierTimingFunction::EaseType::EASE);
}

base::TimeDelta GetStartTime(KeyframeModel* keyframe_model) {
  if (keyframe_model->direction() == KeyframeModel::Direction::NORMAL) {
    return base::TimeDelta();
  }
  return keyframe_model->curve()->Duration();
}

base::TimeDelta GetEndTime(KeyframeModel* keyframe_model) {
  if (keyframe_model->direction() == KeyframeModel::Direction::REVERSE) {
    return base::TimeDelta();
  }
  return keyframe_model->curve()->Duration();
}

bool SufficientlyEqual(float lhs, float rhs) {
  return base::IsApproximatelyEqual(lhs, rhs, kTolerance);
}

bool SufficientlyEqual(const gfx::TransformOperations& lhs,
                       const gfx::TransformOperations& rhs) {
  return lhs.ApproximatelyEqual(rhs, kTolerance);
}

bool SufficientlyEqual(const gfx::SizeF& lhs, const gfx::SizeF& rhs) {
  return base::IsApproximatelyEqual(lhs.width(), rhs.width(), kTolerance) &&
         base::IsApproximatelyEqual(lhs.height(), rhs.height(), kTolerance);
}

bool SufficientlyEqual(SkColor lhs, SkColor rhs) {
  return lhs == rhs;
}

template <typename T>
struct AnimationTraits {};

#define DEFINE_ANIMATION_TRAITS(value_type, name)                         \
  template <>                                                             \
  struct AnimationTraits<value_type> {                                    \
    typedef value_type ValueType;                                         \
    typedef name##AnimationCurve::Target TargetType;                      \
    typedef name##AnimationCurve CurveType;                               \
    typedef Keyframed##name##AnimationCurve KeyframedCurveType;           \
    typedef name##Keyframe KeyframeType;                                  \
    static const CurveType* ToDerivedCurve(const AnimationCurve* curve) { \
      return name##AnimationCurve::To##name##AnimationCurve(curve);       \
    }                                                                     \
    static void OnValueAnimated(name##AnimationCurve::Target* target,     \
                                const ValueType& target_value,            \
                                int target_property) {                    \
      target->On##name##Animated(target_value, target_property, nullptr); \
    }                                                                     \
  }

DEFINE_ANIMATION_TRAITS(float, Float);
DEFINE_ANIMATION_TRAITS(gfx::TransformOperations, Transform);
DEFINE_ANIMATION_TRAITS(gfx::SizeF, Size);
DEFINE_ANIMATION_TRAITS(SkColor, Color);

#undef DEFINE_ANIMATION_TRAITS

template <typename ValueType>
void TransitionValueTo(KeyframeEffect* animator,
                       typename AnimationTraits<ValueType>::TargetType* target,
                       base::TimeTicks monotonic_time,
                       int target_property,
                       const ValueType& from,
                       const ValueType& to) {
  DCHECK(target);

  if (animator->transition().target_properties.find(target_property) ==
      animator->transition().target_properties.end()) {
    AnimationTraits<ValueType>::OnValueAnimated(target, to, target_property);
    return;
  }

  KeyframeModel* running_keyframe_model =
      animator->GetRunningKeyframeModelForProperty(target_property);

  ValueType effective_current = from;

  if (running_keyframe_model) {
    const auto* curve = AnimationTraits<ValueType>::ToDerivedCurve(
        running_keyframe_model->curve());

    if (running_keyframe_model->IsFinishedAt(monotonic_time)) {
      effective_current = curve->GetValue(GetEndTime(running_keyframe_model));
    } else {
      if (SufficientlyEqual(
              to, curve->GetValue(GetEndTime(running_keyframe_model)))) {
        return;
      }
      if (SufficientlyEqual(
              to, curve->GetValue(GetStartTime(running_keyframe_model)))) {
        ReverseKeyframeModel(monotonic_time, running_keyframe_model);
        return;
      }
    }
  } else if (SufficientlyEqual(to, from)) {
    return;
  }

  animator->RemoveKeyframeModels(target_property);

  std::unique_ptr<typename AnimationTraits<ValueType>::KeyframedCurveType>
      curve(AnimationTraits<ValueType>::KeyframedCurveType::Create());

  curve->AddKeyframe(AnimationTraits<ValueType>::KeyframeType::Create(
      base::TimeDelta(), effective_current, CreateTransitionTimingFunction()));

  curve->AddKeyframe(AnimationTraits<ValueType>::KeyframeType::Create(
      animator->transition().duration, to, CreateTransitionTimingFunction()));

  curve->set_target(target);

  animator->AddKeyframeModel(KeyframeModel::Create(
      std::move(curve), KeyframeEffect::GetNextKeyframeModelId(),
      target_property));
}

}  // namespace

int KeyframeEffect::GetNextKeyframeModelId() {
  return s_next_keyframe_model_id++;
}

int KeyframeEffect::GetNextGroupId() {
  return s_next_group_id++;
}

KeyframeEffect::KeyframeEffect() {}
KeyframeEffect::~KeyframeEffect() {}

void KeyframeEffect::AddKeyframeModel(
    std::unique_ptr<KeyframeModel> keyframe_model) {
  keyframe_models_.push_back(std::move(keyframe_model));
}

void KeyframeEffect::RemoveKeyframeModel(int keyframe_model_id) {
  base::EraseIf(keyframe_models_,
                [keyframe_model_id](
                    const std::unique_ptr<KeyframeModel>& keyframe_model) {
                  return keyframe_model->id() == keyframe_model_id;
                });
}

void KeyframeEffect::RemoveKeyframeModels(int target_property) {
  base::EraseIf(
      keyframe_models_,
      [target_property](const std::unique_ptr<KeyframeModel>& keyframe_model) {
        return keyframe_model->TargetProperty() == target_property;
      });
}

void KeyframeEffect::Tick(base::TimeTicks monotonic_time) {
  TickInternal(monotonic_time, true);
}

void KeyframeEffect::TickKeyframeModel(base::TimeTicks monotonic_time,
                                       KeyframeModel* keyframe_model) {
  if ((keyframe_model->run_state() != KeyframeModel::STARTING &&
       keyframe_model->run_state() != KeyframeModel::RUNNING &&
       keyframe_model->run_state() != KeyframeModel::PAUSED) ||
      !keyframe_model->HasActiveTime(monotonic_time)) {
    return;
  }

  AnimationCurve* curve = keyframe_model->curve();
  base::TimeDelta trimmed =
      keyframe_model->TrimTimeToCurrentIteration(monotonic_time);
  curve->Tick(trimmed, keyframe_model->TargetProperty(), keyframe_model);
}

void KeyframeEffect::TickInternal(base::TimeTicks monotonic_time,
                                  bool include_infinite_animations) {
  StartKeyframeModels(monotonic_time, include_infinite_animations);

  for (auto& keyframe_model : keyframe_models_) {
    if (!include_infinite_animations &&
        keyframe_model->iterations() == std::numeric_limits<double>::infinity())
      continue;
    TickKeyframeModel(monotonic_time, keyframe_model.get());
  }

  // Remove finished keyframe_models.
  base::EraseIf(
      keyframe_models_,
      [monotonic_time](const std::unique_ptr<KeyframeModel>& keyframe_model) {
        return !keyframe_model->is_finished() &&
               keyframe_model->IsFinishedAt(monotonic_time);
      });

  StartKeyframeModels(monotonic_time, include_infinite_animations);
}

void KeyframeEffect::FinishAll() {
  base::TimeTicks now = base::TimeTicks::Now();
  const bool include_infinite_animations = false;
  TickInternal(now, include_infinite_animations);
  TickInternal(base::TimeTicks::Max(), include_infinite_animations);
#ifndef NDEBUG
  for (auto& keyframe_model : keyframe_models_) {
    DCHECK_EQ(std::numeric_limits<double>::infinity(),
              keyframe_model->iterations());
  }
#endif
}

void KeyframeEffect::SetTransitionedProperties(
    const std::set<int>& properties) {
  transition_.target_properties = properties;
}

void KeyframeEffect::SetTransitionDuration(base::TimeDelta delta) {
  transition_.duration = delta;
}

void KeyframeEffect::TransitionFloatTo(FloatAnimationCurve::Target* target,
                                       base::TimeTicks monotonic_time,
                                       int target_property,
                                       float from,
                                       float to) {
  TransitionValueTo<float>(this, target, monotonic_time, target_property, from,
                           to);
}

void KeyframeEffect::TransitionTransformOperationsTo(
    TransformAnimationCurve::Target* target,
    base::TimeTicks monotonic_time,
    int target_property,
    const gfx::TransformOperations& from,
    const gfx::TransformOperations& to) {
  TransitionValueTo<gfx::TransformOperations>(this, target, monotonic_time,
                                              target_property, from, to);
}

void KeyframeEffect::TransitionSizeTo(SizeAnimationCurve::Target* target,
                                      base::TimeTicks monotonic_time,
                                      int target_property,
                                      const gfx::SizeF& from,
                                      const gfx::SizeF& to) {
  TransitionValueTo<gfx::SizeF>(this, target, monotonic_time, target_property,
                                from, to);
}

void KeyframeEffect::TransitionColorTo(ColorAnimationCurve::Target* target,
                                       base::TimeTicks monotonic_time,
                                       int target_property,
                                       SkColor from,
                                       SkColor to) {
  TransitionValueTo<SkColor>(this, target, monotonic_time, target_property,
                             from, to);
}

bool KeyframeEffect::IsAnimatingProperty(int property) const {
  for (auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->TargetProperty() == property)
      return true;
  }
  return false;
}

float KeyframeEffect::GetTargetFloatValue(int target_property,
                                          float default_value) const {
  return GetTargetValue<float>(target_property, default_value);
}

gfx::TransformOperations KeyframeEffect::GetTargetTransformOperationsValue(
    int target_property,
    const gfx::TransformOperations& default_value) const {
  return GetTargetValue<gfx::TransformOperations>(target_property,
                                                  default_value);
}

gfx::SizeF KeyframeEffect::GetTargetSizeValue(
    int target_property,
    const gfx::SizeF& default_value) const {
  return GetTargetValue<gfx::SizeF>(target_property, default_value);
}

SkColor KeyframeEffect::GetTargetColorValue(int target_property,
                                            SkColor default_value) const {
  return GetTargetValue<SkColor>(target_property, default_value);
}

void KeyframeEffect::StartKeyframeModels(base::TimeTicks monotonic_time,
                                         bool include_infinite_animations) {
  TargetProperties animated_properties;
  for (auto& keyframe_model : keyframe_models_) {
    if (!include_infinite_animations &&
        keyframe_model->iterations() == std::numeric_limits<double>::infinity())
      continue;
    if (keyframe_model->run_state() == KeyframeModel::RUNNING ||
        keyframe_model->run_state() == KeyframeModel::PAUSED) {
      animated_properties[keyframe_model->TargetProperty()] = true;
    }
  }
  for (auto& keyframe_model : keyframe_models_) {
    if (!include_infinite_animations &&
        keyframe_model->iterations() == std::numeric_limits<double>::infinity())
      continue;
    if (!animated_properties[keyframe_model->TargetProperty()] &&
        keyframe_model->run_state() ==
            KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY) {
      animated_properties[keyframe_model->TargetProperty()] = true;
      keyframe_model->SetRunState(KeyframeModel::RUNNING, monotonic_time);
      keyframe_model->set_start_time(monotonic_time);
    }
  }
}

KeyframeModel* KeyframeEffect::GetRunningKeyframeModelForProperty(
    int target_property) const {
  for (auto& keyframe_model : keyframe_models_) {
    if ((keyframe_model->run_state() == KeyframeModel::RUNNING ||
         keyframe_model->run_state() == KeyframeModel::PAUSED) &&
        keyframe_model->TargetProperty() == target_property) {
      return keyframe_model.get();
    }
  }
  return nullptr;
}

KeyframeModel* KeyframeEffect::GetKeyframeModelForProperty(
    int target_property) const {
  for (auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->TargetProperty() == target_property) {
      return keyframe_model.get();
    }
  }
  return nullptr;
}

template <typename ValueType>
ValueType KeyframeEffect::GetTargetValue(int target_property,
                                         const ValueType& default_value) const {
  KeyframeModel* running_keyframe_model =
      GetKeyframeModelForProperty(target_property);
  if (!running_keyframe_model) {
    return default_value;
  }
  const auto* curve = AnimationTraits<ValueType>::ToDerivedCurve(
      running_keyframe_model->curve());
  return curve->GetValue(GetEndTime(running_keyframe_model));
}

}  // namespace gfx
