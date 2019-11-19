// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/nine_image_painter.h"

#include <stddef.h>

#include <limits>

#include "base/stl_util.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"

namespace gfx {

namespace {

int ImageRepWidthInPixels(const ImageSkiaRep& rep) {
  if (rep.is_null())
    return 0;
  return rep.pixel_width();
}

int ImageRepHeightInPixels(const ImageSkiaRep& rep) {
  if (rep.is_null())
    return 0;
  return rep.pixel_height();
}

void Fill(Canvas* c,
          const ImageSkiaRep& rep,
          int x,
          int y,
          int w,
          int h,
          const cc::PaintFlags& flags) {
  if (rep.is_null())
    return;
  c->DrawImageIntInPixel(rep, x, y, w, h, false, flags);
}

}  // namespace

NineImagePainter::NineImagePainter(const std::vector<ImageSkia>& images) {
  DCHECK_EQ(base::size(images_), images.size());
  for (size_t i = 0; i < base::size(images_); ++i)
    images_[i] = images[i];
}

NineImagePainter::NineImagePainter(const ImageSkia& image,
                                   const Insets& insets) {
  std::vector<gfx::Rect> regions;
  GetSubsetRegions(image, insets, &regions);
  DCHECK_EQ(9u, regions.size());

  for (size_t i = 0; i < 9; ++i)
    images_[i] = ImageSkiaOperations::ExtractSubset(image, regions[i]);
}

NineImagePainter::~NineImagePainter() {
}

bool NineImagePainter::IsEmpty() const {
  return images_[0].isNull();
}

Size NineImagePainter::GetMinimumSize() const {
  return IsEmpty() ? Size() : Size(
      images_[0].width() + images_[1].width() + images_[2].width(),
      images_[0].height() + images_[3].height() + images_[6].height());
}

void NineImagePainter::Paint(Canvas* canvas, const Rect& bounds) {
  // When no alpha value is specified, use default value of 100% opacity.
  Paint(canvas, bounds, std::numeric_limits<uint8_t>::max());
}

void NineImagePainter::Paint(Canvas* canvas,
                             const Rect& bounds,
                             const uint8_t alpha) {
  if (IsEmpty())
    return;

  ScopedCanvas scoped_canvas(canvas);

  // Painting and doing layout at physical device pixels to avoid cracks or
  // overlap.
  const float scale = canvas->UndoDeviceScaleFactor();

  // Since the drawing from the following Fill() calls assumes the mapped origin
  // is at (0,0), we need to translate the canvas to the mapped origin.
  const int left_in_pixels = ToRoundedInt(bounds.x() * scale);
  const int top_in_pixels = ToRoundedInt(bounds.y() * scale);
  const int right_in_pixels = ToRoundedInt(bounds.right() * scale);
  const int bottom_in_pixels = ToRoundedInt(bounds.bottom() * scale);

  const int width_in_pixels = right_in_pixels - left_in_pixels;
  const int height_in_pixels = bottom_in_pixels - top_in_pixels;

  // Since the drawing from the following Fill() calls assumes the mapped origin
  // is at (0,0), we need to translate the canvas to the mapped origin.
  canvas->Translate(gfx::Vector2d(left_in_pixels, top_in_pixels));

  ImageSkiaRep image_reps[9];
  static_assert(base::size(image_reps) == std::extent<decltype(images_)>(), "");
  for (size_t i = 0; i < base::size(image_reps); ++i) {
    image_reps[i] = images_[i].GetRepresentation(scale);
    DCHECK(image_reps[i].is_null() || image_reps[i].scale() == scale);
  }

  // In case the corners and edges don't all have the same width/height, we draw
  // the center first, and extend it out in all directions to the edges of the
  // images with the smallest widths/heights.  This way there will be no
  // unpainted areas, though some corners or edges might overlap the center.
  int i0w = ImageRepWidthInPixels(image_reps[0]);
  int i2w = ImageRepWidthInPixels(image_reps[2]);
  int i3w = ImageRepWidthInPixels(image_reps[3]);
  int i5w = ImageRepWidthInPixels(image_reps[5]);
  int i6w = ImageRepWidthInPixels(image_reps[6]);
  int i8w = ImageRepWidthInPixels(image_reps[8]);

  int i0h = ImageRepHeightInPixels(image_reps[0]);
  int i1h = ImageRepHeightInPixels(image_reps[1]);
  int i2h = ImageRepHeightInPixels(image_reps[2]);
  int i6h = ImageRepHeightInPixels(image_reps[6]);
  int i7h = ImageRepHeightInPixels(image_reps[7]);
  int i8h = ImageRepHeightInPixels(image_reps[8]);

  i0w = std::min(i0w, width_in_pixels);
  i2w = std::min(i2w, width_in_pixels - i0w);
  i3w = std::min(i3w, width_in_pixels);
  i5w = std::min(i5w, width_in_pixels - i3w);
  i6w = std::min(i6w, width_in_pixels);
  i8w = std::min(i8w, width_in_pixels - i6w);

  i0h = std::min(i0h, height_in_pixels);
  i1h = std::min(i1h, height_in_pixels);
  i2h = std::min(i2h, height_in_pixels);
  i6h = std::min(i6h, height_in_pixels - i0h);
  i7h = std::min(i7h, height_in_pixels - i1h);
  i8h = std::min(i8h, height_in_pixels - i2h);

  int i4x = std::min({i0w, i3w, i6w});
  int i4y = std::min({i0h, i1h, i2h});
  int i4w = std::max(width_in_pixels - i4x - std::min({i2w, i5w, i8w}), 0);
  int i4h = std::max(height_in_pixels - i4y - std::min({i6h, i7h, i8h}), 0);

  cc::PaintFlags flags;
  flags.setAlpha(alpha);

  Fill(canvas, image_reps[4], i4x, i4y, i4w, i4h, flags);
  Fill(canvas, image_reps[0], 0, 0, i0w, i0h, flags);
  Fill(canvas, image_reps[1], i0w, 0, width_in_pixels - i0w - i2w, i1h, flags);
  Fill(canvas, image_reps[2], width_in_pixels - i2w, 0, i2w, i2h, flags);
  Fill(canvas, image_reps[3], 0, i0h, i3w, height_in_pixels - i0h - i6h, flags);
  Fill(canvas, image_reps[5], width_in_pixels - i5w, i2h, i5w,
       height_in_pixels - i2h - i8h, flags);
  Fill(canvas, image_reps[6], 0, height_in_pixels - i6h, i6w, i6h, flags);
  Fill(canvas, image_reps[7], i6w, height_in_pixels - i7h,
       width_in_pixels - i6w - i8w, i7h, flags);
  Fill(canvas, image_reps[8], width_in_pixels - i8w, height_in_pixels - i8h,
       i8w, i8h, flags);
}

// static
void NineImagePainter::GetSubsetRegions(const ImageSkia& image,
                                        const Insets& insets,
                                        std::vector<Rect>* regions) {
  DCHECK_GE(image.width(), insets.width());
  DCHECK_GE(image.height(), insets.height());

  std::vector<Rect> result(9);

  const int x[] = {
      0, insets.left(), image.width() - insets.right(), image.width()};
  const int y[] = {
      0, insets.top(), image.height() - insets.bottom(), image.height()};

  for (size_t j = 0; j < 3; ++j) {
    for (size_t i = 0; i < 3; ++i) {
      result[i + j * 3] = Rect(x[i], y[j], x[i + 1] - x[i], y[j + 1] - y[j]);
    }
  }
  result.swap(*regions);
}

}  // namespace gfx
