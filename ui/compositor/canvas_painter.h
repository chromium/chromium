// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_CANVAS_PAINTER_H_
#define UI_COMPOSITOR_CANVAS_PAINTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class DisplayItemList;
}

namespace ui {

// This class provides a simple helper for rasterizing from a PaintContext
// interface directly into a bitmap.  After constructing an instance of a
// CanvasPainter, the context() can be used to do painting using the normal
// composited paint paths.  When the painter is destroyed, any painting done
// with the context() will be rastered into the provided output bitmap.
//
// TODO(enne): rename this class to be PaintContextRasterizer or some such.
class COMPOSITOR_EXPORT CanvasPainter {
 public:
  CanvasPainter(SkBitmap* output,
                const gfx::Size& output_size,
                float device_scale_factor,
                SkColor clear_color,
                bool is_pixel_canvas);

  CanvasPainter(const CanvasPainter&) = delete;
  CanvasPainter& operator=(const CanvasPainter&) = delete;

  ~CanvasPainter();

  const PaintContext& context() const { return context_; }

 private:
  friend class CanvasPainterTest;

  const raw_ptr<SkBitmap> output_;
  const gfx::Size pixel_output_size_;
  const float raster_scale_;
  const SkColor clear_color_;
  scoped_refptr<cc::DisplayItemList> list_;
  PaintContext context_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_CANVAS_PAINTER_H_
