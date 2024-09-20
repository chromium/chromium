// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a simple integer rectangle class.  The containment semantics
// are array-like; that is, the coordinate (x, y) is considered to be
// contained by the rectangle, but the coordinate (x + width, y) is not.
// The class will happily let you create malformed rectangles (that is,
// rectangles with negative width and/or height), but there will be assertions
// in the operations (such as Contains()) to complain in this case.

#ifndef UI_GFX_GEOMETRY_RECT_H_
#define UI_GFX_GEOMETRY_RECT_H_

#include <cmath>
#include <iosfwd>
#include <string>

#include "base/containers/span.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

#if BUILDFLAG(IS_WIN)
typedef struct tagRECT RECT;
#elif BUILDFLAG(IS_APPLE)
typedef struct CGRect CGRect;
#endif

namespace gfx {

class GEOMETRY_EXPORT Rect {
 public:
  constexpr Rect() = default;
  constexpr Rect(int width, int height) : size_(width, height) {}
  constexpr Rect(int x, int y, int width, int height)
      : origin_(x, y),
        size_(ClampWidthOrHeight(x, width), ClampWidthOrHeight(y, height)) {}
  constexpr explicit Rect(const Size& size) : size_(size) {}
  constexpr Rect(const Point& origin, const Size& size)
      : origin_(origin),
        size_(ClampWidthOrHeight(origin.x(), size.width()),
              ClampWidthOrHeight(origin.y(), size.height())) {}

#if BUILDFLAG(IS_WIN)
  explicit Rect(const RECT& r);
#elif BUILDFLAG(IS_APPLE)
  explicit Rect(const CGRect& r);
#endif

#if BUILDFLAG(IS_WIN)
  // Construct an equivalent Win32 RECT object.
  RECT ToRECT() const;
#elif BUILDFLAG(IS_APPLE)
  // Construct an equivalent CoreGraphics object.
  CGRect ToCGRect() const;
#endif

  constexpr int x() const { return origin_.x(); }
  // Sets the X position while preserving the width.
  void set_x(int x) {
    origin_.set_x(x);
    size_.set_width(ClampWidthOrHeight(x, width()));
  }

  constexpr int y() const { return origin_.y(); }
  // Sets the Y position while preserving the height.
  void set_y(int y) {
    origin_.set_y(y);
    size_.set_height(ClampWidthOrHeight(y, height()));
  }

  constexpr int width() const { return size_.width(); }
  void set_width(int width) { size_.set_width(ClampWidthOrHeight(x(), width)); }

  constexpr int height() const { return size_.height(); }
  void set_height(int height) {
    size_.set_height(ClampWidthOrHeight(y(), height));
  }

  constexpr const Point& origin() const { return origin_; }
  void set_origin(const Point& origin) {
    origin_ = origin;
    // Ensure that width and height remain valid.
    set_width(width());
    set_height(height());
  }

  constexpr const Size& size() const { return size_; }
  void set_size(const Size& size) {
    set_width(size.width());
    set_height(size.height());
  }

  constexpr int right() const { return x() + width(); }
  constexpr int bottom() const { return y() + height(); }

  constexpr Point top_right() const { return Point(right(), y()); }
  constexpr Point bottom_left() const { return Point(x(), bottom()); }
  constexpr Point bottom_right() const { return Point(right(), bottom()); }

  constexpr Point left_center() const { return Point(x(), y() + height() / 2); }
  constexpr Point top_center() const { return Point(x() + width() / 2, y()); }
  constexpr Point right_center() const {
    return Point(right(), y() + height() / 2);
  }
  constexpr Point bottom_center() const {
    return Point(x() + width() / 2, bottom());
  }

  Vector2d OffsetFromOrigin() const { return Vector2d(x(), y()); }

  void SetRect(int x, int y, int width, int height) {
    origin_.SetPoint(x, y);
    // Ensure that width and height remain valid.
    set_width(width);
    set_height(height);
  }

  // Use in place of SetRect() when you know the edges of the rectangle instead
  // of the dimensions, rather than trying to determine the width/height
  // yourself. This safely handles cases where the width/height would overflow.
  void SetByBounds(int left, int top, int right, int bottom) {
    SetHorizontalBounds(left, right);
    SetVerticalBounds(top, bottom);
  }
  void SetHorizontalBounds(int left, int right) {
    set_x(left);
    set_width(base::ClampSub(right, left));
    if (this->right() != right) [[unlikely]] {
      AdjustForSaturatedRight(right);
    }
  }
  void SetVerticalBounds(int top, int bottom) {
    set_y(top);
    set_height(base::ClampSub(bottom, top));
    if (this->bottom() != bottom) [[unlikely]] {
      AdjustForSaturatedBottom(bottom);
    }
  }

  // Shrink the rectangle by |inset| on all sides.
  void Inset(int inset) { Inset(Insets(inset)); }
  // Shrink the rectangle by the given |insets|.
  void Inset(const Insets& insets);

  // Expand the rectangle by |outset| on all sides.
  void Outset(int outset) { Inset(-outset); }
  // Expand the rectangle by the given |outsets|.
  void Outset(const Outsets& outsets) { Inset(outsets.ToInsets()); }

  // Move the rectangle by a horizontal and vertical distance.
  void Offset(int horizontal, int vertical) {
    Offset(Vector2d(horizontal, vertical));
  }
  void Offset(const Vector2d& distance);
  void operator+=(const Vector2d& offset) { Offset(offset); }
  void operator-=(const Vector2d& offset) { Offset(-offset); }

  Insets InsetsFrom(const Rect& inner) const;

  // Returns true if the area of the rectangle is zero.
  bool IsEmpty() const { return size_.IsEmpty(); }

  // A rect is less than another rect if its origin is less than
  // the other rect's origin. If the origins are equal, then the
  // shortest rect is less than the other. If the origin and the
  // height are equal, then the narrowest rect is less than.
  // This comparison is required to use Rects in sets, or sorted
  // vectors.
  bool operator<(const Rect& other) const;

  // Returns true if the point identified by point_x and point_y falls inside
  // this rectangle.  The point (x, y) is inside the rectangle, but the
  // point (x + width, y + height) is not.
  bool Contains(int point_x, int point_y) const;

  // Returns true if the specified point is contained by this rectangle.
  bool Contains(const Point& point) const {
    return Contains(point.x(), point.y());
  }

  // Returns true if this rectangle contains the specified rectangle.
  bool Contains(const Rect& rect) const;

  // Returns true if this rectangle intersects the specified rectangle.
  // An empty rectangle doesn't intersect any rectangle.
  bool Intersects(const Rect& rect) const;

  // Sets this rect to be the intersection of this rectangle with the given
  // rectangle.
  void Intersect(const Rect& rect);

  // Sets this rect to be the intersection of itself and |rect| using
  // edge-inclusive geometry.  If the two rectangles overlap but the overlap
  // region is zero-area (either because one of the two rectangles is zero-area,
  // or because the rectangles overlap at an edge or a corner), the result is
  // the zero-area intersection.  The return value indicates whether the two
  // rectangle actually have an intersection, since checking the result for
  // isEmpty() is not conclusive.
  bool InclusiveIntersect(const Rect& rect);

  // Sets this rect to be the union of this rectangle with the given rectangle.
  // The union is the smallest rectangle containing both rectangles if not
  // empty. If both rects are empty, this rect will become |rect|.
  void Union(const Rect& rect);

  // Similar to Union(), but the result will contain both rectangles even if
  // either of them is empty. For example, union of (100, 100, 0x0) and
  // (200, 200, 50x0) is (100, 100, 150x100).
  void UnionEvenIfEmpty(const Rect& rect);

  // Sets this rect to be the rectangle resulting from subtracting |rect| from
  // |*this|, i.e. the bounding rect of |Region(*this) - Region(rect)|.
  void Subtract(const Rect& rect);

  // Fits as much of the receiving rectangle into the supplied rectangle as
  // possible, becoming the result. For example, if the receiver had
  // a x-location of 2 and a width of 4, and the supplied rectangle had
  // an x-location of 0 with a width of 5, the returned rectangle would have
  // an x-location of 1 with a width of 4.
  void AdjustToFit(const Rect& rect);

  // Returns the center of this rectangle.
  Point CenterPoint() const;

  // Becomes a rectangle that has the same center point but with a size capped
  // at given |size|.
  void ClampToCenteredSize(const Size& size);

  // Transpose x and y axis.
  void Transpose();

  // Splits `this` in two halves, `left_half` and `right_half`.
  void SplitVertically(Rect& left_half, Rect& right_half) const;

  // Splits `this` in two halves, `top_half` and `bottom_half`.
  void SplitHorizontally(Rect& top_half, Rect& bottom_half) const;

  // Returns true if this rectangle shares an entire edge (i.e., same width or
  // same height) with the given rectangle, and the rectangles do not overlap.
  bool SharesEdgeWith(const Rect& rect) const;

  // Returns the manhattan distance from the rect to the point. If the point is
  // inside the rect, returns 0.
  int ManhattanDistanceToPoint(const Point& point) const;

  // Returns the manhattan distance between the contents of this rect and the
  // contents of the given rect. That is, if the intersection of the two rects
  // is non-empty then the function returns 0. If the rects share a side, it
  // returns the smallest non-zero value appropriate for int.
  int ManhattanInternalDistance(const Rect& rect) const;

  std::string ToString() const;

  bool ApproximatelyEqual(const Rect& rect, int tolerance) const;

 private:
  // Clamp the width/height to avoid integer overflow in bottom() and right().
  // This returns the clamped width/height given an |x_or_y| and a
  // |width_or_height|.
  static constexpr int ClampWidthOrHeight(int x_or_y, int width_or_height) {
    return base::ClampAdd(x_or_y, width_or_height) - x_or_y;
  }

  void AdjustForSaturatedRight(int right);
  void AdjustForSaturatedBottom(int bottom);

  gfx::Point origin_;
  gfx::Size size_;
};

inline bool operator==(const Rect& lhs, const Rect& rhs) {
  return lhs.origin() == rhs.origin() && lhs.size() == rhs.size();
}

inline bool operator!=(const Rect& lhs, const Rect& rhs) {
  return !(lhs == rhs);
}

GEOMETRY_EXPORT Rect operator+(const Rect& lhs, const Vector2d& rhs);
GEOMETRY_EXPORT Rect operator-(const Rect& lhs, const Vector2d& rhs);

inline Rect operator+(const Vector2d& lhs, const Rect& rhs) {
  return rhs + lhs;
}

GEOMETRY_EXPORT Rect IntersectRects(const Rect& a, const Rect& b);
GEOMETRY_EXPORT Rect UnionRects(const Rect& a, const Rect& b);
GEOMETRY_EXPORT Rect UnionRects(base::span<const Rect> rects);
GEOMETRY_EXPORT Rect UnionRectsEvenIfEmpty(const Rect& a, const Rect& b);
GEOMETRY_EXPORT Rect SubtractRects(const Rect& a, const Rect& b);

// Constructs a rectangle with |p1| and |p2| as opposite corners.
//
// This could also be thought of as "the smallest rect that contains both
// points", except that we consider points on the right/bottom edges of the
// rect to be outside the rect.  So technically one or both points will not be
// contained within the rect, because they will appear on one of these edges.
GEOMETRY_EXPORT Rect BoundingRect(const Point& p1, const Point& p2);

// Scales the rect and returns the enclosing rect. The components are clamped
// if they would overflow.
inline Rect ScaleToEnclosingRect(const Rect& rect,
                                 float x_scale,
                                 float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return rect;
  int x = base::ClampFloor(rect.x() * x_scale);
  int y = base::ClampFloor(rect.y() * y_scale);
  int r = rect.width() == 0 ? x : base::ClampCeil(rect.right() * x_scale);
  int b = rect.height() == 0 ? y : base::ClampCeil(rect.bottom() * y_scale);
  Rect result;
  result.SetByBounds(x, y, r, b);
  return result;
}

inline Rect ScaleToEnclosingRect(const Rect& rect, float scale) {
  return ScaleToEnclosingRect(rect, scale, scale);
}

inline Rect ScaleToEnclosedRect(const Rect& rect,
                                float x_scale,
                                float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return rect;
  int x = base::ClampCeil(rect.x() * x_scale);
  int y = base::ClampCeil(rect.y() * y_scale);
  int r = rect.width() == 0 ? x : base::ClampFloor(rect.right() * x_scale);
  int b = rect.height() == 0 ? y : base::ClampFloor(rect.bottom() * y_scale);
  Rect result;
  result.SetByBounds(x, y, r, b);
  return result;
}

inline Rect ScaleToEnclosedRect(const Rect& rect, float scale) {
  return ScaleToEnclosedRect(rect, scale, scale);
}

// Scales |rect| by scaling its four corner points. If the corner points lie on
// non-integral coordinate after scaling, their values are rounded to the
// nearest integer. The components are clamped if they would overflow.
// This is helpful during layout when relative positions of multiple gfx::Rect
// in a given coordinate space needs to be same after scaling as it was before
// scaling. ie. this gives a lossless relative positioning of rects.
inline Rect ScaleToRoundedRect(const Rect& rect, float x_scale, float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return rect;
  int x = base::ClampRound(rect.x() * x_scale);
  int y = base::ClampRound(rect.y() * y_scale);
  int r = rect.width() == 0 ? x : base::ClampRound(rect.right() * x_scale);
  int b = rect.height() == 0 ? y : base::ClampRound(rect.bottom() * y_scale);
  Rect result;
  result.SetByBounds(x, y, r, b);
  return result;
}

inline Rect ScaleToRoundedRect(const Rect& rect, float scale) {
  return ScaleToRoundedRect(rect, scale, scale);
}

// Scales `rect` by `scale` and rounds to enclosing rect, but for each edge, if
// the distance between the edge and the nearest integer grid is smaller than
// `error`, the edge is snapped to the integer grid.  The default error is 0.001
// , which is used by cc/viz. Use this when scaling the window/layer size.
GEOMETRY_EXPORT Rect ScaleToEnclosingRectIgnoringError(const Rect& rect,
                                                       float scale,
                                                       float error = 0.001f);

// Return a maximum rectangle that is covered by the a or b.
GEOMETRY_EXPORT Rect MaximumCoveredRect(const Rect& a, const Rect& b);

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const Rect& rect, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_RECT_H_
