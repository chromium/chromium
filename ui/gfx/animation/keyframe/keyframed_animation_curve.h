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

  using Keyframes = std::vector<std::unique_ptr<ColorKeyframe>>;
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

  using Keyframes = std::vector<std::unique_ptr<FloatKeyframe>>;
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
  bool PreservesAxisAlignment() const override;
  bool MaximumScale(float* max_scale) const override;

 private:
  KeyframedTransformAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  std::vector<std::unique_ptr<TransformKeyframe>> keyframes_;
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

 private:
  KeyframedSizeAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  std::vector<std::unique_ptr<SizeKeyframe>> keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_H_
