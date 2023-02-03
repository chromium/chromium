// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_POINT_F_H_
#define UI_GFX_GEOMETRY_POINT_F_H_

#include <iosfwd>
#include <string>
#include <tuple>

#include "build/build_config.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if BUILDFLAG(IS_APPLE)
struct CGPoint;
#endif

namespace perfetto {
class TracedValue;
}
namespace gfx {

// A floating version of gfx::Point.
class GEOMETRY_EXPORT PointF {
 public:
  constexpr PointF() : x_(0.f), y_(0.f) {}
  constexpr PointF(float x, float y) : x_(x), y_(y) {}

  constexpr explicit PointF(const Point& p)
      : PointF(static_cast<float>(p.x()), static_cast<float>(p.y())) {}

#if BUILDFLAG(IS_APPLE)
  explicit PointF(const CGPoint&);
  CGPoint ToCGPoint() const;
#endif

  constexpr float x() const { return x_; }
  constexpr float y() const { return y_; }
  void set_x(float x) { x_ = x; }
  void set_y(float y) { y_ = y; }

  void SetPoint(float x, float y) {
    x_ = x;
    y_ = y;
  }

  void Offset(float delta_x, float delta_y) {
    x_ += delta_x;
    y_ += delta_y;
  }

  constexpr void operator+=(const Vector2dF& vector) {
    x_ += vector.x();
    y_ += vector.y();
  }

  constexpr void operator-=(const Vector2dF& vector) {
    x_ -= vector.x();
    y_ -= vector.y();
  }

  void SetToMin(const PointF& other);
  void SetToMax(const PointF& other);

  bool IsOrigin() const { return x_ == 0 && y_ == 0; }

  constexpr Vector2dF OffsetFromOrigin() const { return Vector2dF(x_, y_); }

  // A point is less than another point if its y-value is closer
  // to the origin. If the y-values are the same, then point with
  // the x-value closer to the origin is considered less than the
  // other.
  // This comparison is required to use PointF in sets, or sorted
  // vectors.
  bool operator<(const PointF& rhs) const {
    return std::tie(y_, x_) < std::tie(rhs.y_, rhs.x_);
  }

  void Scale(float scale) {
    Scale(scale, scale);
  }

  void Scale(float x_scale, float y_scale) {
    SetPoint(x() * x_scale, y() * y_scale);
  }

  // Scales the point by the inverse of the given scale.
  void InvScale(float inv_scale) { InvScale(inv_scale, inv_scale); }

  // Scales each component by the inverse of the given scales.
  void InvScale(float inv_x_scale, float inv_y_scale) {
    x_ /= inv_x_scale;
    y_ /= inv_y_scale;
  }

  void Transpose() {
    using std::swap;
    swap(x_, y_);
  }

  // Uses the Pythagorean theorem to determine the straight line distance
  // between the two points, and returns true if it is less than
  // |allowed_distance|.
  bool IsWithinDistance(const PointF& rhs, const float allowed_distance) const;

  // Returns a string representation of point.
  std::string ToString() const;

  // Write a represtation of this object into a trace event argument.
  void WriteIntoTrace(perfetto::TracedValue) const;

 private:
  float x_;
  float y_;
};

constexpr bool operator==(const PointF& lhs, const PointF& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

constexpr bool operator!=(const PointF& lhs, const PointF& rhs) {
  return !(lhs == rhs);
}

constexpr PointF operator+(const PointF& lhs, const Vector2dF& rhs) {
  PointF result(lhs);
  result += rhs;
  return result;
}

constexpr PointF operator-(const PointF& lhs, const Vector2dF& rhs) {
  PointF result(lhs);
  result -= rhs;
  return result;
}

inline Vector2dF operator-(const PointF& lhs, const PointF& rhs) {
  return Vector2dF(lhs.x() - rhs.x(), lhs.y() - rhs.y());
}

inline PointF PointAtOffsetFromOrigin(const Vector2dF& offset_from_origin) {
  return PointF(offset_from_origin.x(), offset_from_origin.y());
}

GEOMETRY_EXPORT PointF ScalePoint(const PointF& p,
                                  float x_scale,
                                  float y_scale);

inline PointF ScalePoint(const PointF& p, float scale) {
  return ScalePoint(p, scale, scale);
}

inline PointF TransposePoint(const PointF& p) {
  return PointF(p.y(), p.x());
}

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const PointF& point, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_POINT_F_H_
