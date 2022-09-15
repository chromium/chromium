// Copyright 2020 The Chromium Authors
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

  if (gradient_mask_ && !gradient_mask_->IsEmpty())
    gradient_mask_->Transform(transform);

  return true;
}

std::string MaskFilterInfo::ToString() const {
  std::string result = "MaskFilterInfo{" + rounded_corner_bounds_.ToString();

  if (gradient_mask_)
    result += ", gradient_mask=" + gradient_mask_->ToString() + "}";

  return result;
}

}  // namespace gfx
