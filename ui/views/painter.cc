// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/painter.h"

#include "base/logging.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/nine_image_painter.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"

namespace views {

namespace {

// SolidRoundRectPainter -------------------------------------------------------

// Creates a round rect painter with a 1 pixel border. The border paints on top
// of the background.
class SolidRoundRectPainter : public Painter {
 public:
  SolidRoundRectPainter(SkColor bg_color,
                        SkColor stroke_color,
                        float radius,
                        const gfx::Insets& insets,
                        SkBlendMode blend_mode,
                        bool antialias);
  ~SolidRoundRectPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  const SkColor bg_color_;
  const SkColor stroke_color_;
  const float radius_;
  const gfx::Insets insets_;
  const SkBlendMode blend_mode_;
  const bool antialias_;

  DISALLOW_COPY_AND_ASSIGN(SolidRoundRectPainter);
};

SolidRoundRectPainter::SolidRoundRectPainter(SkColor bg_color,
                                             SkColor stroke_color,
                                             float radius,
                                             const gfx::Insets& insets,
                                             SkBlendMode blend_mode,
                                             bool antialias)
    : bg_color_(bg_color),
      stroke_color_(stroke_color),
      radius_(radius),
      insets_(insets),
      blend_mode_(blend_mode),
      antialias_(antialias) {}

SolidRoundRectPainter::~SolidRoundRectPainter() = default;

gfx::Size SolidRoundRectPainter::GetMinimumSize() const {
  return gfx::Size();
}

void SolidRoundRectPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  gfx::Rect inset_rect(size);
  inset_rect.Inset(insets_);
  gfx::RectF fill_rect(gfx::ScaleToEnclosingRect(inset_rect, scale));
  gfx::RectF stroke_rect = fill_rect;
  float scaled_radius = radius_ * scale;

  cc::PaintFlags flags;
  flags.setBlendMode(blend_mode_);
  if (antialias_)
    flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(bg_color_);
  canvas->DrawRoundRect(fill_rect, scaled_radius, flags);

  if (stroke_color_ != SK_ColorTRANSPARENT) {
    constexpr float kStrokeWidth = 1.0f;
    stroke_rect.Inset(gfx::InsetsF(kStrokeWidth / 2));
    scaled_radius -= kStrokeWidth / 2;
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kStrokeWidth);
    flags.setColor(stroke_color_);
    canvas->DrawRoundRect(stroke_rect, scaled_radius, flags);
  }
}

// SolidFocusPainter -----------------------------------------------------------

class SolidFocusPainter : public Painter {
 public:
  SolidFocusPainter(SkColor color, int thickness, const gfx::InsetsF& insets);
  ~SolidFocusPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  const SkColor color_;
  const int thickness_;
  const gfx::InsetsF insets_;

  DISALLOW_COPY_AND_ASSIGN(SolidFocusPainter);
};

SolidFocusPainter::SolidFocusPainter(SkColor color,
                                     int thickness,
                                     const gfx::InsetsF& insets)
    : color_(color), thickness_(thickness), insets_(insets) {}

SolidFocusPainter::~SolidFocusPainter() = default;

gfx::Size SolidFocusPainter::GetMinimumSize() const {
  return gfx::Size();
}

void SolidFocusPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  gfx::RectF rect((gfx::Rect(size)));
  rect.Inset(insets_);
  canvas->DrawSolidFocusRect(rect, color_, thickness_);
}

// ImagePainter ---------------------------------------------------------------

// ImagePainter stores and paints nine images as a scalable grid.
class ImagePainter : public Painter {
 public:
  // Constructs an ImagePainter with the specified image resource ids.
  // See CreateImageGridPainter()'s comment regarding image ID count and order.
  explicit ImagePainter(const int image_ids[]);

  // Constructs an ImagePainter with the specified image and insets.
  ImagePainter(const gfx::ImageSkia& image, const gfx::Insets& insets);

  ~ImagePainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  std::unique_ptr<gfx::NineImagePainter> nine_painter_;

  DISALLOW_COPY_AND_ASSIGN(ImagePainter);
};

ImagePainter::ImagePainter(const int image_ids[])
    : nine_painter_(ui::CreateNineImagePainter(image_ids)) {
}

ImagePainter::ImagePainter(const gfx::ImageSkia& image,
                           const gfx::Insets& insets)
    : nine_painter_(new gfx::NineImagePainter(image, insets)) {
}

ImagePainter::~ImagePainter() = default;

gfx::Size ImagePainter::GetMinimumSize() const {
  return nine_painter_->GetMinimumSize();
}

void ImagePainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  nine_painter_->Paint(canvas, gfx::Rect(size));
}

class PaintedLayer : public ui::LayerOwner, public ui::LayerDelegate {
 public:
  explicit PaintedLayer(std::unique_ptr<Painter> painter);
  ~PaintedLayer() override;

  // LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 private:
  std::unique_ptr<Painter> painter_;

  DISALLOW_COPY_AND_ASSIGN(PaintedLayer);
};

PaintedLayer::PaintedLayer(std::unique_ptr<Painter> painter)
    : painter_(std::move(painter)) {
  SetLayer(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED));
  layer()->set_delegate(this);
}

PaintedLayer::~PaintedLayer() = default;

void PaintedLayer::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  painter_->Paint(recorder.canvas(), layer()->size());
}

void PaintedLayer::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                              float new_device_scale_factor) {}

}  // namespace


// Painter --------------------------------------------------------------------

Painter::Painter() = default;

Painter::~Painter() = default;

// static
void Painter::PaintPainterAt(gfx::Canvas* canvas,
                             Painter* painter,
                             const gfx::Rect& rect) {
  DCHECK(canvas);
  DCHECK(painter);
  canvas->Save();
  canvas->Translate(rect.OffsetFromOrigin());
  painter->Paint(canvas, rect.size());
  canvas->Restore();
}

// static
void Painter::PaintFocusPainter(View* view,
                                gfx::Canvas* canvas,
                                Painter* focus_painter) {
  if (focus_painter && view->HasFocus())
    PaintPainterAt(canvas, focus_painter, view->GetLocalBounds());
}

// static
std::unique_ptr<Painter> Painter::CreateSolidRoundRectPainter(
    SkColor color,
    float radius,
    const gfx::Insets& insets,
    SkBlendMode blend_mode,
    bool antialias) {
  return std::make_unique<SolidRoundRectPainter>(
      color, SK_ColorTRANSPARENT, radius, insets, blend_mode, antialias);
}

// static
std::unique_ptr<Painter> Painter::CreateRoundRectWith1PxBorderPainter(
    SkColor bg_color,
    SkColor stroke_color,
    float radius,
    SkBlendMode blend_mode,
    bool antialias) {
  return std::make_unique<SolidRoundRectPainter>(
      bg_color, stroke_color, radius, gfx::Insets(), blend_mode, antialias);
}

// static
std::unique_ptr<Painter> Painter::CreateImagePainter(
    const gfx::ImageSkia& image,
    const gfx::Insets& insets) {
  return std::make_unique<ImagePainter>(image, insets);
}

// static
std::unique_ptr<Painter> Painter::CreateImageGridPainter(
    const int image_ids[]) {
  return std::make_unique<ImagePainter>(image_ids);
}

// static
std::unique_ptr<Painter> Painter::CreateSolidFocusPainter(
    SkColor color,
    const gfx::Insets& insets) {
  // Before Canvas::DrawSolidFocusRect correctly inset the rect's bounds based
  // on the thickness, callers had to add 1 to the bottom and right insets.
  // Subtract that here so it works the same way with the new
  // Canvas::DrawSolidFocusRect.
  const gfx::Insets corrected_insets = insets - gfx::Insets(0, 0, 1, 1);
  return std::make_unique<SolidFocusPainter>(color, 1, corrected_insets);
}

// static
std::unique_ptr<Painter> Painter::CreateSolidFocusPainter(
    SkColor color,
    int thickness,
    const gfx::InsetsF& insets) {
  return std::make_unique<SolidFocusPainter>(color, thickness, insets);
}

// static
std::unique_ptr<ui::LayerOwner> Painter::CreatePaintedLayer(
    std::unique_ptr<Painter> painter) {
  return std::make_unique<PaintedLayer>(std::move(painter));
}

}  // namespace views
