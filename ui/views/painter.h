// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_PAINTER_H_
#define UI_VIEWS_PAINTER_H_

#include <stddef.h>

#include <memory>

#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class ImageSkia;
class InsetsF;
class Rect;
class RoundedCornersF;
class Size;
}  // namespace gfx

namespace ui {
class LayerOwner;
}

namespace views {

class View;

// Painter, as the name implies, is responsible for painting in a particular
// region. Think of Painter as a Border or Background that can be painted
// in any region of a View.
class VIEWS_EXPORT Painter {
 public:
  Painter();

  Painter(const Painter&) = delete;
  Painter& operator=(const Painter&) = delete;

  virtual ~Painter();

  // A convenience method for painting a Painter in a particular region.
  // This translates the canvas to x/y and paints the painter.
  static void PaintPainterAt(gfx::Canvas* canvas,
                             Painter* painter,
                             const gfx::Rect& rect);

  // Convenience that paints |focus_painter| only if |view| HasFocus() and
  // |focus_painter| is non-NULL.
  static void PaintFocusPainter(View* view,
                                gfx::Canvas* canvas,
                                Painter* focus_painter);

  // Creates a painter that draws a RoundRect with a solid color and given
  // corner radius.
  static std::unique_ptr<Painter> CreateSolidRoundRectPainter(
      SkColor color,
      float radius,
      const gfx::Insets& insets = gfx::Insets(),
      SkBlendMode blend_mode = SkBlendMode::kSrcOver,
      bool antialias = true);

  // Creates a painter that draws a RoundRect with a solid color and given
  // corner radii.
  static std::unique_ptr<Painter> CreateSolidRoundRectPainterWithVariableRadius(
      SkColor color,
      gfx::RoundedCornersF radii,
      const gfx::Insets& insets = gfx::Insets(),
      SkBlendMode blend_mode = SkBlendMode::kSrcOver,
      bool antialias = true);

  // Creates a painter that draws a RoundRect with a solid color and a given
  // corner radius, and also adds a 1px border (inset) in the given color.
  // If should_border_scale is true, the 1px border will resize based on the
  // scale factor.
  static std::unique_ptr<Painter> CreateRoundRectWith1PxBorderPainter(
      SkColor bg_color,
      SkColor stroke_color,
      float radius,
      SkBlendMode blend_mode = SkBlendMode::kSrcOver,
      bool antialias = true,
      bool should_border_scale = false);

  // Creates a painter that divides |image| into nine regions. The four corners
  // are rendered at the size specified in insets (eg. the upper-left corner is
  // rendered at 0 x 0 with a size of insets.left() x insets.top()). The center
  // and edge images are stretched to fill the painted area.
  static std::unique_ptr<Painter> CreateImagePainter(
      const gfx::ImageSkia& image,
      const gfx::Insets& insets);

  // Creates a painter that paints images in a scalable grid. The images must
  // share widths by column and heights by row. The corners are painted at full
  // size, while center and edge images are stretched to fill the painted area.
  // The center image may be zero (to be skipped). This ordering must be used:
  // Top-Left/Top/Top-Right/Left/[Center]/Right/Bottom-Left/Bottom/Bottom-Right.
  static std::unique_ptr<Painter> CreateImageGridPainter(const int image_ids[]);

  // Deprecated: used the InsetsF version below.
  static std::unique_ptr<Painter> CreateSolidFocusPainter(
      SkColor color,
      const gfx::Insets& insets);
  // |thickness| is in dip.
  static std::unique_ptr<Painter> CreateSolidFocusPainter(
      SkColor color,
      int thickness,
      const gfx::InsetsF& insets);

  // Creates and returns a texture layer that is painted by |painter|.
  static std::unique_ptr<ui::LayerOwner> CreatePaintedLayer(
      std::unique_ptr<Painter> painter);

  // Returns the minimum size this painter can paint without obvious graphical
  // problems (e.g. overlapping images).
  virtual gfx::Size GetMinimumSize() const = 0;

  // Paints the painter in the specified region.
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_PAINTER_H_
