// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/linear_gradient.h"

#include <sstream>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/numerics/angle_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {

// static
LinearGradient& LinearGradient::GetEmpty() {
  static LinearGradient kEmpty;
  return kEmpty;
}

LinearGradient::LinearGradient() = default;

LinearGradient::LinearGradient(int16_t angle) : angle_(angle) {}

LinearGradient::LinearGradient(const LinearGradient& copy) = default;

void LinearGradient::AddStep(float fraction, uint8_t alpha) {
  DCHECK_LT(step_count_, kMaxStepSize);
  DCHECK_GE(fraction, 0);
  DCHECK_LE(fraction, 1);
  // make sure the step's fraction is monotonically increasing.
  DCHECK(step_count_ ? steps_[step_count_ - 1].fraction < fraction : true)
      << base::StringPrintf("prev[%zu]=%f, next[%zu]=%f", step_count_ - 1,
                            steps_[step_count_ - 1].fraction, step_count_,
                            fraction);
  steps_[step_count_].fraction = fraction;
  steps_[step_count_++].alpha = alpha;
}

void LinearGradient::ReverseSteps() {
  std::reverse(steps_.begin(), steps_.end());
  std::rotate(steps_.begin(), steps_.end() - step_count_, steps_.end());
  for (size_t i = 0; i < step_count_; i++)
    steps_[i].fraction = 1.f - steps_[i].fraction;
}

void LinearGradient::ApplyTransform(const Transform& transform) {
  if (transform.IsIdentityOrTranslation())
    return;

  float radian = base::DegToRad(static_cast<float>(angle_));
  float y = -sin(radian);
  float x = cos(radian);
  PointF origin = transform.MapPoint(PointF());
  PointF end = transform.MapPoint(PointF(x, y));
  Vector2dF diff = end - origin;
  float new_angle = base::RadToDeg(atan2(diff.y(), diff.x()));
  angle_ = -static_cast<int16_t>(std::round(new_angle));
}

void LinearGradient::ApplyTransform(const AxisTransform2d& transform) {
  if (transform.scale().x() == transform.scale().y())
    return;

  float radian = base::DegToRad(static_cast<float>(angle_));
  float y = -sin(radian) * transform.scale().y();
  float x = cos(radian) * transform.scale().x();
  float new_angle = base::RadToDeg(atan2(y, x));
  angle_ = -static_cast<int16_t>(std::round(new_angle));
}

std::string LinearGradient::ToString() const {
  std::string result = base::StringPrintf(
      "LinearGradient{angle=%d, step_count=%zu [", angle_, step_count_);
  for (size_t i = 0; i < step_count_; ++i) {
    if (i)
      result += " - ";
    result += base::StringPrintf("%f:%u", steps_[i].fraction, steps_[i].alpha);
  }
  return result + "]}";
}

}  // namespace gfx
