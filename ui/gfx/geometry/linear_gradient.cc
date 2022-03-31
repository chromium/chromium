// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/linear_gradient.h"

#include <sstream>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/angle_conversions.h"
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

void LinearGradient::AddStep(float percent, uint8_t alpha) {
  DCHECK_LT(step_count_, kMaxStepSize);
  DCHECK_GE(percent, 0);
  DCHECK_LE(percent, 100);
  // make sure the step's percent is monotonically increasing.
  DCHECK(step_count_ ? steps_[step_count_ - 1].percent < percent : true)
      << base::StringPrintf("prev[%zu]=%f, next[%zu]=%f", step_count_ - 1,
                            steps_[step_count_ - 1].percent, step_count_,
                            steps_[step_count_].percent);
  steps_[step_count_].percent = percent;
  steps_[step_count_++].alpha = alpha;
}

void LinearGradient::ReverseSteps() {
  std::reverse(steps_.begin(), steps_.end());
  std::rotate(steps_.begin(), steps_.end() - step_count_, steps_.end());
  for (size_t i = 0; i < step_count_; i++)
    steps_[i].percent = 100.f - steps_[i].percent;
}

void LinearGradient::Transform(const gfx::Transform& transform) {
  gfx::PointF origin, end;
  float radian = gfx::DegToRad(static_cast<float>(angle_));
  float y = -sin(radian);
  float x = cos(radian);
  end.Offset(x, y);
  transform.TransformPoint(&origin);
  transform.TransformPoint(&end);
  gfx::Vector2dF diff = end - origin;
  int16_t new_angle =
      -static_cast<int16_t>(gfx::RadToDeg(atan2(diff.y(), diff.x())));
  angle_ = new_angle;
}

std::string LinearGradient::ToString() const {
  std::string result = base::StringPrintf(
      "LinearGradient{angle=%u, step_count=%zu [", angle_, step_count_);
  for (size_t i = 0; i < step_count_; i++) {
    if (i)
      result += " - ";
    result += base::NumberToString(steps_[i].percent) + ":" +
              base::NumberToString(static_cast<int>(steps_[i].alpha));
  }
  return result + "]}";
}

}  // namespace gfx
