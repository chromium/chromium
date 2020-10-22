// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mask_filter_info.h"

#include <sstream>

#include "ui/gfx/transform.h"

namespace gfx {

MaskFilterInfo::MaskFilterInfo(const RectF& bounds,
                               const RoundedCornersF& radii,
                               bool is_fast_rounded_corner)
    : rounded_corner_bounds_(bounds, radii),
      is_fast_rounded_corner_(is_fast_rounded_corner) {}

bool MaskFilterInfo::Transform(const gfx::Transform& transform) {
  return transform.TransformRRectF(&rounded_corner_bounds_);
}

std::string MaskFilterInfo::ToString() const {
  return "MaskFilterInfo{" + rounded_corner_bounds_.ToString() +
         ", fast_rounded_corner=" +
         (is_fast_rounded_corner_ ? "true" : "false") + "}";
}

}  // namespace gfx
