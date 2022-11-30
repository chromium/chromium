// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_RESIZE_UTILS_H_
#define UI_GFX_GEOMETRY_RESIZE_UTILS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/geometry_export.h"

namespace gfx {

class Rect;
class Size;

enum class ResizeEdge {
  kBottom,
  kBottomLeft,
  kBottomRight,
  kLeft,
  kRight,
  kTop,
  kTopLeft,
  kTopRight
};

// Updates |rect| to adhere to the |aspect_ratio| of the window, if it has
// been set. |resize_edge| refers to the edge of the window being sized.
// |min_window_size| and |max_window_size| are expected to adhere to the
// given aspect ratio.
// |aspect_ratio| must be valid and is found using width / height.
void GEOMETRY_EXPORT SizeRectToAspectRatio(ResizeEdge resize_edge,
                                           float aspect_ratio,
                                           const Size& min_window_size,
                                           absl::optional<Size> max_window_size,
                                           Rect* rect);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_RESIZE_UTILS_H_
