// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_QUAD_F_H_
#define UI_GFX_GEOMETRY_QUAD_F_H_

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <iosfwd>
#include <string>

#include "base/check_op.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {

// A Quad is defined by four corners, allowing it to have edges that are not
// axis-aligned, unlike a Rect.
class GEOMETRY_EXPORT QuadF {
 public:
  constexpr QuadF() = default;
  constexpr QuadF(const PointF& p1,
                  const PointF& p2,
                  const PointF& p3,
                  const PointF& p4)
      : p1_(p1), p2_(p2), p3_(p3), p4_(p4) {}

  constexpr explicit QuadF(const RectF& rect)
      : p1_(rect.x(), rect.y()),
        p2_(rect.right(), rect.y()),
        p3_(rect.right(), rect.bottom()),
        p4_(rect.x(), rect.bottom()) {}

  void operator=(const RectF& rect);

  void set_p1(const PointF& p) { p1_ = p; }
  void set_p2(const PointF& p) { p2_ = p; }
  void set_p3(const PointF& p) { p3_ = p; }
  void set_p4(const PointF& p) { p4_ = p; }

  constexpr const PointF& p1() const { return p1_; }
  constexpr const PointF& p2() const { return p2_; }
  constexpr const PointF& p3() const { return p3_; }
  constexpr const PointF& p4() const { return p4_; }

  // Returns true if the quad is an axis-aligned rectangle.
  bool IsRectilinear() const;

  // Returns true if the points of the quad are in counter-clockwise order. This
  // assumes that the quad is convex, and that no three points are collinear.
  bool IsCounterClockwise() const;

  // Returns true if the |point| is contained within the quad, or lies on on
  // edge of the quad. This assumes that the quad is convex.
  bool Contains(const PointF& point) const;

  // Returns true if the |quad| parameter is contained within |this| quad.
  // This method assumes |this| quad is convex. The |quad| parameter has no
  // restrictions.
  bool ContainsQuad(const QuadF& quad) const;

  // Returns two points (forming an axis-aligned bounding box) that bounds the
  // four points of the quad.
  std::pair<PointF, PointF> Extents() const {
    float rl = std::min({p1_.x(), p2_.x(), p3_.x(), p4_.x()});
    float rr = std::max({p1_.x(), p2_.x(), p3_.x(), p4_.x()});
    float rt = std::min({p1_.y(), p2_.y(), p3_.y(), p4_.y()});
    float rb = std::max({p1_.y(), p2_.y(), p3_.y(), p4_.y()});
    return std::make_pair(PointF(rl, rt), PointF(rr, rb));
  }

  // Returns a rectangle that bounds the four points of the quad. The points of
  // the quad may lie on the right/bottom edge of the resulting rectangle,
  // rather than being strictly inside it.
  RectF BoundingBox() const {
    const auto [min, max] = Extents();
    return RectF(min.x(), min.y(), max.x() - min.x(), max.y() - min.y());
  }

  // Realigns the corners in the quad by rotating them n corners to the right.
  void Realign(size_t times) {
    DCHECK_LE(times, 4u);
    for (size_t i = 0; i < times; ++i) {
      PointF temp = p1_;
      p1_ = p2_;
      p2_ = p3_;
      p3_ = p4_;
      p4_ = temp;
    }
  }

  // Add a vector to the quad, offseting each point in the quad by the vector.
  void operator+=(const Vector2dF& rhs);
  // Subtract a vector from the quad, offseting each point in the quad by the
  // inverse of the vector.
  void operator-=(const Vector2dF& rhs);

  // Scale each point in the quad by the |scale| factor.
  void Scale(float scale) { Scale(scale, scale); }

  // Scale each point in the quad by the scale factors along each axis.
  void Scale(float x_scale, float y_scale);

  // Tests whether any part of the rectangle intersects with this quad.
  // This only works for convex quads.
  // This intersection is edge-inclusive and will return true even if the
  // intersecting area is empty (i.e., the intersection is a line or a point).
  bool IntersectsRect(const RectF&) const;

  // Like the above, but only checks `rect` against the sides of quad ("does
  // half of the job"). Can be used if it is known beforehand that the bounding
  // box of the quad intersects `rect`.
  bool IntersectsRectPartial(const RectF& rect) const;

  // Tests whether any part of the quad intersects with this quad.
  // This intersection is edge-inclusive.
  bool IntersectsQuad(const QuadF& quad) const;

  // Test whether any part of the circle/ellipse intersects with this quad.
  // Note that these two functions only work for convex quads.
  // These intersections are edge-inclusive and will return true even if the
  // intersecting area is empty (i.e., the intersection is a line or a point).
  bool IntersectsCircle(const PointF& center, float radius) const;
  bool IntersectsEllipse(const PointF& center, const SizeF& radii) const;

  // The center of the quad. If the quad is the result of a affine-transformed
  // rectangle this is the same as the original center transformed.
  PointF CenterPoint() const {
    return PointF((p1_.x() + p2_.x() + p3_.x() + p4_.x()) / 4.0,
                  (p1_.y() + p2_.y() + p3_.y() + p4_.y()) / 4.0);
  }

  // Returns a string representation of quad.
  std::string ToString() const;

 private:
  bool IsToTheLeftOfOrTouchingLine(const PointF& base,
                                   const Vector2dF& vector) const;
  bool FullyOutsideOneEdge(const QuadF& quad) const;

  PointF p1_;
  PointF p2_;
  PointF p3_;
  PointF p4_;
};

inline bool operator==(const QuadF& lhs, const QuadF& rhs) {
  return
      lhs.p1() == rhs.p1() && lhs.p2() == rhs.p2() &&
      lhs.p3() == rhs.p3() && lhs.p4() == rhs.p4();
}

inline bool operator!=(const QuadF& lhs, const QuadF& rhs) {
  return !(lhs == rhs);
}

// Add a vector to a quad, offseting each point in the quad by the vector.
GEOMETRY_EXPORT QuadF operator+(const QuadF& lhs, const Vector2dF& rhs);
// Subtract a vector from a quad, offseting each point in the quad by the
// inverse of the vector.
GEOMETRY_EXPORT QuadF operator-(const QuadF& lhs, const Vector2dF& rhs);

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const QuadF& quad, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_QUAD_F_H_
