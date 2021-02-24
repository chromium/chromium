// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_KEYFRAME_EFFECT_H_
#define UI_GFX_ANIMATION_KEYFRAME_KEYFRAME_EFFECT_H_

#include <bitset>
#include <set>
#include <vector>

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframe_animation_export.h"
#include "ui/gfx/animation/keyframe/keyframe_model.h"
#include "ui/gfx/animation/keyframe/transition.h"

namespace gfx {
class SizeF;
class TransformOperations;

static constexpr size_t kMaxTargetPropertyId = 32u;
using TargetProperties = std::bitset<kMaxTargetPropertyId>;

// This is a simplified version of cc::KeyframeEffect. Its sole purpose is the
// management of its collection of KeyframeModels. Ticking them, updating their
// state, and deleting them as required.
//
// For background on the name of this class, please refer to the WebAnimations
// spec: https://www.w3.org/TR/web-animations-1/#the-keyframeeffect-interface
//
// TODO(crbug.com/747185): Make cc::KeyframeEffect a subclass of KeyframeEffect
// and share common code.
class GFX_KEYFRAME_ANIMATION_EXPORT KeyframeEffect {
 public:
  static int GetNextKeyframeModelId();
  static int GetNextGroupId();

  KeyframeEffect();
  ~KeyframeEffect();

  KeyframeEffect(const KeyframeEffect&) = delete;
  KeyframeEffect& operator=(const KeyframeEffect&) = delete;

  void AddKeyframeModel(std::unique_ptr<KeyframeModel> keyframe_model);
  void RemoveKeyframeModel(int keyframe_model_id);
  void RemoveKeyframeModels(int target_property);

  void Tick(base::TimeTicks monotonic_time);

  // This ticks all keyframe models until they are complete.
  void FinishAll();

  using KeyframeModels = std::vector<std::unique_ptr<KeyframeModel>>;
  const KeyframeModels& keyframe_models() { return keyframe_models_; }

  // The transition is analogous to CSS transitions. When configured, the
  // transition object will cause subsequent calls the corresponding
  // TransitionXXXTo functions to induce transition animations.
  const Transition& transition() const { return transition_; }
  void set_transition(const Transition& transition) {
    transition_ = transition;
  }

  void SetTransitionedProperties(const std::set<int>& properties);
  void SetTransitionDuration(base::TimeDelta delta);

  void TransitionFloatTo(FloatAnimationCurve::Target* target,
                         base::TimeTicks monotonic_time,
                         int target_property,
                         float from,
                         float to);
  void TransitionTransformOperationsTo(TransformAnimationCurve::Target* target,
                                       base::TimeTicks monotonic_time,
                                       int target_property,
                                       const gfx::TransformOperations& from,
                                       const gfx::TransformOperations& to);
  void TransitionSizeTo(SizeAnimationCurve::Target* target,
                        base::TimeTicks monotonic_time,
                        int target_property,
                        const gfx::SizeF& from,
                        const gfx::SizeF& to);
  void TransitionColorTo(ColorAnimationCurve::Target* target,
                         base::TimeTicks monotonic_time,
                         int target_property,
                         SkColor from,
                         SkColor to);

  bool IsAnimatingProperty(int property) const;

  float GetTargetFloatValue(int target_property, float default_value) const;
  gfx::TransformOperations GetTargetTransformOperationsValue(
      int target_property,
      const gfx::TransformOperations& default_value) const;
  gfx::SizeF GetTargetSizeValue(int target_property,
                                const gfx::SizeF& default_value) const;
  SkColor GetTargetColorValue(int target_property, SkColor default_value) const;
  KeyframeModel* GetRunningKeyframeModelForProperty(int target_property) const;

 private:
  void TickKeyframeModel(base::TimeTicks monotonic_time,
                         KeyframeModel* keyframe_model);
  void TickInternal(base::TimeTicks monotonic_time,
                    bool include_infinite_animations);
  void StartKeyframeModels(base::TimeTicks monotonic_time,
                           bool include_infinite_animations);
  KeyframeModel* GetKeyframeModelForProperty(int target_property) const;
  template <typename ValueType>
  ValueType GetTargetValue(int target_property,
                           const ValueType& default_value) const;

  KeyframeModels keyframe_models_;
  Transition transition_;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_KEYFRAME_KEYFRAME_EFFECT_H_
