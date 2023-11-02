// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/canvas_painter.h"

#include "cc/paint/display_item_list.h"

namespace ui {

CanvasPainter::CanvasPainter(SkBitmap* output,
                             const gfx::Size& output_size,
                             float device_scale_factor,
                             SkColor clear_color,
                             bool is_pixel_canvas)
    : output_(output),
      pixel_output_size_(
          gfx::ScaleToCeiledSize(output_size, device_scale_factor)),
      raster_scale_(is_pixel_canvas ? 1.f : device_scale_factor),
      clear_color_(clear_color),
      list_(new cc::DisplayItemList),
      context_(list_.get(),
               device_scale_factor,
               gfx::Rect(output_size),
               is_pixel_canvas) {}

CanvasPainter::~CanvasPainter() {
  SkImageInfo info =
      SkImageInfo::MakeN32(pixel_output_size_.width(),
                           pixel_output_size_.height(), kPremul_SkAlphaType);
  if (!output_->tryAllocPixels(info))
    return;

  SkCanvas canvas(*output_, SkSurfaceProps{});
  canvas.clear(clear_color_);

  // When pixel canvas is enabled, the recordings and canvas are already scaled
  // to the correct raster size. This additional scaling is not required and
  // hence |raster_scale_| should be equal to 1 during this operation.
  canvas.scale(raster_scale_, raster_scale_);

  list_->Finalize();
  list_->Raster(&canvas, nullptr);
}

}  // namespace ui
