// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/border.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/paint_flags.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/canvas.h"
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

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const gfx::Insets insets_;
  const SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(SolidSidedBorder);
};

SolidSidedBorder::SolidSidedBorder(const gfx::Insets& insets, SkColor color)
    : insets_(insets),
      color_(color) {
}

void SolidSidedBorder::Paint(const View& view, gfx::Canvas* canvas) {
  // Undo DSF so that we can be sure to draw an integral number of pixels for
  // the border. Integral scale factors should be unaffected by this, but for
  // fractional scale factors this ensures sharp lines.
  gfx::ScopedCanvas scoped(canvas);
  float dsf = canvas->UndoDeviceScaleFactor();

  gfx::RectF scaled_bounds;
  if (view.layer()) {
    scaled_bounds =
        gfx::RectF(ui::ConvertRectToPixel(view.layer(), view.GetLocalBounds()));
  } else {
    scaled_bounds = gfx::RectF(view.GetLocalBounds());
    scaled_bounds.Scale(dsf);
  }

  // This scaling operation floors the inset values.
  scaled_bounds.Inset(insets_.Scale(dsf));
  canvas->sk_canvas()->clipRect(gfx::RectFToSkRect(scaled_bounds),
                                SkClipOp::kDifference, true);
  canvas->DrawColor(color_);
}

gfx::Insets SolidSidedBorder::GetInsets() const {
  return insets_;
}

gfx::Size SolidSidedBorder::GetMinimumSize() const {
  return gfx::Size(insets_.width(), insets_.height());
}

// A border with a rounded rectangle and single color.
class RoundedRectBorder : public Border {
 public:
  RoundedRectBorder(int thickness, int corner_radius, SkColor color);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const int thickness_;
  const int corner_radius_;
  const SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(RoundedRectBorder);
};

RoundedRectBorder::RoundedRectBorder(int thickness,
                                     int corner_radius,
                                     SkColor color)
    : thickness_(thickness), corner_radius_(corner_radius), color_(color) {}

void RoundedRectBorder::Paint(const View& view, gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setStrokeWidth(thickness_);
  flags.setColor(color_);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  float half_thickness = thickness_ / 2.0f;
  gfx::RectF bounds(view.GetLocalBounds());
  bounds.Inset(half_thickness, half_thickness);
  canvas->DrawRoundRect(bounds, corner_radius_, flags);
}

gfx::Insets RoundedRectBorder::GetInsets() const {
  return gfx::Insets(thickness_);
}

gfx::Size RoundedRectBorder::GetMinimumSize() const {
  return gfx::Size(thickness_ * 2, thickness_ * 2);
}

class EmptyBorder : public Border {
 public:
  explicit EmptyBorder(const gfx::Insets& insets);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(EmptyBorder);
};

EmptyBorder::EmptyBorder(const gfx::Insets& insets) : insets_(insets) {
}

void EmptyBorder::Paint(const View& view, gfx::Canvas* canvas) {
}

gfx::Insets EmptyBorder::GetInsets() const {
  return insets_;
}

gfx::Size EmptyBorder::GetMinimumSize() const {
  return gfx::Size();
}

class ExtraInsetsBorder : public Border {
 public:
  ExtraInsetsBorder(std::unique_ptr<Border> border, const gfx::Insets& insets);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  std::unique_ptr<Border> border_;
  const gfx::Insets extra_insets_;

  DISALLOW_COPY_AND_ASSIGN(ExtraInsetsBorder);
};

ExtraInsetsBorder::ExtraInsetsBorder(std::unique_ptr<Border> border,
                                     const gfx::Insets& insets)
    : border_(std::move(border)), extra_insets_(insets) {}

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

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  std::unique_ptr<Painter> painter_;
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(BorderPainter);
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

}  // namespace

Border::Border() = default;

Border::~Border() = default;

std::unique_ptr<Border> NullBorder() {
  return nullptr;
}

std::unique_ptr<Border> CreateSolidBorder(int thickness, SkColor color) {
  return std::make_unique<SolidSidedBorder>(gfx::Insets(thickness), color);
}

std::unique_ptr<Border> CreateEmptyBorder(const gfx::Insets& insets) {
  return std::make_unique<EmptyBorder>(insets);
}

std::unique_ptr<Border> CreateRoundedRectBorder(int thickness,
                                                int corner_radius,
                                                SkColor color) {
  return std::make_unique<RoundedRectBorder>(thickness, corner_radius, color);
}

std::unique_ptr<Border> CreateEmptyBorder(int top,
                                          int left,
                                          int bottom,
                                          int right) {
  return CreateEmptyBorder(gfx::Insets(top, left, bottom, right));
}

std::unique_ptr<Border> CreateSolidSidedBorder(int top,
                                               int left,
                                               int bottom,
                                               int right,
                                               SkColor color) {
  return std::make_unique<SolidSidedBorder>(
      gfx::Insets(top, left, bottom, right), color);
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
