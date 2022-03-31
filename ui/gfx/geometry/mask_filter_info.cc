// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/mask_filter_info.h"

#include <sstream>

#include "ui/gfx/geometry/transform.h"

namespace gfx {

bool MaskFilterInfo::Transform(const gfx::Transform& transform) {
  if (rounded_corner_bounds_.IsEmpty())
    return false;

  if (!transform.TransformRRectF(&rounded_corner_bounds_))
    return false;

  gradient_mask_.Transform(transform);
  return true;
}

std::string MaskFilterInfo::ToString() const {
  return "MaskFilterInfo{" + rounded_corner_bounds_.ToString() +
         ", gradient_mask=" + gradient_mask_.ToString() + "}";
}

}  // namespace gfx
