// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_INSETS_OUTSETS_F_BASE_H_
#define UI_GFX_GEOMETRY_INSETS_OUTSETS_F_BASE_H_

#include <string>

#include "base/strings/stringprintf.h"

namespace gfx {

// This is the base template class of InsetsF and OutsetsF.
template <typename T>
class InsetsOutsetsFBase {
 public:
  constexpr InsetsOutsetsFBase() = default;
  constexpr explicit InsetsOutsetsFBase(float all)
      : top_(all), left_(all), bottom_(all), right_(all) {}

  constexpr float top() const { return top_; }
  constexpr float left() const { return left_; }
  constexpr float bottom() const { return bottom_; }
  constexpr float right() const { return right_; }

  // Returns the total width taken up by the insets/outsets, which is the
  // sum of the left and right insets/outsets.
  constexpr float width() const { return left_ + right_; }

  // Returns the total height taken up by the insets/outsets, which is the
  // sum of the top and bottom insets/outsets.
  constexpr float height() const { return top_ + bottom_; }

  // Returns true if the insets/outsets are empty.
  bool IsEmpty() const { return width() == 0.f && height() == 0.f; }

  // These setters can be used together with the default constructor and the
  // single-parameter constructor to construct InsetsF instances, for example:
  //                                                    // T, L, B, R
  //   InsetsF a = InsetsF().set_top(2);                // 2, 0, 0, 0
  //   InsetsF b = InsetsF().set_left(2).set_bottom(3); // 0, 2, 3, 0
  //   InsetsF c = InsetsF(1).set_top(5);               // 5, 1, 1, 1
  constexpr T& set_top(float top) {
    top_ = top;
    return *static_cast<T*>(this);
  }
  constexpr T& set_left(float left) {
    left_ = left;
    return *static_cast<T*>(this);
  }
  constexpr T& set_bottom(float bottom) {
    bottom_ = bottom;
    return *static_cast<T*>(this);
  }
  constexpr T& set_right(float right) {
    right_ = right;
    return *static_cast<T*>(this);
  }

  // In addition to the above, we can also use the following methods to
  // construct InsetsF/OutsetsF.
  // TLBR() is for Chomium UI code. We should not use it in blink code because
  // the order of parameters is different from the normal orders used in blink.
  // Blink code can use the above setters and VH().
  static constexpr inline T TLBR(float top,
                                 float left,
                                 float bottom,
                                 float right) {
    return T().set_top(top).set_left(left).set_bottom(bottom).set_right(right);
  }
  static constexpr inline T VH(float vertical, float horizontal) {
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

  void Scale(float x_scale, float y_scale) {
    top_ *= y_scale;
    left_ *= x_scale;
    bottom_ *= y_scale;
    right_ *= x_scale;
  }
  void Scale(float scale) { Scale(scale, scale); }

  bool operator==(const InsetsOutsetsFBase<T>& other) const {
    return top_ == other.top_ && left_ == other.left_ &&
           bottom_ == other.bottom_ && right_ == other.right_;
  }

  bool operator!=(const InsetsOutsetsFBase<T>& other) const {
    return !(*this == other);
  }

  void operator+=(const T& other) {
    top_ += other.top_;
    left_ += other.left_;
    bottom_ += other.bottom_;
    right_ += other.right_;
  }

  void operator-=(const T& other) {
    top_ -= other.top_;
    left_ -= other.left_;
    bottom_ -= other.bottom_;
    right_ -= other.right_;
  }

  T operator-() const {
    return T().set_left(-left_).set_right(-right_).set_top(-top_).set_bottom(
        -bottom_);
  }

  // Returns a string representation of the insets/outsets.
  std::string ToString() const {
    return base::StringPrintf("x:%g,%g y:%g,%g", left_, right_, top_, bottom_);
  }

 private:
  float top_ = 0.f;
  float left_ = 0.f;
  float bottom_ = 0.f;
  float right_ = 0.f;
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_INSETS_OUTSETS_F_BASE_H_
