// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_INSETS_OUTSETS_BASE_H_
#define UI_GFX_GEOMETRY_INSETS_OUTSETS_BASE_H_

#include <string>

#include "base/numerics/clamped_math.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {

// The common base template class for Insets and Outsets.
// Represents the widths of the four borders or margins of an unspecified
// rectangle. It stores the thickness of the top, left, bottom and right
// edges, without storing the actual size and position of the rectangle itself.
template <typename T>
class InsetsOutsetsBase {
 public:
  constexpr InsetsOutsetsBase() = default;
  constexpr explicit InsetsOutsetsBase(int all)
      : top_(all),
        left_(all),
        bottom_(GetClampedValue(all, all)),
        right_(GetClampedValue(all, all)) {}

  constexpr int top() const { return top_; }
  constexpr int left() const { return left_; }
  constexpr int bottom() const { return bottom_; }
  constexpr int right() const { return right_; }

  // Returns the total width taken up by the insets/outsets, which is the sum
  // of the left and right insets/outsets.
  constexpr int width() const { return left_ + right_; }

  // Returns the total height taken up by the insets/outsets, which is the sum
  // of the top and bottom insets/outsets.
  constexpr int height() const { return top_ + bottom_; }

  // Returns the sum of the left and right insets/outsets as the width,
  // the sum of the top and bottom insets/outsets as the height.
  constexpr Size size() const { return Size(width(), height()); }

  // Returns true if the insets/outsets are empty.
  bool IsEmpty() const { return width() == 0 && height() == 0; }

  // These setters can be used together with the default constructor and the
  // single-parameter constructor to construct Insets instances, for example:
  //                                                  // T, L, B, R
  //   Insets a = Insets().set_top(2);                // 2, 0, 0, 0
  //   Insets b = Insets().set_left(2).set_bottom(3); // 0, 2, 3, 0
  //   Insets c = Insets().set_left_right(1, 2).set_top_bottom(3, 4);
  //                                                  // 3, 1, 4, 2
  //   Insets d = Insets(1).set_top(5);               // 5, 1, 1, 1
  constexpr T& set_top(int top) {
    top_ = top;
    bottom_ = GetClampedValue(top_, bottom_);
    return *static_cast<T*>(this);
  }
  constexpr T& set_left(int left) {
    left_ = left;
    right_ = GetClampedValue(left_, right_);
    return *static_cast<T*>(this);
  }
  constexpr T& set_bottom(int bottom) {
    bottom_ = GetClampedValue(top_, bottom);
    return *static_cast<T*>(this);
  }
  constexpr T& set_right(int right) {
    right_ = GetClampedValue(left_, right);
    return *static_cast<T*>(this);
  }
  // These are preferred to the above setters when setting a pair of edges
  // because these have less clamping and better performance.
  constexpr T& set_left_right(int left, int right) {
    left_ = left;
    right_ = GetClampedValue(left_, right);
    return *static_cast<T*>(this);
  }
  constexpr T& set_top_bottom(int top, int bottom) {
    top_ = top;
    bottom_ = GetClampedValue(top_, bottom);
    return *static_cast<T*>(this);
  }

  // Sets each side to the maximum of the side and the corresponding side of
  // |other|.
  void SetToMax(const T& other) {
    top_ = std::max(top_, other.top_);
    left_ = std::max(left_, other.left_);
    bottom_ = std::max(bottom_, other.bottom_);
    right_ = std::max(right_, other.right_);
  }

  bool operator==(const T& other) const {
    return top_ == other.top_ && left_ == other.left_ &&
           bottom_ == other.bottom_ && right_ == other.right_;
  }

  bool operator!=(const T& other) const { return !(*this == other); }

  void operator+=(const T& other) {
    top_ = base::ClampAdd(top_, other.top_);
    left_ = base::ClampAdd(left_, other.left_);
    bottom_ = GetClampedValue(top_, base::ClampAdd(bottom_, other.bottom_));
    right_ = GetClampedValue(left_, base::ClampAdd(right_, other.right_));
  }

  void operator-=(const T& other) {
    top_ = base::ClampSub(top_, other.top_);
    left_ = base::ClampSub(left_, other.left_);
    bottom_ = GetClampedValue(top_, base::ClampSub(bottom_, other.bottom_));
    right_ = GetClampedValue(left_, base::ClampSub(right_, other.right_));
  }

  T operator-() const {
    return T()
        .set_left_right(-base::MakeClampedNum(left_),
                        -base::MakeClampedNum(right_))
        .set_top_bottom(-base::MakeClampedNum(top_),
                        -base::MakeClampedNum(bottom_));
  }

  // Returns a string representation of the insets/outsets.
  std::string ToString() const {
    return base::StringPrintf("x:%d,%d y:%d,%d", left_, right_, top_, bottom_);
  }

 private:
  // Returns true iff a+b would overflow max int.
  static constexpr bool AddWouldOverflow(int a, int b) {
    // In this function, GCC tries to make optimizations that would only work if
    // max - a wouldn't overflow but it isn't smart enough to notice that a > 0.
    // So cast everything to unsigned to avoid this.  As it is guaranteed that
    // max - a and b are both already positive, the cast is a noop.
    //
    // This is intended to be: a > 0 && max - a < b
    return a > 0 && b > 0 &&
           static_cast<unsigned>(std::numeric_limits<int>::max() - a) <
               static_cast<unsigned>(b);
  }

  // Returns true iff a+b would underflow min int.
  static constexpr bool AddWouldUnderflow(int a, int b) {
    return a < 0 && b < 0 && std::numeric_limits<int>::min() - a > b;
  }

  // Clamp the right/bottom to avoid integer over/underflow in width() and
  // height(). This returns the right/bottom given a top_or_left and a
  // bottom_or_right.
  // TODO(enne): this should probably use base::ClampAdd, but that
  // function is not a constexpr.
  static constexpr int GetClampedValue(int top_or_left, int bottom_or_right) {
    if (AddWouldOverflow(top_or_left, bottom_or_right)) {
      return std::numeric_limits<int>::max() - top_or_left;
    } else if (AddWouldUnderflow(top_or_left, bottom_or_right)) {
      // If |top_or_left| and |bottom_or_right| are both negative,
      // adds |top_or_left| to prevent underflow by subtracting it.
      return std::numeric_limits<int>::min() - top_or_left;
    } else {
      return bottom_or_right;
    }
  }

  int top_ = 0;
  int left_ = 0;
  int bottom_ = 0;
  int right_ = 0;
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_INSETS_OUTSETS_BASE_H_
