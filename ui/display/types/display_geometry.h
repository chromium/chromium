// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_GEOMETRY_H_
#define UI_DISPLAY_TYPES_DISPLAY_GEOMETRY_H_

#include "ui/display/types/display_types_export.h"
#include "ui/gfx/geometry/rect.h"

namespace display {

struct DISPLAY_TYPES_EXPORT DisplayGeometry {
  bool operator==(const DisplayGeometry& other) const {
    return bounds_px == other.bounds_px && scale == other.scale;
  }

  gfx::Rect bounds_px;
  float scale;
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_GEOMETRY_H_
