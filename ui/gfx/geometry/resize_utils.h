// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_RESIZE_UTILS_H_
#define UI_GFX_GEOMETRY_RESIZE_UTILS_H_

#include <optional>

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
                                           std::optional<Size> max_window_size,
                                           Rect* rect);

// As above, but computes a size for `rect` such that it has the right aspect
// ratio after subtracting `excluded_margin` from it.  This lets the aspect
// ratio ignore fixed borders, like a title bar.  For example, if
// `excluded_margin` is (10, 5) and `aspect_ratio` is 1.0f, then the resulting
// rectangle might have a size of (30, 25) or (40, 35).  One could use the
// margin for drawing in the edges, and the part that's left over would have the
// proper aspect ratio: 20/20 or 30/30, respectively.
void GEOMETRY_EXPORT
SizeRectToAspectRatioWithExcludedMargin(ResizeEdge resize_edge,
                                        float aspect_ratio,
                                        const Size& min_window_size,
                                        std::optional<Size> max_window_size,
                                        const Size& excluded_margin,
                                        Rect& rect);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_RESIZE_UTILS_H_
