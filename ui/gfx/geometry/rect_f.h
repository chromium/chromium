// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_RECT_F_H_
#define UI_GFX_GEOMETRY_RECT_F_H_

#include <iosfwd>
#include <string>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if BUILDFLAG(IS_APPLE)
typedef struct CGRect CGRect;
#endif

namespace gfx {

// A floating version of gfx::Rect.
class GEOMETRY_EXPORT RectF {
 public:
  constexpr RectF() = default;
  constexpr RectF(float width, float height) : size_(width, height) {}
  constexpr RectF(float x, float y, float width, float height)
      : origin_(x, y), size_(width, height) {}
  constexpr explicit RectF(const SizeF& size) : size_(size) {}
  constexpr explicit RectF(const Size& size) : size_(size) {}
  constexpr RectF(const PointF& origin, const SizeF& size)
      : origin_(origin), size_(size) {}

  constexpr explicit RectF(const Rect& r)
      : RectF(static_cast<float>(r.x()),
              static_cast<float>(r.y()),
              static_cast<float>(r.width()),
              static_cast<float>(r.height())) {}

#if BUILDFLAG(IS_APPLE)
  explicit RectF(const CGRect& r);
  // Construct an equivalent CoreGraphics object.
  CGRect ToCGRect() const;
#endif

  constexpr float x() const { return origin_.x(); }
  void set_x(float x) { origin_.set_x(x); }

  constexpr float y() const { return origin_.y(); }
  void set_y(float y) { origin_.set_y(y); }

  constexpr float width() const { return size_.width(); }
  void set_width(float width) { size_.set_width(width); }

  constexpr float height() const { return size_.height(); }
  void set_height(float height) { size_.set_height(height); }

  constexpr const PointF& origin() const { return origin_; }
  void set_origin(const PointF& origin) { origin_ = origin; }

  constexpr const SizeF& size() const { return size_; }
  void set_size(const SizeF& size) { size_ = size; }

  constexpr float right() const { return x() + width(); }
  constexpr float bottom() const { return y() + height(); }

  constexpr PointF top_right() const { return PointF(right(), y()); }
  constexpr PointF bottom_left() const { return PointF(x(), bottom()); }
  constexpr PointF bottom_right() const { return PointF(right(), bottom()); }

  constexpr PointF left_center() const {
    return PointF(x(), y() + height() / 2);
  }
  constexpr PointF top_center() const { return PointF(x() + width() / 2, y()); }
  constexpr PointF right_center() const {
    return PointF(right(), y() + height() / 2);
  }
  constexpr PointF bottom_center() const {
    return PointF(x() + width() / 2, bottom());
  }

  Vector2dF OffsetFromOrigin() const { return Vector2dF(x(), y()); }

  void SetRect(float x, float y, float width, float height) {
    origin_.SetPoint(x, y);
    size_.SetSize(width, height);
  }

  // Shrinks the rectangle by |inset| on all sides.
  void Inset(float inset) { Inset(InsetsF(inset)); }
  // Shrinks the rectangle by the given |insets|.
  void Inset(const InsetsF& insets);

  // Expands the rectangle by |outset| on all sides.
  void Outset(float outset) { Inset(-outset); }
  // Expands the rectangle by the given |outsets|.
  void Outset(const OutsetsF& outsets) { Inset(outsets.ToInsets()); }

  // Move the rectangle by a horizontal and vertical distance.
  void Offset(float horizontal, float vertical);
  void Offset(const Vector2dF& distance) { Offset(distance.x(), distance.y()); }
  void operator+=(const Vector2dF& offset);
  void operator-=(const Vector2dF& offset);

  InsetsF InsetsFrom(const RectF& inner) const;

  // Returns true if the area of the rectangle is zero.
  constexpr bool IsEmpty() const { return size_.IsEmpty(); }

  // A rect is less than another rect if its origin is less than
  // the other rect's origin. If the origins are equal, then the
  // shortest rect is less than the other. If the origin and the
  // height are equal, then the narrowest rect is less than.
  // This comparison is required to use Rects in sets, or sorted
  // vectors.
  bool operator<(const RectF& other) const;

  // Returns true if the point identified by point_x and point_y falls inside
  // this rectangle (including the left and the top edges, excluding the right
  // and the bottom edges). If this rectangle is empty, this method returns
  // false regardless of the point.
  bool Contains(float point_x, float point_y) const;

  // Returns true if the specified point is contained by this rectangle.
  bool Contains(const PointF& point) const {
    return Contains(point.x(), point.y());
  }

  // Similar to Contains(), but uses edge-inclusive geometry, i.e. also returns
  // true if the point is on the right or the bottom edge. If this rectangle
  // is empty, this method returns true only if the point is at the origin of
  // this rectangle.
  bool InclusiveContains(float point_x, float point_y) const;
  bool InclusiveContains(const PointF& point) const {
    return InclusiveContains(point.x(), point.y());
  }

  // Returns true if this rectangle contains the specified rectangle.
  bool Contains(const RectF& rect) const;

  // Returns true if this rectangle intersects the specified rectangle.
  // An empty rectangle doesn't intersect any rectangle.
  bool Intersects(const RectF& rect) const;

  // Sets this rect to be the intersection of this rectangle with the given
  // rectangle.
  void Intersect(const RectF& rect);

  // Sets this rect to be the intersection of itself and |rect| using
  // edge-inclusive geometry.  If the two rectangles overlap but the overlap
  // region is zero-area (either because one of the two rectangles is zero-area,
  // or because the rectangles overlap at an edge or a corner), the result is
  // the zero-area intersection.  The return value indicates whether the two
  // rectangle actually have an intersection, since checking the result for
  // isEmpty() is not conclusive.
  bool InclusiveIntersect(const RectF& rect);

  // Sets this rect to be the union of this rectangle with the given rectangle.
  // The union is the smallest rectangle containing both rectangles if not
  // empty. If both rects are empty, this rect will become |rect|.
  void Union(const RectF& rect);

  // Similar to Union(), but the result will contain both rectangles even if
  // either of them is empty. For example, union of (100, 100, 0x0) and
  // (200, 200, 50x0) is (100, 100, 150x100).
  void UnionEvenIfEmpty(const RectF& rect);

  // Sets this rect to be the rectangle resulting from subtracting |rect| from
  // |*this|, i.e. the bounding rect of |Region(*this) - Region(rect)|.
  void Subtract(const RectF& rect);

  // Fits as much of the receiving rectangle into the supplied rectangle as
  // possible, becoming the result. For example, if the receiver had
  // a x-location of 2 and a width of 4, and the supplied rectangle had
  // an x-location of 0 with a width of 5, the returned rectangle would have
  // an x-location of 1 with a width of 4.
  void AdjustToFit(const RectF& rect);

  // Returns the center of this rectangle.
  PointF CenterPoint() const;

  // Becomes a rectangle that has the same center point but with a size capped
  // at given |size|.
  void ClampToCenteredSize(const SizeF& size);

  // Transpose x and y axis.
  void Transpose();

  // Splits `this` in two halves, `left_half` and `right_half`.
  void SplitVertically(RectF& left_half, RectF& right_half) const;

  // Splits `this` in two halves, `top_half` and `bottom_half`.
  void SplitHorizontally(RectF& top_half, RectF& bottom_half) const;

  // Returns true if this rectangle shares an entire edge (i.e., same width or
  // same height) with the given rectangle, and the rectangles do not overlap.
  bool SharesEdgeWith(const RectF& rect) const;

  // Returns the manhattan distance from the rect to the point. If the point is
  // inside the rect, returns 0.
  float ManhattanDistanceToPoint(const PointF& point) const;

  // Returns the manhattan distance between the contents of this rect and the
  // contents of the given rect. That is, if the intersection of the two rects
  // is non-empty then the function returns 0. If the rects share a side, it
  // returns the smallest non-zero value appropriate for float.
  float ManhattanInternalDistance(const RectF& rect) const;

  // Returns the closest point in or on an edge of this rect to the given point.
  PointF ClosestPoint(const PointF& point) const;

  // Scales the rectangle by |scale|.
  void Scale(float scale) {
    Scale(scale, scale);
  }

  void Scale(float x_scale, float y_scale) {
    set_origin(ScalePoint(origin(), x_scale, y_scale));
    set_size(ScaleSize(size(), x_scale, y_scale));
  }

  // Divides the rectangle by |inv_scale|.
  void InvScale(float inv_scale) { InvScale(inv_scale, inv_scale); }

  void InvScale(float x_scale, float y_scale) {
    origin_.InvScale(x_scale, y_scale);
    size_.InvScale(x_scale, y_scale);
  }

  // This method reports if the RectF can be safely converted to an integer
  // Rect. When it is false, some dimension of the RectF is outside the bounds
  // of what an integer can represent, and converting it to a Rect will require
  // clamping.
  bool IsExpressibleAsRect() const;

  std::string ToString() const;

  bool ApproximatelyEqual(const RectF& rect,
                          float tolerance_x,
                          float tolerance_y) const;

 private:
  PointF origin_;
  SizeF size_;
};

constexpr bool operator==(const RectF& lhs, const RectF& rhs) {
  return lhs.origin() == rhs.origin() && lhs.size() == rhs.size();
}

constexpr bool operator!=(const RectF& lhs, const RectF& rhs) {
  return !(lhs == rhs);
}

inline RectF operator+(const RectF& lhs, const Vector2dF& rhs) {
  return RectF(lhs.x() + rhs.x(), lhs.y() + rhs.y(),
      lhs.width(), lhs.height());
}

inline RectF operator-(const RectF& lhs, const Vector2dF& rhs) {
  return RectF(lhs.x() - rhs.x(), lhs.y() - rhs.y(),
      lhs.width(), lhs.height());
}

inline RectF operator+(const Vector2dF& lhs, const RectF& rhs) {
  return rhs + lhs;
}

GEOMETRY_EXPORT RectF IntersectRects(const RectF& a, const RectF& b);
GEOMETRY_EXPORT RectF UnionRects(const RectF& a, const RectF& b);
GEOMETRY_EXPORT RectF UnionRects(base::span<const RectF> rects);
GEOMETRY_EXPORT RectF UnionRectsEvenIfEmpty(const RectF& a, const RectF& b);
GEOMETRY_EXPORT RectF SubtractRects(const RectF& a, const RectF& b);

inline RectF ScaleRect(const RectF& r, float x_scale, float y_scale) {
  return RectF(r.x() * x_scale, r.y() * y_scale,
       r.width() * x_scale, r.height() * y_scale);
}

inline RectF ScaleRect(const RectF& r, const SizeF& size) {
  return ScaleRect(r, size.width(), size.height());
}

inline RectF ScaleRect(const RectF& r, const Size& size) {
  return ScaleRect(r, SizeF(size));
}

inline RectF ScaleRect(const RectF& r, float scale) {
  return ScaleRect(r, scale, scale);
}

inline RectF TransposeRect(const RectF& r) {
  return RectF(r.y(), r.x(), r.height(), r.width());
}

// Constructs a rectangle with |p1| and |p2| as opposite corners.
//
// This could also be thought of as "the smallest rect that contains both
// points", except that we consider points on the right/bottom edges of the
// rect to be outside the rect.  So technically one or both points will not be
// contained within the rect, because they will appear on one of these edges.
GEOMETRY_EXPORT RectF BoundingRect(const PointF& p1, const PointF& p2);

// Return a maximum rectangle in which any point is covered by either a or b.
GEOMETRY_EXPORT RectF MaximumCoveredRect(const RectF& a, const RectF& b);

// Returns the rect in |dest_rect| corresponding to |r] in |src_rect| when
// |src_rect| is mapped to |dest_rect|.
GEOMETRY_EXPORT RectF MapRect(const RectF& r,
                              const RectF& src_rect,
                              const RectF& dest_rect);

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const RectF& rect, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_RECT_F_H_
