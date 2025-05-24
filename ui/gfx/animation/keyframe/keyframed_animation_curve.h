// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_H_
#define UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframe_animation_export.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform_operations.h"

namespace gfx {

class GFX_KEYFRAME_ANIMATION_EXPORT Keyframe {
 public:
  Keyframe(const Keyframe&) = delete;
  Keyframe& operator=(const Keyframe&) = delete;

  base::TimeDelta Time() const;
  const TimingFunction* timing_function() const {
    return timing_function_.get();
  }

 protected:
  Keyframe(base::TimeDelta time,
           std::unique_ptr<TimingFunction> timing_function);
  virtual ~Keyframe();

 private:
  base::TimeDelta time_;
  std::unique_ptr<TimingFunction> timing_function_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT ColorKeyframe : public Keyframe {
 public:
  static std::unique_ptr<ColorKeyframe> Create(
      base::TimeDelta time,
      SkColor value,
      std::unique_ptr<TimingFunction> timing_function);
  ~ColorKeyframe() override;

  SkColor Value() const;

  std::unique_ptr<ColorKeyframe> Clone() const;

 private:
  ColorKeyframe(base::TimeDelta time,
                SkColor value,
                std::unique_ptr<TimingFunction> timing_function);

  SkColor value_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT FloatKeyframe : public Keyframe {
 public:
  static std::unique_ptr<FloatKeyframe> Create(
      base::TimeDelta time,
      float value,
      std::unique_ptr<TimingFunction> timing_function);
  ~FloatKeyframe() override;

  float Value() const;

  std::unique_ptr<FloatKeyframe> Clone() const;

 private:
  FloatKeyframe(base::TimeDelta time,
                float value,
                std::unique_ptr<TimingFunction> timing_function);

  float value_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT TransformKeyframe : public Keyframe {
 public:
  static std::unique_ptr<TransformKeyframe> Create(
      base::TimeDelta time,
      const gfx::TransformOperations& value,
      std::unique_ptr<TimingFunction> timing_function);
  ~TransformKeyframe() override;

  const gfx::TransformOperations& Value() const;

  std::unique_ptr<TransformKeyframe> Clone() const;

 private:
  TransformKeyframe(base::TimeDelta time,
                    const gfx::TransformOperations& value,
                    std::unique_ptr<TimingFunction> timing_function);

  gfx::TransformOperations value_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT SizeKeyframe : public Keyframe {
 public:
  static std::unique_ptr<SizeKeyframe> Create(
      base::TimeDelta time,
      const gfx::SizeF& bounds,
      std::unique_ptr<TimingFunction> timing_function);
  ~SizeKeyframe() override;

  const gfx::SizeF& Value() const;

  std::unique_ptr<SizeKeyframe> Clone() const;

 private:
  SizeKeyframe(base::TimeDelta time,
               const gfx::SizeF& value,
               std::unique_ptr<TimingFunction> timing_function);

  gfx::SizeF value_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT RectKeyframe : public Keyframe {
 public:
  static std::unique_ptr<RectKeyframe> Create(
      base::TimeDelta time,
      const gfx::Rect& value,
      std::unique_ptr<TimingFunction> timing_function);
  ~RectKeyframe() override;

  const gfx::Rect& Value() const;

  std::unique_ptr<RectKeyframe> Clone() const;

 private:
  RectKeyframe(base::TimeDelta time,
               const gfx::Rect& value,
               std::unique_ptr<TimingFunction> timing_function);

  gfx::Rect value_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT KeyframedColorAnimationCurve
    : public ColorAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static std::unique_ptr<KeyframedColorAnimationCurve> Create();

  KeyframedColorAnimationCurve(const KeyframedColorAnimationCurve&) = delete;
  ~KeyframedColorAnimationCurve() override;

  KeyframedColorAnimationCurve& operator=(const KeyframedColorAnimationCurve&) =
      delete;

  void AddKeyframe(std::unique_ptr<ColorKeyframe> keyframe);
  const TimingFunction* timing_function() const {
    return timing_function_.get();
  }
  void SetTimingFunction(std::unique_ptr<TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;
  base::TimeDelta TickInterval() const override;

  // BackgrounColorAnimationCurve implementation
  SkColor GetValue(base::TimeDelta t) const override;
  SkColor GetTransformedValue(
      base::TimeDelta t,
      gfx::TimingFunction::LimitDirection limit_direction) const override;

  std::unique_ptr<AnimationCurve> Retarget(base::TimeDelta t,
                                           SkColor new_target);

  using Keyframes = std::vector<std::unique_ptr<ColorKeyframe>>;
  const Keyframes& keyframes() const { return keyframes_; }
  const Keyframes& keyframes_for_testing() const { return keyframes_; }

 private:
  KeyframedColorAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  Keyframes keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT KeyframedFloatAnimationCurve
    : public FloatAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static std::unique_ptr<KeyframedFloatAnimationCurve> Create();

  KeyframedFloatAnimationCurve(const KeyframedFloatAnimationCurve&) = delete;
  ~KeyframedFloatAnimationCurve() override;

  KeyframedFloatAnimationCurve& operator=(const KeyframedFloatAnimationCurve&) =
      delete;

  void AddKeyframe(std::unique_ptr<FloatKeyframe> keyframe);

  const TimingFunction* timing_function() const {
    return timing_function_.get();
  }
  void SetTimingFunction(std::unique_ptr<TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  TimingFunction* timing_function_for_testing() const {
    return timing_function_.get();
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;
  base::TimeDelta TickInterval() const override;

  // FloatAnimationCurve implementation
  float GetValue(base::TimeDelta t) const override;
  float GetTransformedValue(
      base::TimeDelta t,
      gfx::TimingFunction::LimitDirection limit_direction) const override;

  std::unique_ptr<AnimationCurve> Retarget(base::TimeDelta t, float new_target);

  using Keyframes = std::vector<std::unique_ptr<FloatKeyframe>>;
  const Keyframes& keyframes() const { return keyframes_; }
  const Keyframes& keyframes_for_testing() const { return keyframes_; }

 private:
  KeyframedFloatAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  Keyframes keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT KeyframedTransformAnimationCurve
    : public TransformAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static std::unique_ptr<KeyframedTransformAnimationCurve> Create();

  KeyframedTransformAnimationCurve(const KeyframedTransformAnimationCurve&) =
      delete;
  ~KeyframedTransformAnimationCurve() override;

  KeyframedTransformAnimationCurve& operator=(
      const KeyframedTransformAnimationCurve&) = delete;

  void AddKeyframe(std::unique_ptr<TransformKeyframe> keyframe);
  const TimingFunction* timing_function() const {
    return timing_function_.get();
  }
  void SetTimingFunction(std::unique_ptr<TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;
  base::TimeDelta TickInterval() const override;

  // TransformAnimationCurve implementation
  gfx::TransformOperations GetValue(base::TimeDelta t) const override;
  gfx::TransformOperations GetTransformedValue(
      base::TimeDelta t,
      gfx::TimingFunction::LimitDirection limit_direction) const override;
  bool PreservesAxisAlignment() const override;
  bool MaximumScale(float* max_scale) const override;

  std::unique_ptr<AnimationCurve> Retarget(
      base::TimeDelta t,
      const gfx::TransformOperations& new_target);

  using Keyframes = std::vector<std::unique_ptr<TransformKeyframe>>;
  const Keyframes& keyframes() const { return keyframes_; }

 private:
  KeyframedTransformAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  Keyframes keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT KeyframedSizeAnimationCurve
    : public SizeAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static std::unique_ptr<KeyframedSizeAnimationCurve> Create();

  KeyframedSizeAnimationCurve(const KeyframedSizeAnimationCurve&) = delete;
  ~KeyframedSizeAnimationCurve() override;

  KeyframedSizeAnimationCurve& operator=(const KeyframedSizeAnimationCurve&) =
      delete;

  void AddKeyframe(std::unique_ptr<SizeKeyframe> keyframe);
  const TimingFunction* timing_function() const {
    return timing_function_.get();
  }
  void SetTimingFunction(std::unique_ptr<TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;
  base::TimeDelta TickInterval() const override;

  // SizeAnimationCurve implementation
  gfx::SizeF GetValue(base::TimeDelta t) const override;
  gfx::SizeF GetTransformedValue(
      base::TimeDelta t,
      gfx::TimingFunction::LimitDirection limit_direction) const override;

  std::unique_ptr<AnimationCurve> Retarget(base::TimeDelta t,
                                           const gfx::SizeF& new_target);

  using Keyframes = std::vector<std::unique_ptr<SizeKeyframe>>;
  const Keyframes& keyframes() const { return keyframes_; }

 private:
  KeyframedSizeAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  Keyframes keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT KeyframedRectAnimationCurve
    : public RectAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static std::unique_ptr<KeyframedRectAnimationCurve> Create();

  KeyframedRectAnimationCurve(const KeyframedRectAnimationCurve&) = delete;
  ~KeyframedRectAnimationCurve() override;

  KeyframedRectAnimationCurve& operator=(const KeyframedRectAnimationCurve&) =
      delete;

  void AddKeyframe(std::unique_ptr<RectKeyframe> keyframe);
  const TimingFunction* timing_function() const {
    return timing_function_.get();
  }
  void SetTimingFunction(std::unique_ptr<TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;
  base::TimeDelta TickInterval() const override;

  // RectAnimationCurve implementation
  gfx::Rect GetValue(base::TimeDelta t) const override;
  gfx::Rect GetTransformedValue(
      base::TimeDelta t,
      gfx::TimingFunction::LimitDirection limit_direction) const override;

  std::unique_ptr<AnimationCurve> Retarget(base::TimeDelta t,
                                           const gfx::Rect& new_target);

  using Keyframes = std::vector<std::unique_ptr<RectKeyframe>>;
  const Keyframes& keyframes() const { return keyframes_; }

 private:
  KeyframedRectAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  Keyframes keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_ = 0.;
};

template <typename T>
struct AnimationTraits {};

#define DEFINE_ANIMATION_TRAITS(value_type, name)                           \
  template <>                                                               \
  struct AnimationTraits<value_type> {                                      \
    typedef value_type ValueType;                                           \
    typedef name##AnimationCurve::Target TargetType;                        \
    typedef name##AnimationCurve CurveType;                                 \
    typedef Keyframed##name##AnimationCurve KeyframedCurveType;             \
    typedef name##Keyframe KeyframeType;                                    \
    static const CurveType* ToDerivedCurve(const AnimationCurve* curve) {   \
      return name##AnimationCurve::To##name##AnimationCurve(curve);         \
    }                                                                       \
    static CurveType* ToDerivedCurve(AnimationCurve* curve) {               \
      return name##AnimationCurve::To##name##AnimationCurve(curve);         \
    }                                                                       \
    static const KeyframedCurveType* ToKeyframedCurve(                      \
        const AnimationCurve* curve) {                                      \
      return static_cast<const KeyframedCurveType*>(ToDerivedCurve(curve)); \
    }                                                                       \
    static KeyframedCurveType* ToKeyframedCurve(AnimationCurve* curve) {    \
      return static_cast<KeyframedCurveType*>(ToDerivedCurve(curve));       \
    }                                                                       \
    static void OnValueAnimated(name##AnimationCurve::Target* target,       \
                                const ValueType& target_value,              \
                                int target_property) {                      \
      target->On##name##Animated(target_value, target_property, nullptr);   \
    }                                                                       \
  }

DEFINE_ANIMATION_TRAITS(float, Float);
DEFINE_ANIMATION_TRAITS(TransformOperations, Transform);
DEFINE_ANIMATION_TRAITS(SizeF, Size);
DEFINE_ANIMATION_TRAITS(SkColor, Color);
DEFINE_ANIMATION_TRAITS(Rect, Rect);

#undef DEFINE_ANIMATION_TRAITS

bool SufficientlyEqual(float lhs, float rhs);
bool SufficientlyEqual(const TransformOperations& lhs,
                       const TransformOperations& rhs);
bool SufficientlyEqual(const SizeF& lhs, const SizeF& rhs);
bool SufficientlyEqual(SkColor lhs, SkColor rhs);
bool SufficientlyEqual(const Rect& lhs, const Rect& rhs);

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_H_
