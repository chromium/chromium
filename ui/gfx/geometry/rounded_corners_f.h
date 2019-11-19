// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_ROUNDED_CORNERS_F_H_
#define UI_GFX_GEOMETRY_ROUNDED_CORNERS_F_H_

#include <iosfwd>
#include <limits>
#include <string>

#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {

// Represents the geometry of a region with rounded corners, expressed as four
// corner radii in the order: top-left, top-right, bottom-right, bottom-left.
class GEOMETRY_EXPORT RoundedCornersF {
 public:
  // Creates an empty RoundedCornersF with all corners having zero radius.
  constexpr RoundedCornersF() : RoundedCornersF(0.0f) {}

  // Creates a RoundedCornersF with the same radius for all corners.
  constexpr explicit RoundedCornersF(float all)
      : RoundedCornersF(all, all, all, all) {}

  // Creates a RoundedCornersF with four different corner radii.
  constexpr RoundedCornersF(float upper_left,
                            float upper_right,
                            float lower_right,
                            float lower_left)
      : upper_left_(clamp(upper_left)),
        upper_right_(clamp(upper_right)),
        lower_right_(clamp(lower_right)),
        lower_left_(clamp(lower_left)) {}

  constexpr float upper_left() const { return upper_left_; }
  constexpr float upper_right() const { return upper_right_; }
  constexpr float lower_right() const { return lower_right_; }
  constexpr float lower_left() const { return lower_left_; }

  void set_upper_left(float upper_left) { upper_left_ = clamp(upper_left); }
  void set_upper_right(float upper_right) { upper_right_ = clamp(upper_right); }
  void set_lower_right(float lower_right) { lower_right_ = clamp(lower_right); }
  void set_lower_left(float lower_left) { lower_left_ = clamp(lower_left); }

  void Set(float upper_left,
           float upper_right,
           float lower_right,
           float lower_left) {
    upper_left_ = clamp(upper_left);
    upper_right_ = clamp(upper_right);
    lower_right_ = clamp(lower_right);
    lower_left_ = clamp(lower_left);
  }

  // Returns true if all of the corners are square (zero effective radius).
  bool IsEmpty() const {
    return upper_left_ == 0.0f && upper_right_ == 0.0f &&
           lower_right_ == 0.0f && lower_left_ == 0.0f;
  }

  bool operator==(const RoundedCornersF& corners) const {
    return upper_left_ == corners.upper_left_ &&
           upper_right_ == corners.upper_right_ &&
           lower_right_ == corners.lower_right_ &&
           lower_left_ == corners.lower_left_;
  }

  bool operator!=(const RoundedCornersF& corners) const {
    return !(*this == corners);
  }

  // Returns a string representation of the insets.
  std::string ToString() const;

 private:
  static constexpr float kTrivial = 8.f * std::numeric_limits<float>::epsilon();

  // Prevents values which are smaller than zero or negligibly small.
  // Uses the same logic as gfx::Size.
  static constexpr float clamp(float f) { return f > kTrivial ? f : 0.f; }

  float upper_left_ = 0.0f;
  float upper_right_ = 0.0f;
  float lower_right_ = 0.0f;
  float lower_left_ = 0.0f;
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_ROUNDED_CORNERS_F_H_
