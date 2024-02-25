// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/vector2d_f.h"

#include <cmath>

#include "base/strings/stringprintf.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"

namespace gfx {

std::string Vector2dF::ToString() const {
  return base::StringPrintf("[%g %g]", x_, y_);
}

void Vector2dF::WriteIntoTrace(perfetto::TracedValue ctx) const {
  perfetto::TracedDictionary dict = std::move(ctx).WriteDictionary();
  dict.Add("x", x_);
  dict.Add("y", y_);
}

bool Vector2dF::IsZero() const {
  return x_ == 0 && y_ == 0;
}

void Vector2dF::Add(const Vector2dF& other) {
  x_ += other.x_;
  y_ += other.y_;
}

void Vector2dF::Subtract(const Vector2dF& other) {
  x_ -= other.x_;
  y_ -= other.y_;
}

double Vector2dF::LengthSquared() const {
  return static_cast<double>(x_) * x_ + static_cast<double>(y_) * y_;
}

float Vector2dF::Length() const {
  return hypotf(x_, y_);
}

void Vector2dF::Scale(float x_scale, float y_scale) {
  x_ *= x_scale;
  y_ *= y_scale;
}

void Vector2dF::InvScale(float inv_x_scale, float inv_y_scale) {
  x_ /= inv_x_scale;
  y_ /= inv_y_scale;
}

double CrossProduct(const Vector2dF& lhs, const Vector2dF& rhs) {
  return static_cast<double>(lhs.x()) * rhs.y() -
      static_cast<double>(lhs.y()) * rhs.x();
}

double DotProduct(const Vector2dF& lhs, const Vector2dF& rhs) {
  return static_cast<double>(lhs.x()) * rhs.x() +
      static_cast<double>(lhs.y()) * rhs.y();
}

Vector2dF ScaleVector2d(const Vector2dF& v, float x_scale, float y_scale) {
  Vector2dF scaled_v(v);
  scaled_v.Scale(x_scale, y_scale);
  return scaled_v;
}

float Vector2dF::SlopeAngleRadians() const {
#if BUILDFLAG(IS_APPLE)
  // atan2f(...) returns less accurate results on Mac.
  // 3.1415925 vs. 3.14159274 for atan2f(0, -50) as an example.
  return static_cast<float>(
      atan2(static_cast<double>(y_), static_cast<double>(x_)));
#else
  return atan2f(y_, x_);
#endif
}

}  // namespace gfx
