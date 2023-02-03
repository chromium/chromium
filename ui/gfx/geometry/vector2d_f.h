// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a simple float vector class.  This class is used to indicate a
// distance in two dimensions between two points. Subtracting two points should
// produce a vector, and adding a vector to a point produces the point at the
// vector's distance from the original point.

#ifndef UI_GFX_GEOMETRY_VECTOR2D_F_H_
#define UI_GFX_GEOMETRY_VECTOR2D_F_H_

#include <iosfwd>
#include <string>

#include "ui/gfx/geometry/geometry_export.h"

namespace perfetto {
class TracedValue;
}
namespace gfx {

class GEOMETRY_EXPORT Vector2dF {
 public:
  constexpr Vector2dF() : x_(0), y_(0) {}
  constexpr Vector2dF(float x, float y) : x_(x), y_(y) {}

  constexpr float x() const { return x_; }
  void set_x(float x) { x_ = x; }

  constexpr float y() const { return y_; }
  void set_y(float y) { y_ = y; }

  // True if both components of the vector are 0.
  bool IsZero() const;

  // Add the components of the |other| vector to the current vector.
  void Add(const Vector2dF& other);
  // Subtract the components of the |other| vector from the current vector.
  void Subtract(const Vector2dF& other);

  void operator+=(const Vector2dF& other) { Add(other); }
  void operator-=(const Vector2dF& other) { Subtract(other); }

  void SetToMin(const Vector2dF& other) {
    x_ = std::min(x_, other.x_);
    y_ = std::min(y_, other.y_);
  }

  void SetToMax(const Vector2dF& other) {
    x_ = std::max(x_, other.x_);
    y_ = std::max(y_, other.y_);
  }

  // Gives the square of the diagonal length, i.e. the square of magnitude, of
  // the vector.
  double LengthSquared() const;

  // Gives the diagonal length (i.e. the magnitude) of the vector.
  float Length() const;

  float AspectRatio() const { return x_ / y_; }

  // Gives the slope angle in radians of the vector from the positive x axis,
  // in the range of (-pi, pi]. The sign of the result is the same as the sign
  // of y(), except that the result is pi for Vector2dF(negative-x, zero-y).
  float SlopeAngleRadians() const;

  // Scale the x and y components of the vector by |scale|.
  void Scale(float scale) { Scale(scale, scale); }
  // Scale the x and y components of the vector by |x_scale| and |y_scale|
  // respectively.
  void Scale(float x_scale, float y_scale);

  // Divides all components of the vector by |scale|.
  void InvScale(float inv_scale) { InvScale(inv_scale, inv_scale); }
  // Divides each component of the vector by the given scale factors.
  void InvScale(float inv_x_scale, float inv_y_scale);

  void Transpose() {
    using std::swap;
    swap(x_, y_);
  }

  std::string ToString() const;

  void WriteIntoTrace(perfetto::TracedValue) const;

 private:
  float x_;
  float y_;
};

inline constexpr bool operator==(const Vector2dF& lhs, const Vector2dF& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

inline constexpr bool operator!=(const Vector2dF& lhs, const Vector2dF& rhs) {
  return !(lhs == rhs);
}

inline constexpr Vector2dF operator-(const Vector2dF& v) {
  return Vector2dF(-v.x(), -v.y());
}

inline Vector2dF operator+(const Vector2dF& lhs, const Vector2dF& rhs) {
  Vector2dF result = lhs;
  result.Add(rhs);
  return result;
}

inline Vector2dF operator-(const Vector2dF& lhs, const Vector2dF& rhs) {
  Vector2dF result = lhs;
  result.Add(-rhs);
  return result;
}

// Return the cross product of two vectors, i.e. the determinant.
GEOMETRY_EXPORT double CrossProduct(const Vector2dF& lhs, const Vector2dF& rhs);

// Return the dot product of two vectors.
GEOMETRY_EXPORT double DotProduct(const Vector2dF& lhs, const Vector2dF& rhs);

// Return a vector that is |v| scaled by the given scale factors along each
// axis.
GEOMETRY_EXPORT Vector2dF ScaleVector2d(const Vector2dF& v,
                                        float x_scale,
                                        float y_scale);

// Return a vector that is |v| scaled by the given scale factor.
inline Vector2dF ScaleVector2d(const Vector2dF& v, float scale) {
  return ScaleVector2d(v, scale, scale);
}

inline Vector2dF TransposeVector2d(const Vector2dF& v) {
  return Vector2dF(v.y(), v.x());
}

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const Vector2dF& vector, ::std::ostream* os);

}  // namespace gfx

#endif // UI_GFX_GEOMETRY_VECTOR2D_F_H_
