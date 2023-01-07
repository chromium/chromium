// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/resize_utils.h"

#include <ostream>

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
namespace {

// This function decides whether SizeRectToAspectRatio() will adjust the height
// to match the specified width (resizing horizontally) or vice versa (resizing
// vertically).
bool IsResizingHorizontally(ResizeEdge resize_edge) {
  switch (resize_edge) {
    case ResizeEdge::kLeft:
    case ResizeEdge::kRight:
    case ResizeEdge::kTopLeft:
    case ResizeEdge::kBottomLeft:
      return true;
    default:
      return false;
  }
}

}  // namespace

void SizeRectToAspectRatio(ResizeEdge resize_edge,
                           float aspect_ratio,
                           const Size& min_window_size,
                           absl::optional<Size> max_window_size,
                           Rect* rect) {
  DCHECK_GT(aspect_ratio, 0.0f);
  if (max_window_size.has_value()) {
    DCHECK_GE(max_window_size->width(), min_window_size.width());
    DCHECK_GE(max_window_size->height(), min_window_size.height());
    DCHECK(Rect(rect->origin(), *max_window_size).Contains(*rect))
        << rect->ToString() << " is larger than the maximum size "
        << max_window_size->ToString();
  }
  DCHECK(rect->Contains(Rect(rect->origin(), min_window_size)))
      << rect->ToString() << " is smaller than the minimum size "
      << min_window_size.ToString();

  Size new_size = rect->size();
  if (IsResizingHorizontally(resize_edge)) {
    new_size.set_height(base::ClampRound(new_size.width() / aspect_ratio));
    if (min_window_size.height() > new_size.height() ||
        (max_window_size.has_value() &&
         new_size.height() > max_window_size->height())) {
      if (max_window_size.has_value()) {
        new_size.set_height(base::clamp(new_size.height(),
                                        min_window_size.height(),
                                        max_window_size->height()));
      } else {
        new_size.set_height(min_window_size.height());
      }
      new_size.set_width(base::ClampRound(new_size.height() * aspect_ratio));
    }
  } else {
    new_size.set_width(base::ClampRound(new_size.height() * aspect_ratio));
    if (min_window_size.width() > new_size.width() ||
        (max_window_size.has_value() &&
         new_size.width() > max_window_size->width())) {
      if (max_window_size.has_value()) {
        new_size.set_width(base::clamp(new_size.width(),
                                       min_window_size.width(),
                                       max_window_size->width()));
      } else {
        new_size.set_width(min_window_size.width());
      }
      new_size.set_height(base::ClampRound(new_size.width() / aspect_ratio));
    }
  }

  // The dimensions might still be outside of the allowed ranges at this point.
  // This happens when the aspect ratio makes it impossible to fit |rect|
  // within the size limits without letter-/pillarboxing.
  if (max_window_size.has_value())
    new_size.SetToMin(*max_window_size);

  new_size.SetToMax(min_window_size);

  // |rect| bounds before sizing to aspect ratio.
  int left = rect->x();
  int top = rect->y();
  int right = rect->right();
  int bottom = rect->bottom();

  switch (resize_edge) {
    case ResizeEdge::kRight:
    case ResizeEdge::kBottom:
      right = new_size.width() + left;
      bottom = top + new_size.height();
      break;
    case ResizeEdge::kTop:
      right = new_size.width() + left;
      top = bottom - new_size.height();
      break;
    case ResizeEdge::kLeft:
    case ResizeEdge::kTopLeft:
      left = right - new_size.width();
      top = bottom - new_size.height();
      break;
    case ResizeEdge::kTopRight:
      right = left + new_size.width();
      top = bottom - new_size.height();
      break;
    case ResizeEdge::kBottomLeft:
      left = right - new_size.width();
      bottom = top + new_size.height();
      break;
    case ResizeEdge::kBottomRight:
      right = left + new_size.width();
      bottom = top + new_size.height();
      break;
  }

  rect->SetByBounds(left, top, right, bottom);
}

}  // namespace gfx
