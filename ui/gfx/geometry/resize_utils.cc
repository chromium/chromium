// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/resize_utils.h"

#include <algorithm>

#include "base/check_op.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {

void SizeMinMaxToAspectRatio(float aspect_ratio,
                             Size* min_window_size,
                             Size* max_window_size) {
  DCHECK_GT(aspect_ratio, 0.0f);

  // Calculate the height using the min-width and aspect ratio.
  int min_height = min_window_size->width() / aspect_ratio;
  if (min_height < min_window_size->height()) {
    // The supplied width is too small to honor the min size, so use the height
    // to determine the minimum width.
    min_window_size->set_width(min_window_size->height() * aspect_ratio);
  } else {
    min_window_size->set_height(min_height);
  }

  // Calculate the height using the max-width and aspect ratio.
  int max_height = max_window_size->width() / aspect_ratio;
  if (max_height > max_window_size->height()) {
    // The supplied width is too large to honor the max size, so use the height
    // to determine the maximum width.
    max_window_size->set_width(max_window_size->height() * aspect_ratio);
  } else {
    max_window_size->set_height(max_height);
  }

  DCHECK_GE(max_window_size->width(), min_window_size->width());
  DCHECK_GE(max_window_size->height(), min_window_size->height());
}

void SizeRectToAspectRatio(ResizeEdge resize_edge,
                           float aspect_ratio,
                           const Size& min_window_size,
                           const Size& max_window_size,
                           Rect* rect) {
  DCHECK_GT(aspect_ratio, 0.0f);
  DCHECK_GE(max_window_size.width(), min_window_size.width());
  DCHECK_GE(max_window_size.height(), min_window_size.height());

  float rect_width = 0.0;
  float rect_height = 0.0;
  if (resize_edge == ResizeEdge::kLeft || resize_edge == ResizeEdge::kRight ||
      resize_edge == ResizeEdge::kTopLeft ||
      resize_edge == ResizeEdge::kBottomLeft) { /* horizontal axis to pivot */
    rect_width = std::min(max_window_size.width(),
                          std::max(rect->width(), min_window_size.width()));
    rect_height = rect_width / aspect_ratio;
  } else { /* vertical axis to pivot */
    rect_height = std::min(max_window_size.height(),
                           std::max(rect->height(), min_window_size.height()));
    rect_width = rect_height * aspect_ratio;
  }

  // |rect| bounds before sizing to aspect ratio.
  int left = rect->x();
  int top = rect->y();
  int right = rect->right();
  int bottom = rect->bottom();

  switch (resize_edge) {
    case ResizeEdge::kRight:
    case ResizeEdge::kBottom:
      right = rect_width + left;
      bottom = top + rect_height;
      break;
    case ResizeEdge::kTop:
      right = rect_width + left;
      top = bottom - rect_height;
      break;
    case ResizeEdge::kLeft:
    case ResizeEdge::kTopLeft:
      left = right - rect_width;
      top = bottom - rect_height;
      break;
    case ResizeEdge::kTopRight:
      right = left + rect_width;
      top = bottom - rect_height;
      break;
    case ResizeEdge::kBottomLeft:
      left = right - rect_width;
      bottom = top + rect_height;
      break;
    case ResizeEdge::kBottomRight:
      right = left + rect_width;
      bottom = top + rect_height;
      break;
  }

  rect->SetByBounds(left, top, right, bottom);
}

}  // namespace gfx
