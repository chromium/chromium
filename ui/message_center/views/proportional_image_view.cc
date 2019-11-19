// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/proportional_image_view.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/message_center/message_center_style.h"

namespace message_center {

const char ProportionalImageView::kViewClassName[] = "ProportionalImageView";

ProportionalImageView::ProportionalImageView(const gfx::Size& view_size) {
  SetPreferredSize(view_size);
}

ProportionalImageView::~ProportionalImageView() {}

void ProportionalImageView::SetImage(const gfx::ImageSkia& image,
                                     const gfx::Size& max_image_size) {
  image_ = image;
  max_image_size_ = max_image_size;
  SchedulePaint();
}

void ProportionalImageView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Size draw_size = GetImageDrawingSize();
  if (draw_size.IsEmpty())
    return;

  gfx::Rect draw_bounds = GetContentsBounds();
  draw_bounds.ClampToCenteredSize(draw_size);

  gfx::ImageSkia image =
      (image_.size() == draw_size)
          ? image_
          : gfx::ImageSkiaOperations::CreateResizedImage(
                image_, skia::ImageOperations::RESIZE_BEST, draw_size);
  canvas->DrawImageInt(image, draw_bounds.x(), draw_bounds.y());
}

const char* ProportionalImageView::GetClassName() const {
  return kViewClassName;
}

gfx::Size ProportionalImageView::GetImageDrawingSize() {
  if (!GetVisible())
    return gfx::Size();

  gfx::Size max_size = max_image_size_;
  max_size.SetToMin(GetContentsBounds().size());
  return GetImageSizeForContainerSize(max_size, image_.size());
}

}  // namespace message_center
