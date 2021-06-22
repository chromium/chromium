// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/point_f.h"

#include <cmath>

#include "base/check.h"
#include "base/strings/stringprintf.h"

namespace gfx {

void PointF::SetToMin(const PointF& other) {
  x_ = x_ <= other.x_ ? x_ : other.x_;
  y_ = y_ <= other.y_ ? y_ : other.y_;
}

void PointF::SetToMax(const PointF& other) {
  x_ = x_ >= other.x_ ? x_ : other.x_;
  y_ = y_ >= other.y_ ? y_ : other.y_;
}

bool PointF::IsWithinDistance(const PointF& rhs,
                              const float allowed_distance) const {
  DCHECK(allowed_distance > 0);
  float diff_x = x_ - rhs.x();
  float diff_y = y_ - rhs.y();
  float distance = std::sqrt(diff_x * diff_x + diff_y * diff_y);
  return distance < allowed_distance;
}

std::string PointF::ToString() const {
  return base::StringPrintf("%f,%f", x(), y());
}

PointF ScalePoint(const PointF& p, float x_scale, float y_scale) {
  PointF scaled_p(p);
  scaled_p.Scale(x_scale, y_scale);
  return scaled_p;
}


}  // namespace gfx
