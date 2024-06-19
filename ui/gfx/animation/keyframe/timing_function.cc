// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/timing_function.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"

namespace gfx {

TimingFunction::TimingFunction() = default;

TimingFunction::~TimingFunction() = default;

std::unique_ptr<CubicBezierTimingFunction>
CubicBezierTimingFunction::CreatePreset(EaseType ease_type) {
  // These numbers come from
  // http://www.w3.org/TR/css3-transitions/#transition-timing-function_tag.
  switch (ease_type) {
    case EaseType::EASE:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.25, 0.1, 0.25, 1.0));
    case EaseType::EASE_IN:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.42, 0.0, 1.0, 1.0));
    case EaseType::EASE_OUT:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.0, 0.0, 0.58, 1.0));
    case EaseType::EASE_IN_OUT:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.42, 0.0, 0.58, 1));
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}
std::unique_ptr<CubicBezierTimingFunction>
CubicBezierTimingFunction::Create(double x1, double y1, double x2, double y2) {
  return base::WrapUnique(
      new CubicBezierTimingFunction(EaseType::CUSTOM, x1, y1, x2, y2));
}

CubicBezierTimingFunction::CubicBezierTimingFunction(EaseType ease_type,
                                                     double x1,
                                                     double y1,
                                                     double x2,
                                                     double y2)
    : bezier_(x1, y1, x2, y2), ease_type_(ease_type) {}

CubicBezierTimingFunction::~CubicBezierTimingFunction() = default;

TimingFunction::Type CubicBezierTimingFunction::GetType() const {
  return Type::CUBIC_BEZIER;
}

double CubicBezierTimingFunction::GetValue(
    double x,
    TimingFunction::LimitDirection) const {
  return bezier_.Solve(x);
}

double CubicBezierTimingFunction::Velocity(double x) const {
  return bezier_.Slope(x);
}

std::unique_ptr<TimingFunction> CubicBezierTimingFunction::Clone() const {
  return base::WrapUnique(new CubicBezierTimingFunction(*this));
}

std::unique_ptr<StepsTimingFunction> StepsTimingFunction::Create(
    int steps,
    StepPosition step_position) {
  return base::WrapUnique(new StepsTimingFunction(steps, step_position));
}

StepsTimingFunction::StepsTimingFunction(int steps, StepPosition step_position)
    : steps_(steps), step_position_(step_position) {}

StepsTimingFunction::~StepsTimingFunction() = default;

TimingFunction::Type StepsTimingFunction::GetType() const {
  return Type::STEPS;
}

std::unique_ptr<TimingFunction> StepsTimingFunction::Clone() const {
  return base::WrapUnique(new StepsTimingFunction(*this));
}

double StepsTimingFunction::Velocity(double x) const {
  return 0;
}

double StepsTimingFunction::GetValue(double t, LimitDirection direction) const {
  const double steps = static_cast<double>(steps_);
  double current_step = std::floor((steps * t) + GetStepsStartOffset());
  // Adjust step if using a left limit at a discontinuous step boundary.
  if (direction == LimitDirection::LEFT &&
      steps * t - std::floor(steps * t) == 0) {
    current_step -= 1;
  }
  // Jumps may differ from steps based on the number of end-point
  // discontinuities, which may be 0, 1 or 2.
  int jumps = NumberOfJumps();
  if (t >= 0 && current_step < 0)
    current_step = 0;
  if (t <= 1 && current_step > jumps)
    current_step = jumps;
  return current_step / jumps;
}

int StepsTimingFunction::NumberOfJumps() const {
  switch (step_position_) {
    case StepPosition::END:
    case StepPosition::START:
    case StepPosition::JUMP_END:
    case StepPosition::JUMP_START:
      return steps_;

    case StepPosition::JUMP_BOTH:
      return steps_ < std::numeric_limits<int>::max() ? steps_ + 1 : steps_;

    case StepPosition::JUMP_NONE:
      DCHECK_GT(steps_, 1);
      return steps_ - 1;

    default:
      NOTREACHED_IN_MIGRATION();
      return steps_;
  }
}

float StepsTimingFunction::GetStepsStartOffset() const {
  switch (step_position_) {
    case StepPosition::JUMP_BOTH:
    case StepPosition::JUMP_START:
    case StepPosition::START:
      return 1;

    case StepPosition::JUMP_END:
    case StepPosition::JUMP_NONE:
    case StepPosition::END:
      return 0;

    default:
      NOTREACHED_IN_MIGRATION();
      return 1;
  }
}

LinearTimingFunction::LinearTimingFunction() = default;

LinearTimingFunction::LinearTimingFunction(
    std::vector<LinearEasingPoint> points)
    : points_(std::move(points)) {}

LinearTimingFunction::~LinearTimingFunction() = default;

LinearTimingFunction::LinearTimingFunction(const LinearTimingFunction& other) {
  points_ = other.points_;
}

std::unique_ptr<LinearTimingFunction> LinearTimingFunction::Create() {
  return base::WrapUnique(new LinearTimingFunction());
}

std::unique_ptr<LinearTimingFunction> LinearTimingFunction::Create(
    std::vector<LinearEasingPoint> points) {
  DCHECK(points.size() >= 2);
  return base::WrapUnique(new LinearTimingFunction(std::move(points)));
}

TimingFunction::Type LinearTimingFunction::GetType() const {
  return Type::LINEAR;
}

std::unique_ptr<TimingFunction> LinearTimingFunction::Clone() const {
  return base::WrapUnique(new LinearTimingFunction(*this));
}

double LinearTimingFunction::Velocity(double x) const {
  return 0;
}

double LinearTimingFunction::GetValue(double input_progress,
                                      LimitDirection limit_direction) const {
  if (IsTrivial()) {
    return input_progress;
  }
  // https://w3c.github.io/csswg-drafts/css-easing/#linear-easing-function-output
  // 1. Let points be linearEasingFunction’s points.
  // 2. Let pointAIndex be index of the last item in points with an input
  // less than or equal to inputProgress, or 0 if there is no match.
  auto it = std::upper_bound(points_.cbegin(), points_.cend(), input_progress,
                             [](double progress, const auto& point) {
                               return 100 * progress < point.input;
                             });
  it = it == points_.cend() ? std::prev(it) : it;
  auto point_a = it == points_.cbegin() ? it : std::prev(it);
  // 3. If pointAIndex is equal to points size minus 1, decrement pointAIndex
  // by 1.
  point_a = std::next(point_a) == points_.cend() ? std::prev(point_a) : point_a;
  // 4. Let pointA be points[pointAIndex].
  // 5. Let pointB be points[pointAIndex + 1].
  const auto& point_b = std::next(point_a);
  // 6. If pointA’s input is equal to pointB’s input, return pointB’s output.
  if (point_a->input == point_b->input) {
    return point_b->output;
  }
  // 7. Let progressFromPointA be inputProgress minus pointA’s input.
  const double progress_from_point_a = input_progress - point_a->input / 100;
  // 8. Let pointInputRange be pointB’s input minus pointA’s input.
  const double point_input_range = (point_b->input - point_a->input) / 100;
  // 9. Let progressBetweenPoints be progressFromPointA divided by
  // pointInputRange.
  const double progress_between_points =
      progress_from_point_a / point_input_range;
  // 10. Let pointOutputRange be pointB’s output minus pointA’s output.
  const double point_output_range = point_b->output - point_a->output;
  // 11. Let outputFromLastPoint be progressBetweenPoints multiplied by
  // pointOutputRange.
  const double output_from_last_point =
      progress_between_points * point_output_range;
  // 12. Return pointA’s output plus outputFromLastPoint.
  return point_a->output + output_from_last_point;
}

}  // namespace gfx
