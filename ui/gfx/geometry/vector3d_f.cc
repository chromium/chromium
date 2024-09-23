// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/vector3d_f.h"

#include <cmath>

#include "base/numerics/angle_conversions.h"
#include "base/strings/stringprintf.h"

namespace {
const double kEpsilon = 1.0e-6;
}

namespace gfx {

std::string Vector3dF::ToString() const {
  return base::StringPrintf("[%g %g %g]", x_, y_, z_);
}

bool Vector3dF::IsZero() const {
  return x_ == 0 && y_ == 0 && z_ == 0;
}

void Vector3dF::Add(const Vector3dF& other) {
  x_ += other.x_;
  y_ += other.y_;
  z_ += other.z_;
}

void Vector3dF::Subtract(const Vector3dF& other) {
  x_ -= other.x_;
  y_ -= other.y_;
  z_ -= other.z_;
}

double Vector3dF::LengthSquared() const {
  return static_cast<double>(x_) * x_ + static_cast<double>(y_) * y_ +
      static_cast<double>(z_) * z_;
}

float Vector3dF::Length() const {
  return static_cast<float>(std::sqrt(LengthSquared()));
}

void Vector3dF::Scale(float x_scale, float y_scale, float z_scale) {
  x_ *= x_scale;
  y_ *= y_scale;
  z_ *= z_scale;
}

void Vector3dF::InvScale(float inv_x_scale,
                         float inv_y_scale,
                         float inv_z_scale) {
  x_ /= inv_x_scale;
  y_ /= inv_y_scale;
  z_ /= inv_z_scale;
}

void Vector3dF::Cross(const Vector3dF& other) {
  double dx = x_;
  double dy = y_;
  double dz = z_;
  float x = static_cast<float>(dy * other.z() - dz * other.y());
  float y = static_cast<float>(dz * other.x() - dx * other.z());
  float z = static_cast<float>(dx * other.y() - dy * other.x());
  x_ = x;
  y_ = y;
  z_ = z;
}

bool Vector3dF::GetNormalized(Vector3dF* out) const {
  double length_squared = LengthSquared();
  *out = *this;
  if (length_squared < kEpsilon * kEpsilon)
    return false;
  out->InvScale(sqrt(length_squared));
  return true;
}

float DotProduct(const Vector3dF& lhs, const Vector3dF& rhs) {
  return lhs.x() * rhs.x() + lhs.y() * rhs.y() + lhs.z() * rhs.z();
}

Vector3dF ScaleVector3d(const Vector3dF& v,
                        float x_scale,
                        float y_scale,
                        float z_scale) {
  Vector3dF scaled_v(v);
  scaled_v.Scale(x_scale, y_scale, z_scale);
  return scaled_v;
}

float AngleBetweenVectorsInDegrees(const gfx::Vector3dF& base,
                                   const gfx::Vector3dF& other) {
  // Clamp the resulting value to prevent potential NANs from floating point
  // precision issues.
  return base::RadToDeg(std::acos(fmax(
      fmin(gfx::DotProduct(base, other) / base.Length() / other.Length(), 1.f),
      -1.f)));
}

float ClockwiseAngleBetweenVectorsInDegrees(const gfx::Vector3dF& base,
                                            const gfx::Vector3dF& other,
                                            const gfx::Vector3dF& normal) {
  float angle = AngleBetweenVectorsInDegrees(base, other);
  gfx::Vector3dF cross(base);
  cross.Cross(other);

  // If the dot product of this cross product is normal, it means that the
  // shortest angle between |base| and |other| was counterclockwise with respect
  // to the surface represented by |normal| and this angle must be reversed.
  if (gfx::DotProduct(cross, normal) > 0.0f)
    angle = 360.0f - angle;
  return angle;
}

}  // namespace gfx
