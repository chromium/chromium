// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/canvas_image_source.h"

#include "base/check_op.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/record_paint_canvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
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

  PaddedImageSource(const PaddedImageSource&) = delete;
  PaddedImageSource& operator=(const PaddedImageSource&) = delete;

  // CanvasImageSource:
  void Draw(Canvas* canvas) override {
    canvas->DrawImageInt(image_, insets_.left(), insets_.top());
  }

 private:
  const ImageSkia image_;
  const Insets insets_;
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
  Size size_in_pixel = ScaleToCeiledSize(size_, scale);
  cc::InspectableRecordPaintCanvas record_canvas(size_in_pixel);
  gfx::Canvas canvas(&record_canvas, scale);
#if DCHECK_IS_ON()
  Rect clip_rect;
  DCHECK(canvas.GetClipBounds(&clip_rect));
  DCHECK(clip_rect.Contains(gfx::Rect(size_in_pixel)));
#endif
  canvas.Scale(scale, scale);
  Draw(&canvas);

  return ImageSkiaRep(record_canvas.ReleaseAsRecord(),
                      gfx::ScaleToCeiledSize(size_, scale), scale);
}

}  // namespace gfx
