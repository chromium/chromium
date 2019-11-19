// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/quaternion.h"

#include <algorithm>
#include <cmath>

#include "base/numerics/math_constants.h"
#include "base/numerics/ranges.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {

namespace {

const double kEpsilon = 1e-5;

}  // namespace

Quaternion::Quaternion(const Vector3dF& axis, double theta) {
  // Rotation angle is the product of |angle| and the magnitude of |axis|.
  double length = axis.Length();
  if (std::abs(length) < kEpsilon)
    return;

  Vector3dF normalized = axis;
  normalized.Scale(1.0 / length);

  theta *= 0.5;
  double s = sin(theta);
  x_ = normalized.x() * s;
  y_ = normalized.y() * s;
  z_ = normalized.z() * s;
  w_ = cos(theta);
}

Quaternion::Quaternion(const Vector3dF& from, const Vector3dF& to) {
  double dot = gfx::DotProduct(from, to);
  double norm = sqrt(from.LengthSquared() * to.LengthSquared());
  double real = norm + dot;
  gfx::Vector3dF axis;
  if (real < kEpsilon * norm) {
    real = 0.0f;
    axis = std::abs(from.x()) > std::abs(from.z())
               ? gfx::Vector3dF{-from.y(), from.x(), 0.0}
               : gfx::Vector3dF{0.0, -from.z(), from.y()};
  } else {
    axis = gfx::CrossProduct(from, to);
  }
  x_ = axis.x();
  y_ = axis.y();
  z_ = axis.z();
  w_ = real;
  *this = this->Normalized();
}

// Taken from http://www.w3.org/TR/css3-transforms/.
Quaternion Quaternion::Slerp(const Quaternion& q, double t) const {
  double dot = x_ * q.x_ + y_ * q.y_ + z_ * q.z_ + w_ * q.w_;

  dot = base::ClampToRange(dot, -1.0, 1.0);

  // Quaternions are facing the same direction.
  if (std::abs(dot - 1.0) < kEpsilon || std::abs(dot + 1.0) < kEpsilon)
    return *this;

  double denom = std::sqrt(1.0 - dot * dot);
  double theta = std::acos(dot);
  double w = std::sin(t * theta) * (1.0 / denom);

  double s1 = std::cos(t * theta) - dot * w;
  double s2 = w;

  return (s1 * *this) + (s2 * q);
}

Quaternion Quaternion::Lerp(const Quaternion& q, double t) const {
  return (((1.0 - t) * *this) + (t * q)).Normalized();
}

double Quaternion::Length() const {
  return x_ * x_ + y_ * y_ + z_ * z_ + w_ * w_;
}

Quaternion Quaternion::Normalized() const {
  double length = Length();
  if (length < kEpsilon)
    return *this;
  return *this / sqrt(length);
}

std::string Quaternion::ToString() const {
  // q = (con(abs(v_theta)/2), v_theta/abs(v_theta) * sin(abs(v_theta)/2))
  float abs_theta = acos(w_) * 2;
  float scale = 1. / sin(abs_theta * .5);
  gfx::Vector3dF v(x_, y_, z_);
  v.Scale(scale);
  return base::StringPrintf("[%f %f %f %f], v:", x_, y_, z_, w_) +
         v.ToString() +
         base::StringPrintf(", θ:%fπ", abs_theta / base::kPiFloat);
}

}  // namespace gfx
