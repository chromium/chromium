// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_rep_default.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/record_paint_canvas.h"
#include "cc/paint/skia_paint_canvas.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/color_palette.h"

namespace gfx {

ImageSkiaRep::ImageSkiaRep()
    : type_(ImageRepType::kImageTypeDrawable), scale_(0.0f) {}

ImageSkiaRep::ImageSkiaRep(const gfx::Size& size, float scale)
    : type_(ImageRepType::kImageTypeBitmap), scale_(scale) {
  bitmap_.allocN32Pixels(static_cast<int>(size.width() * this->scale()),
                         static_cast<int>(size.height() * this->scale()));
  bitmap_.eraseColor(kPlaceholderColor);
  bitmap_.setImmutable();
  pixel_size_.SetSize(bitmap_.width(), bitmap_.height());
  paint_image_ = cc::PaintImage::CreateFromBitmap(bitmap_);
}

ImageSkiaRep::ImageSkiaRep(const SkBitmap& src, float scale)
    : type_(ImageRepType::kImageTypeBitmap),
      pixel_size_(gfx::Size(src.width(), src.height())),
      bitmap_(src),
      scale_(scale) {
  CHECK_EQ(bitmap_.colorType(), kN32_SkColorType);
  DCHECK(!bitmap_.drawsNothing());
  bitmap_.setImmutable();
  paint_image_ = cc::PaintImage::CreateFromBitmap(src);
}

ImageSkiaRep::ImageSkiaRep(cc::PaintRecord paint_record,
                           const gfx::Size& pixel_size,
                           float scale)
    : paint_record_(std::move(paint_record)),
      type_(ImageRepType::kImageTypeDrawable),
      pixel_size_(pixel_size),
      scale_(scale) {
  DCHECK(!pixel_size.IsEmpty());
}

ImageSkiaRep::ImageSkiaRep(const ImageSkiaRep& other)
    : paint_image_(other.paint_image_),
      paint_record_(other.paint_record_),
      type_(other.type_),
      pixel_size_(other.pixel_size_),
      bitmap_(other.bitmap_),
      scale_(other.scale_) {}

ImageSkiaRep::~ImageSkiaRep() {}

int ImageSkiaRep::GetWidth() const {
  return static_cast<int>(pixel_width() / scale());
}

int ImageSkiaRep::GetHeight() const {
  return static_cast<int>(pixel_height() / scale());
}

cc::PaintRecord ImageSkiaRep::GetPaintRecord() const {
  DCHECK(type_ == ImageRepType::kImageTypeBitmap || !is_null());
  // If this image rep is of |kImageTypeDrawable| then it must have a paint
  // record.
  if (type_ == ImageRepType::kImageTypeDrawable || paint_record_)
    return *paint_record_;

  // If this ImageRep was generated using a bitmap then it may not have a
  // paint record generated for it yet. We would have to generate it now.
  cc::RecordPaintCanvas record_canvas;
  record_canvas.drawImage(paint_image(), 0, 0);
  return record_canvas.ReleaseAsRecord();
}

const SkBitmap& ImageSkiaRep::GetBitmap() const {
  if (type_ == ImageRepType::kImageTypeDrawable && bitmap_.isNull() &&
      paint_record_) {
    // TODO(malaykeshav): Add a NOTREACHED() once all instances of this call
    // path is removed from the code base.

    // A request for bitmap was made even though this ImageSkiaRep is sourced
    // form a drawable(e.g. CanvasImageSource). This should not be happenning
    // as it forces a rasterization on the UI thread.
    bitmap_.allocN32Pixels(pixel_width(), pixel_height());
    bitmap_.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap_, skia::LegacyDisplayGlobals::GetSkSurfaceProps());
    paint_record_->Playback(&canvas);
    bitmap_.setImmutable();
  }
  return bitmap_;
}

}  // namespace gfx
