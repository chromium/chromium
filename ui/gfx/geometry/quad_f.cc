// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/quad_f.h"

#include <limits>

#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/triangle_f.h"

namespace gfx {

namespace {

PointF RightMostCornerToVector(const RectF& rect, const Vector2dF& vector) {
  // Return the corner of the rectangle that if it is to the left of the vector
  // would mean all of the rectangle is to the left of the vector.
  // The vector here represents the side between two points in a clockwise
  // convex polygon.
  //
  //  Q  XXX
  // QQQ XXX   If the lower left corner of X is left of the vector that goes
  //  QQQ      from the top corner of Q to the right corner of Q, then all of X
  //   Q       is left of the vector, and intersection impossible.
  //
  PointF point;
  if (vector.x() >= 0)
    point.set_y(rect.bottom());
  else
    point.set_y(rect.y());
  if (vector.y() >= 0)
    point.set_x(rect.x());
  else
    point.set_x(rect.right());
  return point;
}

// Tests whether the line is contained by or intersected with the circle.
bool LineIntersectsCircle(const PointF& center,
                          float radius,
                          const PointF& p0,
                          const PointF& p1) {
  float x0 = p0.x() - center.x(), y0 = p0.y() - center.y();
  float x1 = p1.x() - center.x(), y1 = p1.y() - center.y();
  float radius2 = radius * radius;
  if ((x0 * x0 + y0 * y0) <= radius2 || (x1 * x1 + y1 * y1) <= radius2)
    return true;
  if (p0 == p1)
    return false;

  float a = y0 - y1;
  float b = x1 - x0;
  float c = x0 * y1 - x1 * y0;
  float distance2 = c * c / (a * a + b * b);
  // If distance between the center point and the line > the radius,
  // the line doesn't cross (or is contained by) the ellipse.
  if (distance2 > radius2)
    return false;

  // The nearest point on the line is between p0 and p1?
  float x = -a * c / (a * a + b * b);
  float y = -b * c / (a * a + b * b);

  return (((x0 <= x && x <= x1) || (x0 >= x && x >= x1)) &&
          ((y0 <= y && y <= y1) || (y1 <= y && y <= y0)));
}

}  // anonymous namespace

void QuadF::operator=(const RectF& rect) {
  p1_ = PointF(rect.x(), rect.y());
  p2_ = PointF(rect.right(), rect.y());
  p3_ = PointF(rect.right(), rect.bottom());
  p4_ = PointF(rect.x(), rect.bottom());
}

std::string QuadF::ToString() const {
  return base::StringPrintf("%s;%s;%s;%s",
                            p1_.ToString().c_str(),
                            p2_.ToString().c_str(),
                            p3_.ToString().c_str(),
                            p4_.ToString().c_str());
}

static inline bool WithinEpsilon(float a, float b) {
  return std::abs(a - b) < std::numeric_limits<float>::epsilon();
}

bool QuadF::IsRectilinear() const {
  return
      (WithinEpsilon(p1_.x(), p2_.x()) && WithinEpsilon(p2_.y(), p3_.y()) &&
       WithinEpsilon(p3_.x(), p4_.x()) && WithinEpsilon(p4_.y(), p1_.y())) ||
      (WithinEpsilon(p1_.y(), p2_.y()) && WithinEpsilon(p2_.x(), p3_.x()) &&
       WithinEpsilon(p3_.y(), p4_.y()) && WithinEpsilon(p4_.x(), p1_.x()));
}

bool QuadF::IsCounterClockwise() const {
  // This math computes the signed area of the quad. Positive area
  // indicates the quad is clockwise; negative area indicates the quad is
  // counter-clockwise. Note carefully: this is backwards from conventional
  // math because our geometric space uses screen coordiantes with y-axis
  // pointing downards.
  // Reference: http://mathworld.wolfram.com/PolygonArea.html.
  // The equation can be written:
  // Signed area = determinant1 + determinant2 + determinant3 + determinant4
  // In practise, Refactoring the computation of adding determinants so that
  // reducing the number of operations. The equation is:
  // Signed area = element1 + element2 - element3 - element4

  float p24 = p2_.y() - p4_.y();
  float p31 = p3_.y() - p1_.y();

  // Up-cast to double so this cannot overflow.
  double element1 = static_cast<double>(p1_.x()) * p24;
  double element2 = static_cast<double>(p2_.x()) * p31;
  double element3 = static_cast<double>(p3_.x()) * p24;
  double element4 = static_cast<double>(p4_.x()) * p31;

  return element1 + element2 < element3 + element4;
}

bool QuadF::Contains(const PointF& point) const {
  return PointIsInTriangle(point, p1_, p2_, p3_) ||
         PointIsInTriangle(point, p1_, p3_, p4_);
}

bool QuadF::ContainsQuad(const QuadF& other) const {
  return Contains(other.p1()) && Contains(other.p2()) && Contains(other.p3()) &&
         Contains(other.p4());
}

void QuadF::Scale(float x_scale, float y_scale) {
  p1_.Scale(x_scale, y_scale);
  p2_.Scale(x_scale, y_scale);
  p3_.Scale(x_scale, y_scale);
  p4_.Scale(x_scale, y_scale);
}

void QuadF::operator+=(const Vector2dF& rhs) {
  p1_ += rhs;
  p2_ += rhs;
  p3_ += rhs;
  p4_ += rhs;
}

void QuadF::operator-=(const Vector2dF& rhs) {
  p1_ -= rhs;
  p2_ -= rhs;
  p3_ -= rhs;
  p4_ -= rhs;
}

QuadF operator+(const QuadF& lhs, const Vector2dF& rhs) {
  QuadF result = lhs;
  result += rhs;
  return result;
}

QuadF operator-(const QuadF& lhs, const Vector2dF& rhs) {
  QuadF result = lhs;
  result -= rhs;
  return result;
}

bool QuadF::IntersectsRect(const RectF& rect) const {
  // Start by checking this quad against the potential separating axes of the
  // rectangle. Since the rectangle is axis-aligned, we can just check for
  // intersection between the bounding boxes - if they don't intersect one of
  // the edges of the rectangle is a separating axis.
  const auto [min, max] = Extents();
  if (min.y() > rect.bottom() || rect.y() > max.y()) {
    return false;
  }
  if (min.x() > rect.right() || rect.x() > max.x()) {
    return false;
  }
  // None of the edges of the rectangle are a separating axis - test the edges
  // of this quad.
  return IntersectsRectPartial(rect);
}

bool QuadF::IntersectsRectPartial(const RectF& rect) const {
  // For each side of the quad clockwise we check if the rectangle is to the
  // left of it since only content on the right can overlap with the quad.
  // This only works if the quad is convex.
  Vector2dF v1, v2, v3, v4;

  // Ensure we use clockwise vectors.
  if (IsCounterClockwise()) {
    v1 = p4_ - p1_;
    v2 = p1_ - p2_;
    v3 = p2_ - p3_;
    v4 = p3_ - p4_;
  } else {
    v1 = p2_ - p1_;
    v2 = p3_ - p2_;
    v3 = p4_ - p3_;
    v4 = p1_ - p4_;
  }

  PointF p = RightMostCornerToVector(rect, v1);
  if (CrossProduct(v1, p - p1_) < 0)
    return false;

  p = RightMostCornerToVector(rect, v2);
  if (CrossProduct(v2, p - p2_) < 0)
    return false;

  p = RightMostCornerToVector(rect, v3);
  if (CrossProduct(v3, p - p3_) < 0)
    return false;

  p = RightMostCornerToVector(rect, v4);
  if (CrossProduct(v4, p - p4_) < 0)
    return false;

  // If not all of the rectangle is outside one of the quad's four sides, then
  // that means at least a part of the rectangle is overlapping the quad.
  return true;
}

bool QuadF::IsToTheLeftOfOrTouchingLine(const PointF& base,
                                        const Vector2dF& vector) const {
  if (CrossProduct(vector, p1_ - base) >= 0) {
    return false;
  }
  if (CrossProduct(vector, p2_ - base) >= 0) {
    return false;
  }
  if (CrossProduct(vector, p3_ - base) >= 0) {
    return false;
  }
  if (CrossProduct(vector, p4_ - base) >= 0) {
    return false;
  }
  return true;
}

bool QuadF::FullyOutsideOneEdge(const QuadF& quad) const {
  // For each side of the quad clockwise we check if the quad is to the left of
  // it since only content on the right can overlap with the quad. This only
  // works if the quads are convex.
  Vector2dF v1, v2, v3, v4;

  // Ensure we use clockwise vectors.
  if (IsCounterClockwise()) {
    v1 = p4_ - p1_;
    v2 = p1_ - p2_;
    v3 = p2_ - p3_;
    v4 = p3_ - p4_;
  } else {
    v1 = p2_ - p1_;
    v2 = p3_ - p2_;
    v3 = p4_ - p3_;
    v4 = p1_ - p4_;
  }

  if (quad.IsToTheLeftOfOrTouchingLine(p1_, v1)) {
    return true;
  }
  if (quad.IsToTheLeftOfOrTouchingLine(p2_, v2)) {
    return true;
  }
  if (quad.IsToTheLeftOfOrTouchingLine(p3_, v3)) {
    return true;
  }
  if (quad.IsToTheLeftOfOrTouchingLine(p4_, v4)) {
    return true;
  }
  return false;
}

bool QuadF::IntersectsQuad(const QuadF& quad) const {
  // Check if |quad| is fully outside one of the edges of this quad or vice
  // versa.
  return !FullyOutsideOneEdge(quad) && !quad.FullyOutsideOneEdge(*this);
}

bool QuadF::IntersectsCircle(const PointF& center, float radius) const {
  return Contains(center) || LineIntersectsCircle(center, radius, p1_, p2_) ||
         LineIntersectsCircle(center, radius, p2_, p3_) ||
         LineIntersectsCircle(center, radius, p3_, p4_) ||
         LineIntersectsCircle(center, radius, p4_, p1_);
}

bool QuadF::IntersectsEllipse(const PointF& center, const SizeF& radii) const {
  // Transform the ellipse to an origin-centered circle whose radius is the
  // product of major radius and minor radius.  Here we apply the same
  // transformation to the quad.
  QuadF transformed_quad = *this;
  transformed_quad -= center.OffsetFromOrigin();
  transformed_quad.Scale(radii.height(), radii.width());

  PointF origin_point;
  return transformed_quad.IntersectsCircle(origin_point,
                                           radii.height() * radii.width());
}

}  // namespace gfx
