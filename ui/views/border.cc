// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/border.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/paint_flags.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

namespace views {

namespace {

// A simple border with different thicknesses on each side and single color.
class SolidSidedBorder : public Border {
 public:
  SolidSidedBorder(const gfx::Insets& insets, SkColor color);

  SolidSidedBorder(const SolidSidedBorder&) = delete;
  SolidSidedBorder& operator=(const SolidSidedBorder&) = delete;

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const gfx::Insets insets_;
};

SolidSidedBorder::SolidSidedBorder(const gfx::Insets& insets, SkColor color)
    : Border(color), insets_(insets) {}

void SolidSidedBorder::Paint(const View& view, gfx::Canvas* canvas) {
  // Undo DSF so that we can be sure to draw an integral number of pixels for
  // the border. Integral scale factors should be unaffected by this, but for
  // fractional scale factors this ensures sharp lines.
  gfx::ScopedCanvas scoped(canvas);
  float dsf = canvas->UndoDeviceScaleFactor();

  gfx::RectF scaled_bounds;
  if (view.layer()) {
    scaled_bounds = gfx::ConvertRectToPixels(
        view.GetLocalBounds(), view.layer()->device_scale_factor());
  } else {
    scaled_bounds = gfx::RectF(view.GetLocalBounds());
    scaled_bounds.Scale(dsf);
  }

  gfx::InsetsF insets_in_pixels(
      gfx::ToFlooredInsets(gfx::ConvertInsetsToPixels(insets_, dsf)));
  scaled_bounds.Inset(insets_in_pixels);
  canvas->sk_canvas()->clipRect(gfx::RectFToSkRect(scaled_bounds),
                                SkClipOp::kDifference, true);
  canvas->DrawColor(color());
}

gfx::Insets SolidSidedBorder::GetInsets() const {
  return insets_;
}

gfx::Size SolidSidedBorder::GetMinimumSize() const {
  return gfx::Size(insets_.width(), insets_.height());
}

class ThemedSolidSidedBorder : public SolidSidedBorder {
 public:
  ThemedSolidSidedBorder(const gfx::Insets& insets, ui::ColorId color_id);

  // SolidSidedBorder:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  void OnViewThemeChanged(View* view) override;

 private:
  ui::ColorId color_id_;
};

ThemedSolidSidedBorder::ThemedSolidSidedBorder(const gfx::Insets& insets,
                                               ui::ColorId color_id)
    : SolidSidedBorder(insets, gfx::kPlaceholderColor), color_id_(color_id) {}

void ThemedSolidSidedBorder::Paint(const View& view, gfx::Canvas* canvas) {
  set_color(view.GetColorProvider()->GetColor(color_id_));
  SolidSidedBorder::Paint(view, canvas);
}

void ThemedSolidSidedBorder::OnViewThemeChanged(View* view) {
  view->SchedulePaint();
}

// A border with a rounded rectangle and single color.
class RoundedRectBorder : public Border {
 public:
  RoundedRectBorder(int thickness,
                    float corner_radius,
                    const gfx::Insets& paint_insets,
                    SkColor color);

  RoundedRectBorder(const RoundedRectBorder&) = delete;
  RoundedRectBorder& operator=(const RoundedRectBorder&) = delete;

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const int thickness_;
  const float corner_radius_;
  const gfx::Insets paint_insets_;
};

RoundedRectBorder::RoundedRectBorder(int thickness,
                                     float corner_radius,
                                     const gfx::Insets& paint_insets,
                                     SkColor color)
    : Border(color),
      thickness_(thickness),
      corner_radius_(corner_radius),
      paint_insets_(paint_insets) {}

void RoundedRectBorder::Paint(const View& view, gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setStrokeWidth(thickness_);
  flags.setColor(color());
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  const float half_thickness = thickness_ / 2.0f;
  gfx::RectF bounds(view.GetLocalBounds());
  bounds.Inset(gfx::InsetsF(paint_insets_));
  bounds.Inset(half_thickness);
  canvas->DrawRoundRect(bounds, corner_radius_ - half_thickness, flags);
}

gfx::Insets RoundedRectBorder::GetInsets() const {
  return gfx::Insets(thickness_) + paint_insets_;
}

gfx::Size RoundedRectBorder::GetMinimumSize() const {
  return gfx::Size(thickness_ * 2, thickness_ * 2);
}

class EmptyBorder : public Border {
 public:
  explicit EmptyBorder(const gfx::Insets& insets);

  EmptyBorder(const EmptyBorder&) = delete;
  EmptyBorder& operator=(const EmptyBorder&) = delete;

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const gfx::Insets insets_;
};

EmptyBorder::EmptyBorder(const gfx::Insets& insets) : insets_(insets) {}

void EmptyBorder::Paint(const View& view, gfx::Canvas* canvas) {}

gfx::Insets EmptyBorder::GetInsets() const {
  return insets_;
}

gfx::Size EmptyBorder::GetMinimumSize() const {
  return gfx::Size();
}

class ExtraInsetsBorder : public Border {
 public:
  ExtraInsetsBorder(std::unique_ptr<Border> border, const gfx::Insets& insets);

  ExtraInsetsBorder(const ExtraInsetsBorder&) = delete;
  ExtraInsetsBorder& operator=(const ExtraInsetsBorder&) = delete;

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  std::unique_ptr<Border> border_;
  const gfx::Insets extra_insets_;
};

ExtraInsetsBorder::ExtraInsetsBorder(std::unique_ptr<Border> border,
                                     const gfx::Insets& insets)
    : Border(border->color()),
      border_(std::move(border)),
      extra_insets_(insets) {}

void ExtraInsetsBorder::Paint(const View& view, gfx::Canvas* canvas) {
  border_->Paint(view, canvas);
}

gfx::Insets ExtraInsetsBorder::GetInsets() const {
  return border_->GetInsets() + extra_insets_;
}

gfx::Size ExtraInsetsBorder::GetMinimumSize() const {
  gfx::Size size = border_->GetMinimumSize();
  size.Enlarge(extra_insets_.width(), extra_insets_.height());
  return size;
}

class BorderPainter : public Border {
 public:
  BorderPainter(std::unique_ptr<Painter> painter, const gfx::Insets& insets);

  BorderPainter(const BorderPainter&) = delete;
  BorderPainter& operator=(const BorderPainter&) = delete;

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  std::unique_ptr<Painter> painter_;
  const gfx::Insets insets_;
};

BorderPainter::BorderPainter(std::unique_ptr<Painter> painter,
                             const gfx::Insets& insets)
    : painter_(std::move(painter)), insets_(insets) {
  DCHECK(painter_);
}

void BorderPainter::Paint(const View& view, gfx::Canvas* canvas) {
  Painter::PaintPainterAt(canvas, painter_.get(), view.GetLocalBounds());
}

gfx::Insets BorderPainter::GetInsets() const {
  return insets_;
}

gfx::Size BorderPainter::GetMinimumSize() const {
  return painter_->GetMinimumSize();
}

class ThemedRoundedRectBorder : public RoundedRectBorder {
 public:
  ThemedRoundedRectBorder(int thickness,
                          float corner_radius,
                          const gfx::Insets& paint_insets,
                          ui::ColorId color_id)
      : RoundedRectBorder(thickness,
                          corner_radius,
                          paint_insets,
                          gfx::kPlaceholderColor),
        color_id_(color_id) {}

  ThemedRoundedRectBorder(const ThemedRoundedRectBorder&) = delete;
  ThemedRoundedRectBorder& operator=(const ThemedRoundedRectBorder&) = delete;

  void Paint(const View& view, gfx::Canvas* canvas) override {
    set_color(view.GetColorProvider()->GetColor(color_id_));
    RoundedRectBorder::Paint(view, canvas);
  }

  void OnViewThemeChanged(View* view) override { view->SchedulePaint(); }

 private:
  const ui::ColorId color_id_;
};

}  // namespace

Border::Border() = default;

Border::Border(SkColor color) : color_(color) {}

Border::~Border() = default;

void Border::OnViewThemeChanged(View* view) {}

std::unique_ptr<Border> NullBorder() {
  return nullptr;
}

std::unique_ptr<Border> CreateSolidBorder(int thickness, SkColor color) {
  return std::make_unique<SolidSidedBorder>(gfx::Insets(thickness), color);
}

std::unique_ptr<Border> CreateThemedSolidBorder(int thickness,
                                                ui::ColorId color) {
  return std::make_unique<ThemedSolidSidedBorder>(gfx::Insets(thickness),
                                                  color);
}

std::unique_ptr<Border> CreateEmptyBorder(const gfx::Insets& insets) {
  return std::make_unique<EmptyBorder>(insets);
}

std::unique_ptr<Border> CreateEmptyBorder(int thickness) {
  return CreateEmptyBorder(gfx::Insets(thickness));
}

std::unique_ptr<Border> CreateRoundedRectBorder(int thickness,
                                                float corner_radius,
                                                SkColor color) {
  return CreateRoundedRectBorder(thickness, corner_radius, gfx::Insets(),
                                 color);
}

std::unique_ptr<Border> CreateRoundedRectBorder(int thickness,
                                                float corner_radius,
                                                const gfx::Insets& paint_insets,
                                                SkColor color) {
  return std::make_unique<RoundedRectBorder>(thickness, corner_radius,
                                             paint_insets, color);
}

std::unique_ptr<Border> CreateThemedRoundedRectBorder(int thickness,
                                                      float corner_radius,
                                                      ui::ColorId color_id) {
  return CreateThemedRoundedRectBorder(thickness, corner_radius, gfx::Insets(),
                                       color_id);
}

std::unique_ptr<Border> CreateThemedRoundedRectBorder(
    int thickness,
    float corner_radius,
    const gfx::Insets& paint_insets,
    ui::ColorId color_id) {
  return std::make_unique<ThemedRoundedRectBorder>(thickness, corner_radius,
                                                   paint_insets, color_id);
}

std::unique_ptr<Border> CreateSolidSidedBorder(const gfx::Insets& insets,
                                               SkColor color) {
  return std::make_unique<SolidSidedBorder>(insets, color);
}

std::unique_ptr<Border> CreateThemedSolidSidedBorder(const gfx::Insets& insets,
                                                     ui::ColorId color_id) {
  return std::make_unique<ThemedSolidSidedBorder>(insets, color_id);
}

std::unique_ptr<Border> CreatePaddedBorder(std::unique_ptr<Border> border,
                                           const gfx::Insets& insets) {
  return std::make_unique<ExtraInsetsBorder>(std::move(border), insets);
}

std::unique_ptr<Border> CreateBorderPainter(std::unique_ptr<Painter> painter,
                                            const gfx::Insets& insets) {
  return base::WrapUnique(new BorderPainter(std::move(painter), insets));
}

}  // namespace views
