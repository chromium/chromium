// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/rrect_f_builder.h"

namespace gfx {

RRectFBuilder::RRectFBuilder() = default;
RRectFBuilder::RRectFBuilder(RRectFBuilder&& other) = default;

RRectF RRectFBuilder::Build() {
  return RRectF(x_, y_, width_, height_, upper_left_x_, upper_left_y_,
                upper_right_x_, upper_right_y_, lower_right_x_, lower_right_y_,
                lower_left_x_, lower_left_y_);
}

}  // namespace gfx
