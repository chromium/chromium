// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_ANIMATION_CURVE_H_
#define UI_GFX_ANIMATION_KEYFRAME_ANIMATION_CURVE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/keyframe/keyframe_animation_export.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {
class TransformOperations;
class KeyframeModel;

// An animation curve is a function that returns a value given a time.
class GFX_KEYFRAME_ANIMATION_EXPORT AnimationCurve {
 public:
  // TODO(crbug.com/40747850): we shouldn't need the curve type, long term.
  //
  // In the meanime, external clients of the animation machinery will have
  // other curve types and should be added to this enum to ensure uniqueness
  // (eg, there are serveral cc-specific types here, presently).
  enum CurveType {
    COLOR = 0,
    FLOAT,
    TRANSFORM,
    SIZE,
    RECT,

    // cc:: curve types.
    FILTER,
    SCROLL_OFFSET,
  };

  virtual ~AnimationCurve() = default;

  virtual base::TimeDelta Duration() const = 0;
  virtual int Type() const = 0;
  virtual const char* TypeName() const = 0;
  virtual std::unique_ptr<AnimationCurve> Clone() const = 0;
  virtual void Tick(
      base::TimeDelta t,
      int property_id,
      KeyframeModel* keyframe_model,
      gfx::TimingFunction::LimitDirection limit_direction) const = 0;

  // Returns true if this animation preserves axis alignment.
  virtual bool PreservesAxisAlignment() const;

  // Set |max_scale| to the maximum scale along any dimension during the
  // animation, of all steps (keyframes) with calculatable scale. Returns
  // false if none of the steps can calculate a scale.
  virtual bool MaximumScale(float* max_scale) const;

  // Returns step interval if it's step animation. Returns 0 otherwise.
  virtual base::TimeDelta TickInterval() const;
};

// Two methods are provided for sampling curves: GetValue and
// GetTransformedValue. Use GetTransformedValue when a timing function is
// being applied to the sampled animation curve.
// RAW_PTR_EXCLUSION: #macro
#define DECLARE_ANIMATION_CURVE_BODY(T, Name)                                 \
 public:                                                                      \
  static const Name##AnimationCurve* To##Name##AnimationCurve(                \
      const AnimationCurve* c);                                               \
  static Name##AnimationCurve* To##Name##AnimationCurve(AnimationCurve* c);   \
  class Target {                                                              \
   public:                                                                    \
    virtual ~Target() = default;                                              \
    virtual void On##Name##Animated(const T& value,                           \
                                    int target_property_id,                   \
                                    gfx::KeyframeModel* keyframe_model) = 0;  \
  };                                                                          \
  ~Name##AnimationCurve() override = default;                                 \
  virtual T GetValue(base::TimeDelta t) const = 0;                            \
  virtual T GetTransformedValue(                                              \
      base::TimeDelta t, gfx::TimingFunction::LimitDirection limit_direction) \
      const = 0;                                                              \
  void Tick(base::TimeDelta t, int property_id,                               \
            gfx::KeyframeModel* keyframe_model,                               \
            gfx::TimingFunction::LimitDirection limit_direction =             \
                gfx::TimingFunction::LimitDirection::RIGHT) const override;   \
  void set_target(Target* target) {                                           \
    target_ = target;                                                         \
  }                                                                           \
  int Type() const override;                                                  \
  const char* TypeName() const override;                                      \
                                                                              \
 protected:                                                                   \
  Target* target() const {                                                    \
    return target_;                                                           \
  }                                                                           \
                                                                              \
 private:                                                                     \
  raw_ptr<Target, DanglingUntriaged> target_ = nullptr;

class GFX_KEYFRAME_ANIMATION_EXPORT ColorAnimationCurve
    : public AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(SkColor, Color)
};

class GFX_KEYFRAME_ANIMATION_EXPORT FloatAnimationCurve
    : public AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(float, Float)
};

class GFX_KEYFRAME_ANIMATION_EXPORT SizeAnimationCurve : public AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(gfx::SizeF, Size)
};

class GFX_KEYFRAME_ANIMATION_EXPORT TransformAnimationCurve
    : public AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(gfx::TransformOperations, Transform)
};

class GFX_KEYFRAME_ANIMATION_EXPORT RectAnimationCurve : public AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(gfx::Rect, Rect)
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_KEYFRAME_ANIMATION_CURVE_H_
