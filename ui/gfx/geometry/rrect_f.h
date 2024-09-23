// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_RRECT_F_H_
#define UI_GFX_GEOMETRY_RRECT_F_H_

#include <memory>
#include <string>

#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace gfx {

class GEOMETRY_SKIA_EXPORT RRectF {
 public:
  RRectF() = default;
  ~RRectF() = default;
  RRectF(const RRectF& rect) = default;
  RRectF& operator=(const RRectF& rect) = default;
  explicit RRectF(const SkRRect& rect) : skrrect_(rect) {}
  explicit RRectF(const gfx::RectF& rect) : RRectF(rect, 0.f) {}
  RRectF(const gfx::RectF& rect, float radius) : RRectF(rect, radius, radius) {}
  RRectF(const gfx::RectF& rect, float x_rad, float y_rad)
      : RRectF(rect.x(), rect.y(), rect.width(), rect.height(), x_rad, y_rad) {}
  // Sets all x and y radii to radius.
  RRectF(float x, float y, float width, float height, float radius)
      : RRectF(x, y, width, height, radius, radius) {}
  // Sets all x radii to x_rad, and all y radii to y_rad. If one of x_rad or
  // y_rad are zero, sets ALL radii to zero.
  RRectF(float x, float y, float width, float height, float x_rad, float y_rad);
  // Directly sets all four corners.
  RRectF(float x,
         float y,
         float width,
         float height,
         float upper_left_x,
         float upper_left_y,
         float upper_right_x,
         float upper_right_y,
         float lower_right_x,
         float lower_right_y,
         float lower_left_x,
         float lower_left_y);
  RRectF(const gfx::RectF& rect,
         float upper_left_x,
         float upper_left_y,
         float upper_right_x,
         float upper_right_y,
         float lower_right_x,
         float lower_right_y,
         float lower_left_x,
         float lower_left_y)
      : RRectF(rect.x(),
               rect.y(),
               rect.width(),
               rect.height(),
               upper_left_x,
               upper_left_y,
               upper_right_x,
               upper_right_y,
               lower_right_x,
               lower_right_y,
               lower_left_x,
               lower_left_y) {}
  RRectF(const gfx::RectF& rect, const gfx::RoundedCornersF& corners)
      : RRectF(rect.x(),
               rect.y(),
               rect.width(),
               rect.height(),
               corners.upper_left(),
               corners.upper_left(),
               corners.upper_right(),
               corners.upper_right(),
               corners.lower_right(),
               corners.lower_right(),
               corners.lower_left(),
               corners.lower_left()) {}

  // The rectangular portion of the RRectF, without the corner radii.
  gfx::RectF rect() const { return gfx::SkRectToRectF(skrrect_.rect()); }

  // Returns the radii of the all corners. DCHECKs that all corners
  // have the same radii (the type is <= kOval).
  gfx::Vector2dF GetSimpleRadii() const;
  // Returns the radius of all corners. DCHECKs that all corners have the same
  // radii, and that x_rad == y_rad (the type is <= kSingle).
  float GetSimpleRadius() const;

  // Make the RRectF empty.
  void Clear() { skrrect_.setEmpty(); }

  bool Equals(const RRectF& other) const { return skrrect_ == other.skrrect_; }

  // These are all mutually exclusive, and ordered in increasing complexity. The
  // order is assumed in several functions.
  enum class Type {
    kEmpty,   // Zero width or height.
    kRect,    // Non-zero width and height, and zeroed radii - a pure rectangle.
    kSingle,  // Non-zero width and height, and a single, non-zero value for all
              // X and Y radii.
    kSimple,  // Non-zero width and height, X radii all equal and non-zero, Y
              // radii all equal and non-zero, and x_rad != y_rad.
    kOval,    // Non-zero width and height, X radii all equal to width/2, and Y
              // radii all equal to height/2, and x_rad != y_rad.
    kComplex,  // Non-zero width and height, and arbitrary (non-equal) radii.
  };
  Type GetType() const;

  bool IsEmpty() const { return GetType() == Type::kEmpty; }
  bool HasRoundedCorners() const {
    return !IsEmpty() && GetType() != Type::kRect;
  }

  // Enumeration of the corners of a rectangle in clockwise order. Values match
  // SkRRect::Corner.
  enum class Corner {
    kUpperLeft = SkRRect::kUpperLeft_Corner,
    kUpperRight = SkRRect::kUpperRight_Corner,
    kLowerRight = SkRRect::kLowerRight_Corner,
    kLowerLeft = SkRRect::kLowerLeft_Corner,
  };
  // GetCornerRadii may be called for any type of RRect (kRect, kOval, etc.),
  // and it will return "correct" values. If GetType() is kOval or less, all
  // corner values will be identical to each other. SetCornerRadii can similarly
  // be called on any type of RRect, but GetType() may change as a result of the
  // call.
  gfx::Vector2dF GetCornerRadii(Corner corner) const;
  void SetCornerRadii(Corner corner, float x_rad, float y_rad);
  void SetCornerRadii(Corner corner, const gfx::Vector2dF& radii) {
    SetCornerRadii(corner, radii.x(), radii.y());
  }

  // Returns true if |rect| is inside the bounds and corner radii of this
  // RRectF, and if both this RRectF and rect are not empty.
  bool Contains(const RectF& rect) const {
    return skrrect_.contains(gfx::RectFToSkRect(rect));
  }

  // Returns the bounding box that contains the specified rounded corner.
  gfx::RectF CornerBoundingRect(Corner corner) const;

  // Scales the rectangle by |scale|.
  void Scale(float scale) { Scale(scale, scale); }
  // Scales the rectangle by |x_scale| and |y_scale|.
  void Scale(float x_scale, float y_scale);

  // Move the rectangle by a horizontal and vertical distance.
  void Offset(float horizontal, float vertical);
  void Offset(const Vector2dF& distance) { Offset(distance.x(), distance.y()); }
  const RRectF& operator+=(const gfx::Vector2dF& offset);
  const RRectF& operator-=(const gfx::Vector2dF& offset);

  std::string ToString() const;
  bool ApproximatelyEqual(const RRectF& rect, float tolerance) const;

  // Insets bounds by dx and dy, and adjusts radii by dx and dy. dx and dy may
  // be positive, negative, or zero. If either corner radius is zero, the corner
  // has no curvature and is unchanged. Otherwise, if adjusted radius becomes
  // negative, the radius is pinned to zero.
  void Inset(float val) { skrrect_.inset(val, val); }
  void Inset(float dx, float dy) { skrrect_.inset(dx, dy); }
  // Outsets bounds by dx and dy, and adjusts radii by dx and dy. dx and dy may
  // be positive, negative, or zero. If either corner radius is zero, the corner
  // has no curvature and is unchanged. Otherwise, if adjusted radius becomes
  // negative, the radius is pinned to zero.
  void Outset(float val) { skrrect_.outset(val, val); }
  void Outset(float dx, float dy) { skrrect_.outset(dx, dy); }

  explicit operator SkRRect() const { return skrrect_; }

  static RRectF ToEnclosingRRectF(const RRectF& rrect);
  static RRectF ToEnclosingRRectFIgnoringError(const RRectF& rrect,
                                               float error = 0.001f);

 private:
  void GetAllRadii(SkVector radii[4]) const;

  gfx::RoundedCornersF GetRoundedCorners() const;

  SkRRect skrrect_;
};

inline std::ostream& operator<<(std::ostream& os, const RRectF& rect) {
  return os << rect.ToString();
}

inline bool operator==(const RRectF& a, const RRectF& b) {
  return a.Equals(b);
}

inline bool operator!=(const RRectF& a, const RRectF& b) {
  return !(a == b);
}

inline RRectF operator+(const RRectF& a, const gfx::Vector2dF& b) {
  RRectF result = a;
  result += b;
  return result;
}

inline RRectF operator-(const RRectF& a, const Vector2dF& b) {
  RRectF result = a;
  result -= b;
  return result;
}

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_RRECT_F_H_
