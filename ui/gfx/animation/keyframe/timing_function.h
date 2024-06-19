// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_TIMING_FUNCTION_H_
#define UI_GFX_ANIMATION_KEYFRAME_TIMING_FUNCTION_H_

#include <memory>
#include <vector>

#include "ui/gfx/animation/keyframe/keyframe_animation_export.h"
#include "ui/gfx/geometry/cubic_bezier.h"

namespace gfx {

// See http://www.w3.org/TR/css3-transitions/.
class GFX_KEYFRAME_ANIMATION_EXPORT TimingFunction {
 public:
  virtual ~TimingFunction();

  TimingFunction& operator=(const TimingFunction&) = delete;

  // Note that LINEAR is a nullptr TimingFunction (for now).
  enum class Type { LINEAR, CUBIC_BEZIER, STEPS };

  // Which limit to apply at a discontinuous boundary.
  // See https://drafts.csswg.org/css-easing/#step-easing-algo
  enum class LimitDirection { LEFT, RIGHT };

  virtual Type GetType() const = 0;
  virtual double GetValue(double t, LimitDirection limit_direction) const = 0;
  virtual double Velocity(double time) const = 0;
  virtual std::unique_ptr<TimingFunction> Clone() const = 0;

 protected:
  TimingFunction();
};

class GFX_KEYFRAME_ANIMATION_EXPORT CubicBezierTimingFunction
    : public TimingFunction {
 public:
  enum class EaseType { EASE, EASE_IN, EASE_OUT, EASE_IN_OUT, CUSTOM };

  static std::unique_ptr<CubicBezierTimingFunction> CreatePreset(
      EaseType ease_type);
  static std::unique_ptr<CubicBezierTimingFunction> Create(double x1,
                                                           double y1,
                                                           double x2,
                                                           double y2);
  ~CubicBezierTimingFunction() override;

  CubicBezierTimingFunction& operator=(const CubicBezierTimingFunction&) =
      delete;

  // TimingFunction implementation.
  Type GetType() const override;
  double GetValue(
      double time,
      LimitDirection limit_direction = LimitDirection::RIGHT) const override;
  double Velocity(double time) const override;
  std::unique_ptr<TimingFunction> Clone() const override;

  EaseType ease_type() const { return ease_type_; }
  const gfx::CubicBezier& bezier() const { return bezier_; }

 private:
  CubicBezierTimingFunction(EaseType ease_type,
                            double x1,
                            double y1,
                            double x2,
                            double y2);

  gfx::CubicBezier bezier_;
  EaseType ease_type_;
};

class GFX_KEYFRAME_ANIMATION_EXPORT StepsTimingFunction
    : public TimingFunction {
 public:
  // step-timing-function values
  // https://drafts.csswg.org/css-easing-1/#typedef-step-timing-function
  enum class StepPosition {
    START,      // Discontinuity at progress = 0.
                // Alias for jump-start. Maintaining a separate enumerated value
                // for serialization.
    END,        // Discontinuity at progress = 1.
                // Alias for jump-end. Maintaining a separate enumerated value
                // for serialization.
    JUMP_BOTH,  // Discontinuities at progress = 0 and 1.
    JUMP_END,   // Discontinuity at progress = 1.
    JUMP_NONE,  // Continuous at progress = 0 and 1.
    JUMP_START  // Discontinuity at progress = 0.
  };

  static std::unique_ptr<StepsTimingFunction> Create(
      int steps,
      StepPosition step_position);
  ~StepsTimingFunction() override;

  StepsTimingFunction& operator=(const StepsTimingFunction&) = delete;

  // TimingFunction implementation.
  Type GetType() const override;
  double GetValue(double t, LimitDirection limit_direction) const override;
  std::unique_ptr<TimingFunction> Clone() const override;
  double Velocity(double time) const override;

  int steps() const { return steps_; }
  StepPosition step_position() const { return step_position_; }

 private:
  StepsTimingFunction(int steps, StepPosition step_position);

  // The number of jumps is the number of discontinuities in the timing
  // function. There is a subtle distinction between the number of steps and
  // jumps. The number of steps is the number of intervals in the timing
  // function. The number of jumps differs from the number of steps when either
  // both or neither end point has a discontinuity.
  // https://drafts.csswg.org/css-easing-1/#step-easing-functions
  int NumberOfJumps() const;

  float GetStepsStartOffset() const;

  int steps_;
  StepPosition step_position_;
};

struct GFX_KEYFRAME_ANIMATION_EXPORT LinearEasingPoint {
  double input;
  double output;

  LinearEasingPoint() = default;
  LinearEasingPoint(double input, double output) {
    this->input = input;
    this->output = output;
  }

  bool operator==(const LinearEasingPoint& other) const {
    return input == other.input && output == other.output;
  }
  bool operator!=(const LinearEasingPoint& other) const {
    return !(*this == other);
  }
};

class GFX_KEYFRAME_ANIMATION_EXPORT LinearTimingFunction
    : public TimingFunction {
 public:
  static std::unique_ptr<LinearTimingFunction> Create();
  static std::unique_ptr<LinearTimingFunction> Create(
      std::vector<LinearEasingPoint> points);

  LinearTimingFunction& operator=(const LinearTimingFunction&) = delete;
  ~LinearTimingFunction() override;

  // TimingFunction implementation.
  Type GetType() const override;
  double GetValue(
      double t,
      LimitDirection limit_direction = LimitDirection::RIGHT) const override;
  std::unique_ptr<TimingFunction> Clone() const override;
  double Velocity(double time) const override;

  const LinearEasingPoint& Point(size_t i) const { return points_[i]; }
  const std::vector<LinearEasingPoint>& Points() const { return points_; }
  bool IsTrivial() const { return !points_.size(); }

 private:
  LinearTimingFunction();
  explicit LinearTimingFunction(std::vector<LinearEasingPoint> points);
  LinearTimingFunction(const LinearTimingFunction&);
  std::vector<LinearEasingPoint> points_;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_KEYFRAME_TIMING_FUNCTION_H_
