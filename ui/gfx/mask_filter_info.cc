// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mask_filter_info.h"

#include <sstream>

#include "ui/gfx/transform.h"

namespace gfx {

bool MaskFilterInfo::Transform(const gfx::Transform& transform) {
  return rounded_corner_bounds_.IsEmpty()
             ? false
             : transform.TransformRRectF(&rounded_corner_bounds_);
}

std::string MaskFilterInfo::ToString() const {
  return "MaskFilterInfo{" + rounded_corner_bounds_.ToString() + "}";
}

}  // namespace gfx
