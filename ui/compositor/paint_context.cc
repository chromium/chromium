// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_context.h"

#include "ui/gfx/canvas.h"

namespace ui {

PaintContext::PaintContext(cc::DisplayItemList* list,
                           float device_scale_factor,
                           const gfx::Rect& invalidation,
                           bool is_pixel_canvas)
    : list_(list),
      device_scale_factor_(device_scale_factor),
      invalidation_(gfx::ScaleToRoundedRect(
          invalidation,
          is_pixel_canvas ? device_scale_factor_ : 1.f)),
      is_pixel_canvas_(is_pixel_canvas) {
#if DCHECK_IS_ON()
  root_visited_ = nullptr;
  inside_paint_recorder_ = false;
#endif
}

PaintContext::PaintContext(const PaintContext& other,
                           const gfx::Vector2d& offset)
    : list_(other.list_),
      device_scale_factor_(other.device_scale_factor_),
      invalidation_(other.invalidation_),
      offset_(other.offset_ + offset),
      is_pixel_canvas_(other.is_pixel_canvas_) {
#if DCHECK_IS_ON()
  root_visited_ = other.root_visited_;
  inside_paint_recorder_ = other.inside_paint_recorder_;
#endif
}

PaintContext::PaintContext(const PaintContext& other,
                           CloneWithoutInvalidation c)
    : list_(other.list_),
      device_scale_factor_(other.device_scale_factor_),
      invalidation_(),
      offset_(other.offset_),
      is_pixel_canvas_(other.is_pixel_canvas_) {
#if DCHECK_IS_ON()
  root_visited_ = other.root_visited_;
  inside_paint_recorder_ = other.inside_paint_recorder_;
#endif
}

PaintContext::~PaintContext() {
}

gfx::Rect PaintContext::ToLayerSpaceBounds(
    const gfx::Size& size_in_context) const {
  return gfx::Rect(size_in_context) + offset_;
}

gfx::Rect PaintContext::ToLayerSpaceRect(const gfx::Rect& rect) const {
  return rect + offset_;
}

}  // namespace ui
