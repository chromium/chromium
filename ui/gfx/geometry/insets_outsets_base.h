// Copyright 2022 The Chromium Authors
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
        bottom_(ClampBottomOrRight(all, all)),
        right_(ClampBottomOrRight(all, all)) {}

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
    bottom_ = ClampBottomOrRight(top_, bottom_);
    return *static_cast<T*>(this);
  }
  constexpr T& set_left(int left) {
    left_ = left;
    right_ = ClampBottomOrRight(left_, right_);
    return *static_cast<T*>(this);
  }
  constexpr T& set_bottom(int bottom) {
    bottom_ = ClampBottomOrRight(top_, bottom);
    return *static_cast<T*>(this);
  }
  constexpr T& set_right(int right) {
    right_ = ClampBottomOrRight(left_, right);
    return *static_cast<T*>(this);
  }
  // These are preferred to the above setters when setting a pair of edges
  // because these have less clamping and better performance.
  constexpr T& set_left_right(int left, int right) {
    left_ = left;
    right_ = ClampBottomOrRight(left_, right);
    return *static_cast<T*>(this);
  }
  constexpr T& set_top_bottom(int top, int bottom) {
    top_ = top;
    bottom_ = ClampBottomOrRight(top_, bottom);
    return *static_cast<T*>(this);
  }

  // In addition to the above, we can also use the following methods to
  // construct Insets/Outsets.
  // TLBR() is for Chomium UI code. We should not use it in blink code because
  // the order of parameters is different from the normal orders used in blink.
  // Blink code can use the above setters and VH().
  static constexpr T TLBR(int top, int left, int bottom, int right) {
    return T().set_top_bottom(top, bottom).set_left_right(left, right);
  }
  static constexpr T VH(int vertical, int horizontal) {
    return TLBR(vertical, horizontal, vertical, horizontal);
  }

  // Sets each side to the maximum of the side and the corresponding side of
  // |other|.
  void SetToMax(const T& other) {
    top_ = std::max(top_, other.top_);
    left_ = std::max(left_, other.left_);
    bottom_ = std::max(bottom_, other.bottom_);
    right_ = std::max(right_, other.right_);
  }

  bool operator==(const InsetsOutsetsBase<T>& other) const {
    return top_ == other.top_ && left_ == other.left_ &&
           bottom_ == other.bottom_ && right_ == other.right_;
  }

  bool operator!=(const InsetsOutsetsBase<T>& other) const {
    return !(*this == other);
  }

  void operator+=(const T& other) {
    top_ = base::ClampAdd(top_, other.top_);
    left_ = base::ClampAdd(left_, other.left_);
    bottom_ = ClampBottomOrRight(top_, base::ClampAdd(bottom_, other.bottom_));
    right_ = ClampBottomOrRight(left_, base::ClampAdd(right_, other.right_));
  }

  void operator-=(const T& other) {
    top_ = base::ClampSub(top_, other.top_);
    left_ = base::ClampSub(left_, other.left_);
    bottom_ = ClampBottomOrRight(top_, base::ClampSub(bottom_, other.bottom_));
    right_ = ClampBottomOrRight(left_, base::ClampSub(right_, other.right_));
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
  // Clamp the bottom/right to avoid integer over/underflow in width() and
  // height(). This returns the clamped bottom/right given a |top_or_left| and
  // a |bottom_or_right|.
  static constexpr int ClampBottomOrRight(int top_or_left,
                                          int bottom_or_right) {
    return base::ClampAdd(top_or_left, bottom_or_right) - top_or_left;
  }

  int top_ = 0;
  int left_ = 0;
  int bottom_ = 0;
  int right_ = 0;
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_INSETS_OUTSETS_BASE_H_
