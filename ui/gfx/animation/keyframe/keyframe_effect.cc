// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/keyframe_effect.h"

#include <algorithm>
#include <vector>

#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace gfx {

namespace {

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
      effective_current = curve->GetTransformedValue(
          GetEndTime(running_keyframe_model),
          gfx::TimingFunction::LimitDirection::RIGHT);
    } else {
      if (SufficientlyEqual(to,
                            curve->GetTransformedValue(
                                GetEndTime(running_keyframe_model),
                                gfx::TimingFunction::LimitDirection::RIGHT))) {
        return;
      }
      if (SufficientlyEqual(to,
                            curve->GetTransformedValue(
                                GetStartTime(running_keyframe_model),
                                gfx::TimingFunction::LimitDirection::RIGHT))) {
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

KeyframeEffect::KeyframeEffect() = default;
KeyframeEffect::KeyframeEffect(KeyframeEffect&&) = default;
KeyframeEffect::~KeyframeEffect() = default;

void KeyframeEffect::AddKeyframeModel(
    std::unique_ptr<KeyframeModel> keyframe_model) {
  keyframe_models_.push_back(std::move(keyframe_model));
}

void KeyframeEffect::RemoveKeyframeModel(int keyframe_model_id) {
  // Since we want to use the KeyframeModels that we're going to remove, we
  // need to use a stable_partition here instead of remove_if. remove_if leaves
  // the removed items in an unspecified state.
  auto keyframe_models_to_remove = std::stable_partition(
      keyframe_models_.begin(), keyframe_models_.end(),
      [keyframe_model_id](
          const std::unique_ptr<gfx::KeyframeModel>& keyframe_model) {
        return keyframe_model->id() != keyframe_model_id;
      });

  RemoveKeyframeModelRange(keyframe_models_to_remove, keyframe_models_.end());
}

void KeyframeEffect::RemoveKeyframeModels(int target_property) {
  auto keyframe_models_to_remove = std::stable_partition(
      keyframe_models_.begin(), keyframe_models_.end(),
      [target_property](
          const std::unique_ptr<gfx::KeyframeModel>& keyframe_model) {
        return keyframe_model->TargetProperty() != target_property;
      });
  RemoveKeyframeModelRange(keyframe_models_to_remove, keyframe_models_.end());
}

void KeyframeEffect::RemoveAllKeyframeModels() {
  RemoveKeyframeModelRange(keyframe_models_.begin(), keyframe_models_.end());
}

bool KeyframeEffect::Tick(base::TimeTicks monotonic_time) {
  return TickInternal(monotonic_time, true);
}

void KeyframeEffect::RemoveKeyframeModelRange(
    typename KeyframeModels::iterator to_remove_begin,
    typename KeyframeModels::iterator to_remove_end) {
  keyframe_models_.erase(to_remove_begin, to_remove_end);
}

void KeyframeEffect::TickKeyframeModel(base::TimeTicks monotonic_time,
                                       KeyframeModel* keyframe_model) {
  if ((keyframe_model->run_state() != KeyframeModel::STARTING &&
       keyframe_model->run_state() != KeyframeModel::RUNNING &&
       keyframe_model->run_state() != KeyframeModel::PAUSED &&
       keyframe_model->run_state() != KeyframeModel::WAITING_FOR_DELETION) ||
      !keyframe_model->HasActiveTime(monotonic_time)) {
    return;
  }

  AnimationCurve* curve = keyframe_model->curve();
  TimingFunction::LimitDirection limit_direction;
  base::TimeDelta trimmed = keyframe_model->TrimTimeToCurrentIteration(
      monotonic_time, &limit_direction);
  curve->Tick(trimmed, keyframe_model->TargetProperty(), keyframe_model,
              limit_direction);
}

bool KeyframeEffect::TickInternal(base::TimeTicks monotonic_time,
                                  bool include_infinite_animations) {
  StartKeyframeModels(monotonic_time, include_infinite_animations);

  bool active = false;
  for (auto& keyframe_model : keyframe_models_) {
    if (!include_infinite_animations &&
        keyframe_model->iterations() == std::numeric_limits<double>::infinity())
      continue;
    TickKeyframeModel(monotonic_time, keyframe_model.get());
    active = true;
  }

  // Remove finished keyframe_models.
  std::erase_if(
      keyframe_models_,
      [monotonic_time](const std::unique_ptr<KeyframeModel>& keyframe_model) {
        return !keyframe_model->is_finished() &&
               keyframe_model->IsFinishedAt(monotonic_time);
      });

  StartKeyframeModels(monotonic_time, include_infinite_animations);
  return active;
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

bool KeyframeEffect::IsAnimating() const {
  return !keyframe_models_.empty();
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

KeyframeModel* KeyframeEffect::GetKeyframeModel(int target_property) const {
  for (size_t i = 0; i < keyframe_models().size(); ++i) {
    size_t index = keyframe_models().size() - i - 1;
    if (keyframe_models_[index]->TargetProperty() == target_property)
      return keyframe_models_[index].get();
  }
  return nullptr;
}

KeyframeModel* KeyframeEffect::GetKeyframeModelById(int id) const {
  for (auto& keyframe_model : keyframe_models())
    if (keyframe_model->id() == id)
      return keyframe_model.get();
  return nullptr;
}

template <typename ValueType>
ValueType KeyframeEffect::GetTargetValue(int target_property,
                                         const ValueType& default_value) const {
  KeyframeModel* running_keyframe_model = GetKeyframeModel(target_property);
  if (!running_keyframe_model) {
    return default_value;
  }
  const auto* curve = AnimationTraits<ValueType>::ToDerivedCurve(
      running_keyframe_model->curve());
  return curve->GetTransformedValue(GetEndTime(running_keyframe_model),
                                    gfx::TimingFunction::LimitDirection::RIGHT);
}

}  // namespace gfx
