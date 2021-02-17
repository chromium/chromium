// Copyright 2012 The Chromium Authors. All rights reserved.
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
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform_operations.h"

namespace gfx {

struct GFX_KEYFRAME_ANIMATION_EXPORT KeyframeTimeValues {
  base::TimeDelta start_time;
  base::TimeDelta duration;
  double progress;
};

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

struct GFX_KEYFRAME_ANIMATION_EXPORT KeyframeAnimationTimeValues {
  base::TimeDelta start_time;
  base::TimeDelta duration;
  double progress;
};

// Calculates commonly used time information for keyframe animations.
GFX_KEYFRAME_ANIMATION_EXPORT
KeyframeAnimationTimeValues ComputeKeyframeAnimationTimeValues(
    base::TimeDelta start_time,
    base::TimeDelta end_time,
    double scaled_duration,
    base::TimeDelta time);

// Remaps the given time according to the timing function, if necessary.
GFX_KEYFRAME_ANIMATION_EXPORT
base::TimeDelta ComputeTransformedAnimationTime(
    base::TimeDelta start_time,
    base::TimeDelta end_time,
    const std::unique_ptr<TimingFunction>& timing_function,
    double scaled_duration,
    base::TimeDelta time);

#define DECLARE_KEYFRAME_BODY(T, Name)                                  \
 public:                                                                \
  static std::unique_ptr<Name##Keyframe> Create(                        \
      base::TimeDelta time, const T& value,                             \
      std::unique_ptr<gfx::TimingFunction> timing_function);            \
  ~Name##Keyframe() override;                                           \
  T Value() const;                                                      \
  std::unique_ptr<Name##Keyframe> Clone() const;                        \
                                                                        \
 private:                                                               \
  Name##Keyframe(base::TimeDelta time, const T& value,                  \
                 std::unique_ptr<gfx::TimingFunction> timing_function); \
  T value_;

class GFX_KEYFRAME_ANIMATION_EXPORT ColorKeyframe : public Keyframe {
  DECLARE_KEYFRAME_BODY(SkColor, Color)
};

class GFX_KEYFRAME_ANIMATION_EXPORT FloatKeyframe : public Keyframe {
  DECLARE_KEYFRAME_BODY(float, Float)
};

class GFX_KEYFRAME_ANIMATION_EXPORT TransformKeyframe : public Keyframe {
  DECLARE_KEYFRAME_BODY(TransformOperations, Transform)
};

class GFX_KEYFRAME_ANIMATION_EXPORT SizeKeyframe : public Keyframe {
  DECLARE_KEYFRAME_BODY(SizeF, Size)
};

#define DECLARE_KEYFRAMED_CURVE_BODY(T, Name)                               \
 public:                                                                    \
  static std::unique_ptr<Keyframed##Name##AnimationCurve> Create();         \
  Keyframed##Name##AnimationCurve(const Keyframed##Name##AnimationCurve&) = \
      delete;                                                               \
  ~Keyframed##Name##AnimationCurve() override;                              \
  Keyframed##Name##AnimationCurve& operator=(                               \
      const Keyframed##Name##AnimationCurve&) = delete;                     \
  void AddKeyframe(std::unique_ptr<Name##Keyframe> keyframe);               \
  void SetTimingFunction(                                                   \
      std::unique_ptr<gfx::TimingFunction> timing_function) {               \
    timing_function_ = std::move(timing_function);                          \
  }                                                                         \
  gfx::TimingFunction* timing_function_for_testing() const {                \
    return timing_function_.get();                                          \
  }                                                                         \
  double scaled_duration() const { return scaled_duration_; }               \
  void set_scaled_duration(double scaled_duration) {                        \
    scaled_duration_ = scaled_duration;                                     \
  }                                                                         \
  base::TimeDelta Duration() const override;                                \
  std::unique_ptr<AnimationCurve> Clone() const override;                   \
  T GetValue(base::TimeDelta t) const override;                             \
  using Keyframes = std::vector<std::unique_ptr<Name##Keyframe>>;           \
  const Keyframes& keyframes_for_testing() const { return keyframes_; }     \
                                                                            \
 private:                                                                   \
  Keyframed##Name##AnimationCurve();                                        \
  double TransformedKeyframeProgress(double scaled_duration,                \
                                     base::TimeDelta time, size_t i);       \
  Keyframes keyframes_;                                                     \
  std::unique_ptr<gfx::TimingFunction> timing_function_;                    \
  double scaled_duration_;

class GFX_KEYFRAME_ANIMATION_EXPORT KeyframedColorAnimationCurve
    : public ColorAnimationCurve {
  DECLARE_KEYFRAMED_CURVE_BODY(SkColor, Color)
};

class GFX_KEYFRAME_ANIMATION_EXPORT KeyframedFloatAnimationCurve
    : public FloatAnimationCurve {
  DECLARE_KEYFRAMED_CURVE_BODY(float, Float)
};

class GFX_KEYFRAME_ANIMATION_EXPORT KeyframedSizeAnimationCurve
    : public SizeAnimationCurve {
  DECLARE_KEYFRAMED_CURVE_BODY(SizeF, Size)
};

class GFX_KEYFRAME_ANIMATION_EXPORT KeyframedTransformAnimationCurve
    : public TransformAnimationCurve {
 public:
  bool PreservesAxisAlignment() const override;
  bool MaximumScale(float* max_scale) const override;
  DECLARE_KEYFRAMED_CURVE_BODY(TransformOperations, Transform)
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_H_
