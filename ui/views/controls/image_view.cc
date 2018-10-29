// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/image_view.h"

#include <utility>

#include "base/logging.h"
#include "cc/paint/paint_flags.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/canvas.h"

namespace views {

namespace {

// Returns the pixels for the bitmap in |image| at scale |image_scale|.
void* GetBitmapPixels(const gfx::ImageSkia& img, float image_scale) {
  DCHECK_NE(0.0f, image_scale);
  return img.GetRepresentation(image_scale).GetBitmap().getPixels();
}

}  // namespace

// static
const char ImageView::kViewClassName[] = "ImageView";

ImageView::ImageView() = default;

ImageView::~ImageView() = default;

void ImageView::SetImage(const gfx::ImageSkia& img) {
  if (IsImageEqual(img))
    return;

  last_painted_bitmap_pixels_ = nullptr;
  gfx::Size pref_size(GetPreferredSize());
  image_ = img;
  scaled_image_ = gfx::ImageSkia();
  if (pref_size != GetPreferredSize())
    PreferredSizeChanged();
  SchedulePaint();
}

void ImageView::SetImage(const gfx::ImageSkia* image_skia) {
  if (image_skia) {
    SetImage(*image_skia);
  } else {
    gfx::ImageSkia t;
    SetImage(t);
  }
}

const gfx::ImageSkia& ImageView::GetImage() const {
  return image_;
}

bool ImageView::IsImageEqual(const gfx::ImageSkia& img) const {
  // Even though we copy ImageSkia in SetImage() the backing store
  // (ImageSkiaStorage) is not copied and may have changed since the last call
  // to SetImage(). The expectation is that SetImage() with different pixels is
  // treated as though the image changed. For this reason we compare not only
  // the backing store but also the pixels of the last image we painted.
  return image_.BackedBySameObjectAs(img) &&
      last_paint_scale_ != 0.0f &&
      last_painted_bitmap_pixels_ == GetBitmapPixels(img, last_paint_scale_);
}

gfx::Size ImageView::GetImageSize() const {
  return image_size_.value_or(image_.size());
}

void ImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  OnPaintImage(canvas);
}

const char* ImageView::GetClassName() const {
  return kViewClassName;
}

void ImageView::OnPaintImage(gfx::Canvas* canvas) {
  last_paint_scale_ = canvas->image_scale();
  last_painted_bitmap_pixels_ = nullptr;

  gfx::ImageSkia image = GetPaintImage(last_paint_scale_);
  if (image.isNull())
    return;

  gfx::Rect image_bounds(GetImageBounds());
  if (image_bounds.IsEmpty())
    return;

  if (image_bounds.size() != gfx::Size(image.width(), image.height())) {
    // Resize case
    cc::PaintFlags flags;
    flags.setFilterQuality(kLow_SkFilterQuality);
    canvas->DrawImageInt(image, 0, 0, image.width(), image.height(),
                         image_bounds.x(), image_bounds.y(),
                         image_bounds.width(), image_bounds.height(), true,
                         flags);
  } else {
    canvas->DrawImageInt(image, image_bounds.x(), image_bounds.y());
  }
  last_painted_bitmap_pixels_ = GetBitmapPixels(image, last_paint_scale_);
}

gfx::ImageSkia ImageView::GetPaintImage(float scale) {
  if (image_.isNull())
    return image_;

  const gfx::ImageSkiaRep& rep = image_.GetRepresentation(scale);
  if (rep.scale() == scale)
    return image_;

  if (scaled_image_.HasRepresentation(scale))
    return scaled_image_;

  // Only caches one image rep for the current scale.
  scaled_image_ = gfx::ImageSkia();

  gfx::Size scaled_size =
      gfx::ScaleToCeiledSize(rep.pixel_size(), scale / rep.scale());
  scaled_image_.AddRepresentation(gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(rep.GetBitmap(),
                                    skia::ImageOperations::RESIZE_BEST,
                                    scaled_size.width(), scaled_size.height()),
      scale));
  return scaled_image_;
}

}  // namespace views
