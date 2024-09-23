// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/painter.h"

#include <utility>

#include "base/check.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
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
                        gfx::RoundedCornersF radii,
                        const gfx::Insets& insets,
                        SkBlendMode blend_mode,
                        bool antialias,
                        bool should_border_scale);

  SolidRoundRectPainter(const SolidRoundRectPainter&) = delete;
  SolidRoundRectPainter& operator=(const SolidRoundRectPainter&) = delete;

  ~SolidRoundRectPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  const SkColor bg_color_;
  const SkColor stroke_color_;
  const gfx::RoundedCornersF radii_;
  const gfx::Insets insets_;
  const SkBlendMode blend_mode_;
  const bool antialias_;
  const bool should_border_scale_;
};

SolidRoundRectPainter::SolidRoundRectPainter(SkColor bg_color,
                                             SkColor stroke_color,
                                             gfx::RoundedCornersF radii,
                                             const gfx::Insets& insets,
                                             SkBlendMode blend_mode,
                                             bool antialias,
                                             bool should_border_scale)
    : bg_color_(bg_color),
      stroke_color_(stroke_color),
      radii_(radii),
      insets_(insets),
      blend_mode_(blend_mode),
      antialias_(antialias),
      should_border_scale_(should_border_scale) {}

SolidRoundRectPainter::~SolidRoundRectPainter() = default;

gfx::Size SolidRoundRectPainter::GetMinimumSize() const {
  return gfx::Size();
}

void SolidRoundRectPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  gfx::Rect inset_rect(size);
  inset_rect.Inset(insets_);
  gfx::RectF fill_rect(gfx::ScaleToEnclosedRect(inset_rect, scale));
  gfx::RectF stroke_rect = fill_rect;

  cc::PaintFlags flags;
  flags.setBlendMode(blend_mode_);
  if (antialias_)
    flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(bg_color_);
  SkPath fill_path;
  const std::array<SkScalar, 8> scaled_radii = {
      {radii_.upper_left() * scale, radii_.upper_left() * scale,
       radii_.upper_right() * scale, radii_.upper_right() * scale,
       radii_.lower_right() * scale, radii_.lower_right() * scale,
       radii_.lower_left() * scale, radii_.lower_left() * scale}};

  UNSAFE_TODO(fill_path.addRoundRect(gfx::RectFToSkRect(fill_rect),
                                     scaled_radii.data()));
  canvas->DrawPath(fill_path, flags);

  if (stroke_color_ != SK_ColorTRANSPARENT) {
    const float stroke_width = should_border_scale_ ? scale : 1.0f;
    const float stroke_inset = stroke_width / 2;
    stroke_rect.Inset(gfx::InsetsF(stroke_inset));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(stroke_width);
    flags.setColor(stroke_color_);

    SkPath stroke_path;
    std::array<SkScalar, 8> stroke_radii;
    for (size_t i = 0; i < 8; i++) {
      stroke_radii[i] = scaled_radii[i] - stroke_width / 2;
    }

    UNSAFE_TODO(stroke_path.addRoundRect(gfx::RectFToSkRect(stroke_rect),
                                         stroke_radii.data()));
    canvas->DrawPath(stroke_path, flags);
  }
}

// SolidFocusPainter -----------------------------------------------------------

class SolidFocusPainter : public Painter {
 public:
  SolidFocusPainter(SkColor color, int thickness, const gfx::InsetsF& insets);

  SolidFocusPainter(const SolidFocusPainter&) = delete;
  SolidFocusPainter& operator=(const SolidFocusPainter&) = delete;

  ~SolidFocusPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  const SkColor color_;
  const int thickness_;
  const gfx::InsetsF insets_;
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

  ImagePainter(const ImagePainter&) = delete;
  ImagePainter& operator=(const ImagePainter&) = delete;

  ~ImagePainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  std::unique_ptr<gfx::NineImagePainter> nine_painter_;
};

ImagePainter::ImagePainter(const int image_ids[])
    : nine_painter_(ui::CreateNineImagePainter(image_ids)) {}

ImagePainter::ImagePainter(const gfx::ImageSkia& image,
                           const gfx::Insets& insets)
    : nine_painter_(new gfx::NineImagePainter(image, insets)) {}

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

  PaintedLayer(const PaintedLayer&) = delete;
  PaintedLayer& operator=(const PaintedLayer&) = delete;

  ~PaintedLayer() override;

  // LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 private:
  std::unique_ptr<Painter> painter_;
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
      color, SK_ColorTRANSPARENT, gfx::RoundedCornersF(radius), insets,
      blend_mode, antialias, false);
}

// static
std::unique_ptr<Painter> Painter::CreateSolidRoundRectPainterWithVariableRadius(
    SkColor color,
    gfx::RoundedCornersF radii,
    const gfx::Insets& insets,
    SkBlendMode blend_mode,
    bool antialias) {
  return std::make_unique<SolidRoundRectPainter>(
      color, SK_ColorTRANSPARENT, radii, insets, blend_mode, antialias, false);
}

// static
std::unique_ptr<Painter> Painter::CreateRoundRectWith1PxBorderPainter(
    SkColor bg_color,
    SkColor stroke_color,
    float radius,
    SkBlendMode blend_mode,
    bool antialias,
    bool should_border_scale) {
  return std::make_unique<SolidRoundRectPainter>(
      bg_color, stroke_color, gfx::RoundedCornersF(radius), gfx::Insets(),
      blend_mode, antialias, should_border_scale);
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
  const gfx::InsetsF corrected_insets(insets - gfx::Insets::TLBR(0, 0, 1, 1));
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
