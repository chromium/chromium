// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RRECT_F_BUILDER_H_
#define UI_GFX_RRECT_F_BUILDER_H_

#include "ui/gfx/rrect_f.h"

namespace gfx {

// RRectFBuilder is implemented to make the parameter setting easier for RRectF.
//
// For example: To build an RRectF at point(40, 50) with size(60,70),
// with corner radii {(1, 2),(3, 4),(5, 6),(7, 8)}, use:
//
//  RRectF a = RRectFBuilder()
//                  .set_origin(40, 50)
//                  .set_size(60, 70)
//                  .set_upper_left(1, 2)
//                  .set_upper_right(3, 4)
//                  .set_lower_right(5, 6)
//                  .set_lower_left(7, 8)
//                  .Build();
class GEOMETRY_SKIA_EXPORT RRectFBuilder {
 public:
  RRectFBuilder();
  RRectFBuilder(RRectFBuilder&& other);
  ~RRectFBuilder() = default;

  RRectFBuilder&& set_origin(float x, float y) {
    x_ = x;
    y_ = y;
    return std::move(*this);
  }
  RRectFBuilder&& set_origin(const PointF& origin) {
    x_ = origin.x();
    y_ = origin.y();
    return std::move(*this);
  }

  RRectFBuilder&& set_size(float width, float height) {
    width_ = width;
    height_ = height;
    return std::move(*this);
  }
  RRectFBuilder&& set_size(const SizeF& size) {
    width_ = size.width();
    height_ = size.height();
    return std::move(*this);
  }

  RRectFBuilder&& set_rect(const gfx::RectF& rect) {
    x_ = rect.x();
    y_ = rect.y();
    width_ = rect.width();
    height_ = rect.height();
    return std::move(*this);
  }
  template <class T>
  void set_rect(const T&) = delete;  // To avoid implicit conversion.

  RRectFBuilder&& set_radius(float radius) {
    set_upper_left(radius, radius);
    set_upper_right(radius, radius);
    set_lower_right(radius, radius);
    set_lower_left(radius, radius);
    return std::move(*this);
  }
  RRectFBuilder&& set_radius(float x_rad, float y_rad) {
    set_upper_left(x_rad, y_rad);
    set_upper_right(x_rad, y_rad);
    set_lower_right(x_rad, y_rad);
    set_lower_left(x_rad, y_rad);
    return std::move(*this);
  }

  RRectFBuilder&& set_upper_left(float upper_left_x, float upper_left_y) {
    upper_left_x_ = upper_left_x;
    upper_left_y_ = upper_left_y;
    return std::move(*this);
  }
  RRectFBuilder&& set_upper_right(float upper_right_x, float upper_right_y) {
    upper_right_x_ = upper_right_x;
    upper_right_y_ = upper_right_y;
    return std::move(*this);
  }
  RRectFBuilder&& set_lower_right(float lower_right_x, float lower_right_y) {
    lower_right_x_ = lower_right_x;
    lower_right_y_ = lower_right_y;
    return std::move(*this);
  }
  RRectFBuilder&& set_lower_left(float lower_left_x, float lower_left_y) {
    lower_left_x_ = lower_left_x;
    lower_left_y_ = lower_left_y;
    return std::move(*this);
  }

  RRectFBuilder&& set_corners(const gfx::RoundedCornersF& corners) {
    upper_left_x_ = corners.upper_left();
    upper_left_y_ = corners.upper_left();
    upper_right_x_ = corners.upper_right();
    upper_right_y_ = corners.upper_right();
    lower_right_x_ = corners.lower_right();
    lower_right_y_ = corners.lower_right();
    lower_left_x_ = corners.lower_left();
    lower_left_y_ = corners.lower_left();
    return std::move(*this);
  }

  RRectF Build();

 private:
  float x_ = 0.f;
  float y_ = 0.f;
  float width_ = 0.f;
  float height_ = 0.f;
  float upper_left_x_ = 0.f;
  float upper_left_y_ = 0.f;
  float upper_right_x_ = 0.f;
  float upper_right_y_ = 0.f;
  float lower_right_x_ = 0.f;
  float lower_right_y_ = 0.f;
  float lower_left_x_ = 0.f;
  float lower_left_y_ = 0.f;
};

}  // namespace gfx

#endif  // UI_GFX_RRECT_F_BUILDER_H_
