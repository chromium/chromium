// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_INSETS_H_
#define UI_GFX_GEOMETRY_INSETS_H_

#include <string>

#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/insets_f.h"

namespace gfx {

class Vector2d;

// Represents the widths of the four borders or margins of an unspecified
// rectangle. An Insets stores the thickness of the top, left, bottom and right
// edges, without storing the actual size and position of the rectangle itself.
//
// This can be used to represent a space within a rectangle, by "shrinking" the
// rectangle by the inset amount on all four sides. Alternatively, it can
// represent a border that has a different thickness on each side.
class GEOMETRY_EXPORT Insets {
 public:
  constexpr Insets() : top_(0), left_(0), bottom_(0), right_(0) {}
  constexpr explicit Insets(int all)
      : top_(all), left_(all), bottom_(all), right_(all) {}
  constexpr Insets(int vertical, int horizontal)
      : top_(vertical),
        left_(horizontal),
        bottom_(vertical),
        right_(horizontal) {}
  constexpr Insets(int top, int left, int bottom, int right)
      : top_(top), left_(left), bottom_(bottom), right_(right) {}

  constexpr int top() const { return top_; }
  constexpr int left() const { return left_; }
  constexpr int bottom() const { return bottom_; }
  constexpr int right() const { return right_; }

  // Returns the total width taken up by the insets, which is the sum of the
  // left and right insets.
  constexpr int width() const { return left_ + right_; }

  // Returns the total height taken up by the insets, which is the sum of the
  // top and bottom insets.
  constexpr int height() const { return top_ + bottom_; }

  // Returns true if the insets are empty.
  bool IsEmpty() const { return width() == 0 && height() == 0; }

  void Set(int top, int left, int bottom, int right) {
    top_ = top;
    left_ = left;
    bottom_ = bottom;
    right_ = right;
  }

  bool operator==(const Insets& insets) const {
    return top_ == insets.top_ && left_ == insets.left_ &&
           bottom_ == insets.bottom_ && right_ == insets.right_;
  }

  bool operator!=(const Insets& insets) const {
    return !(*this == insets);
  }

  void operator+=(const Insets& insets) {
    top_ += insets.top_;
    left_ += insets.left_;
    bottom_ += insets.bottom_;
    right_ += insets.right_;
  }

  void operator-=(const Insets& insets) {
    top_ -= insets.top_;
    left_ -= insets.left_;
    bottom_ -= insets.bottom_;
    right_ -= insets.right_;
  }

  Insets operator-() const {
    return Insets(-top_, -left_, -bottom_, -right_);
  }

  Insets Scale(float scale) const {
    return Scale(scale, scale);
  }

  Insets Scale(float x_scale, float y_scale) const {
    return Insets(static_cast<int>(top() * y_scale),
                  static_cast<int>(left() * x_scale),
                  static_cast<int>(bottom() * y_scale),
                  static_cast<int>(right() * x_scale));
  }

  // Adjusts the vertical and horizontal dimensions by the values described in
  // |vector|. Offsetting insets before applying to a rectangle would be
  // equivalent to offseting the rectangle then applying the insets.
  Insets Offset(const gfx::Vector2d& vector) const;

  operator InsetsF() const {
    return InsetsF(static_cast<float>(top()), static_cast<float>(left()),
                   static_cast<float>(bottom()), static_cast<float>(right()));
  }

  // Returns a string representation of the insets.
  std::string ToString() const;

 private:
  int top_;
  int left_;
  int bottom_;
  int right_;
};

inline Insets operator+(Insets lhs, const Insets& rhs) {
  lhs += rhs;
  return lhs;
}

inline Insets operator-(Insets lhs, const Insets& rhs) {
  lhs -= rhs;
  return lhs;
}

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_INSETS_H_
