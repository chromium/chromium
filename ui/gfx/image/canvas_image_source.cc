// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/canvas_image_source.h"

#include "base/logging.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/record_paint_canvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/switches.h"

namespace gfx {

namespace {

class PaddedImageSource : public CanvasImageSource {
 public:
  PaddedImageSource(const ImageSkia& image, const Insets& insets)
      : CanvasImageSource(Size(image.width() + insets.width(),
                               image.height() + insets.height())),
        image_(image),
        insets_(insets) {}

  // CanvasImageSource:
  void Draw(Canvas* canvas) override {
    canvas->DrawImageInt(image_, insets_.left(), insets_.top());
  }

 private:
  const ImageSkia image_;
  const Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(PaddedImageSource);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CanvasImageSource

// static
ImageSkia CanvasImageSource::CreatePadded(const ImageSkia& image,
                                          const Insets& insets) {
  return MakeImageSkia<PaddedImageSource>(image, insets);
}

CanvasImageSource::CanvasImageSource(const Size& size) : size_(size) {}

ImageSkiaRep CanvasImageSource::GetImageForScale(float scale) {
  scoped_refptr<cc::DisplayItemList> display_item_list =
      base::MakeRefCounted<cc::DisplayItemList>(
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer);
  display_item_list->StartPaint();

  SizeF size_in_pixel = ScaleSize(SizeF(size_), scale);
  cc::RecordPaintCanvas record_canvas(
      display_item_list.get(),
      SkRect::MakeWH(SkFloatToScalar(size_in_pixel.width()),
                     SkFloatToScalar(size_in_pixel.height())));
  gfx::Canvas canvas(&record_canvas, scale);
#if DCHECK_IS_ON()
  Rect clip_rect;
  DCHECK(canvas.GetClipBounds(&clip_rect));
  DCHECK(clip_rect.Contains(gfx::Rect(ToCeiledSize(size_in_pixel))));
#endif
  canvas.Scale(scale, scale);
  Draw(&canvas);

  display_item_list->EndPaintOfPairedEnd();
  display_item_list->Finalize();
  return ImageSkiaRep(display_item_list->ReleaseAsRecord(),
                      gfx::ScaleToCeiledSize(size_, scale), scale);
}

}  // namespace gfx
