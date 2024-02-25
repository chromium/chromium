// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/resize_utils.h"

#include <algorithm>
#include <ostream>

#include "base/check_op.h"
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

void SizeRectToAspectRatioWithExcludedMargin(
    ResizeEdge resize_edge,
    float aspect_ratio,
    const Size& original_min_window_size,
    std::optional<Size> max_window_size,
    const Size& excluded_margin,
    Rect& rect) {
  DCHECK_GT(aspect_ratio, 0.0f);
  if (max_window_size.has_value()) {
    DCHECK_GE(max_window_size->width(), original_min_window_size.width());
    DCHECK_GE(max_window_size->height(), original_min_window_size.height());
    DCHECK_GE(max_window_size->width(), excluded_margin.width());
    DCHECK_GE(max_window_size->height(), excluded_margin.height());
    DCHECK(Rect(rect.origin(), *max_window_size).Contains(rect))
        << rect.ToString() << " is larger than the maximum size "
        << max_window_size->ToString();
  }
  DCHECK(rect.Contains(Rect(rect.origin(), original_min_window_size)))
      << rect.ToString() << " is smaller than the minimum size "
      << original_min_window_size.ToString();

  // Compute the aspect ratio with the excluded margin removed from both the
  // rectangle and the maximum size. Note that the edge we ask for doesn't
  // really matter; we'll position the resulting rectangle correctly later.
  Size new_size(rect.width() - excluded_margin.width(),
                rect.height() - excluded_margin.height());
  if (max_window_size) {
    max_window_size.emplace(
        max_window_size->width() - excluded_margin.width(),
        max_window_size->height() - excluded_margin.height());
  }

  // Also remove the margin from the minimum size, since it'll get added back at
  // the end.
  const Size min_window_size = original_min_window_size - excluded_margin;

  if (IsResizingHorizontally(resize_edge)) {
    new_size.set_height(base::ClampRound(new_size.width() / aspect_ratio));
    if (min_window_size.height() > new_size.height() ||
        (max_window_size.has_value() &&
         new_size.height() > max_window_size->height())) {
      if (max_window_size.has_value()) {
        new_size.set_height(std::clamp(new_size.height(),
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
        new_size.set_width(std::clamp(new_size.width(), min_window_size.width(),
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

  // The minimum size also excludes any excluded margin, so the content area has
  // to make up the adjusted difference.
  new_size.SetToMax(min_window_size);

  // Now add the excluded margin back to the total size, so that the total size
  // is aligned with the resize edge.
  new_size.Enlarge(excluded_margin.width(), excluded_margin.height());

  // |rect| bounds before sizing to aspect ratio.
  int left = rect.x();
  int top = rect.y();
  int right = rect.right();
  int bottom = rect.bottom();

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

  rect.SetByBounds(left, top, right, bottom);
}

void SizeRectToAspectRatio(ResizeEdge resize_edge,
                           float aspect_ratio,
                           const Size& min_window_size,
                           std::optional<Size> max_window_size,
                           Rect* rect) {
  SizeRectToAspectRatioWithExcludedMargin(
      resize_edge, aspect_ratio, min_window_size, std::move(max_window_size),
      gfx::Size(), *rect);
}

}  // namespace gfx
