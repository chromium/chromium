// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/background.h"

#include <utility>

#include "base/check.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_WIN)
#include "skia/ext/skia_utils_win.h"
#endif

namespace views {

// SolidBackground is a trivial Background implementation that fills the
// background in a solid color.
class SolidBackground : public Background {
 public:
  explicit SolidBackground(SkColor color) { SetNativeControlColor(color); }

  SolidBackground(const SolidBackground&) = delete;
  SolidBackground& operator=(const SolidBackground&) = delete;

  void Paint(gfx::Canvas* canvas, View* view) const override {
    // Fill the background. Note that we don't constrain to the bounds as
    // canvas is already clipped for us.
    canvas->DrawColor(get_color());
  }
};

// Shared class for RoundedRectBackground and ThemedRoundedRectBackground.
class BaseRoundedRectBackground : public Background {
 public:
  BaseRoundedRectBackground(float top_radius,
                            float bottom_radius,
                            int for_border_thickness)
      : top_radius_(top_radius),
        bottom_radius_(bottom_radius),
        half_thickness_(for_border_thickness / 2.0f) {}

  BaseRoundedRectBackground(const BaseRoundedRectBackground&) = delete;
  BaseRoundedRectBackground& operator=(const BaseRoundedRectBackground&) =
      delete;

  void Paint(gfx::Canvas* canvas, View* view) const override {
    gfx::Rect rect(view->GetLocalBounds());
    rect.Inset(half_thickness_);
    SkPath path;
    SkScalar radii[8] = {top_radius_,    top_radius_,    top_radius_,
                         top_radius_,    bottom_radius_, bottom_radius_,
                         bottom_radius_, bottom_radius_};
    path.addRoundRect(gfx::RectToSkRect(rect), radii);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    canvas->DrawPath(path, flags);
  }

 private:
  const float top_radius_;
  const float bottom_radius_;
  const float half_thickness_;
};

// RoundedRectBackground is a filled solid colored background that has
// rounded corners.
class RoundedRectBackground : public BaseRoundedRectBackground {
 public:
  RoundedRectBackground(SkColor color, float radius, int for_border_thickness)
      : BaseRoundedRectBackground(radius, radius, for_border_thickness) {
    SetNativeControlColor(color);
  }

  RoundedRectBackground(const RoundedRectBackground&) = delete;
  RoundedRectBackground& operator=(const RoundedRectBackground&) = delete;
};

// ThemedVectorIconBackground is an image drawn on the view's background using
// ThemedVectorIcon to react to theme changes.
class ThemedVectorIconBackground : public Background {
 public:
  explicit ThemedVectorIconBackground(const ui::ThemedVectorIcon& icon)
      : icon_(icon) {
    DCHECK(!icon_.empty());
  }

  ThemedVectorIconBackground(const ThemedVectorIconBackground&) = delete;
  ThemedVectorIconBackground& operator=(const ThemedVectorIconBackground&) =
      delete;

  void OnViewThemeChanged(View* view) override { view->SchedulePaint(); }

  void Paint(gfx::Canvas* canvas, View* view) const override {
    canvas->DrawImageInt(icon_.GetImageSkia(view->GetColorProvider()), 0, 0);
  }

 private:
  const ui::ThemedVectorIcon icon_;
};

// ThemedSolidBackground is a solid background that stays in sync with a view's
// ColorProvider.
class ThemedSolidBackground : public SolidBackground {
 public:
  explicit ThemedSolidBackground(ui::ColorId color_id)
      : SolidBackground(gfx::kPlaceholderColor), color_id_(color_id) {}

  ThemedSolidBackground(const ThemedSolidBackground&) = delete;
  ThemedSolidBackground& operator=(const ThemedSolidBackground&) = delete;

  ~ThemedSolidBackground() override = default;

  void OnViewThemeChanged(View* view) override {
    SetNativeControlColor(view->GetColorProvider()->GetColor(color_id_));
    view->SchedulePaint();
  }

 private:
  const ui::ColorId color_id_;
};

// ThemedRoundedRectBackground is a solid rounded rect background that stays in
// sync with a view's ColorProvider.
class ThemedRoundedRectBackground : public BaseRoundedRectBackground {
 public:
  ThemedRoundedRectBackground(ui::ColorId color_id,
                              float radius,
                              int for_border_thickness)
      : ThemedRoundedRectBackground(color_id,
                                    radius,
                                    radius,
                                    for_border_thickness) {}

  ThemedRoundedRectBackground(ui::ColorId color_id,
                              float top_radius,
                              float bottom_radius,
                              int for_border_thickness)
      : BaseRoundedRectBackground(top_radius,
                                  bottom_radius,
                                  for_border_thickness),
        color_id_(color_id) {}

  ThemedRoundedRectBackground(const ThemedRoundedRectBackground&) = delete;
  ThemedRoundedRectBackground& operator=(const ThemedRoundedRectBackground&) =
      delete;

  ~ThemedRoundedRectBackground() override = default;

  void OnViewThemeChanged(View* view) override {
    SetNativeControlColor(view->GetColorProvider()->GetColor(color_id_));
    view->SchedulePaint();
  }

 private:
  const ui::ColorId color_id_;
};

class BackgroundPainter : public Background {
 public:
  explicit BackgroundPainter(std::unique_ptr<Painter> painter)
      : painter_(std::move(painter)) {
    DCHECK(painter_);
  }

  BackgroundPainter(const BackgroundPainter&) = delete;
  BackgroundPainter& operator=(const BackgroundPainter&) = delete;

  ~BackgroundPainter() override = default;

  void Paint(gfx::Canvas* canvas, View* view) const override {
    Painter::PaintPainterAt(canvas, painter_.get(), view->GetLocalBounds());
  }

 private:
  std::unique_ptr<Painter> painter_;
};

Background::Background() = default;

Background::~Background() = default;

void Background::SetNativeControlColor(SkColor color) {
  color_ = color;
}

void Background::OnViewThemeChanged(View* view) {}

std::unique_ptr<Background> CreateSolidBackground(SkColor color) {
  return std::make_unique<SolidBackground>(color);
}

std::unique_ptr<Background> CreateRoundedRectBackground(
    SkColor color,
    float radius,
    int for_border_thickness) {
  return std::make_unique<RoundedRectBackground>(color, radius,
                                                 for_border_thickness);
}

std::unique_ptr<Background> CreateThemedRoundedRectBackground(
    ui::ColorId color_id,
    float radius,
    int for_border_thickness) {
  return std::make_unique<ThemedRoundedRectBackground>(color_id, radius,
                                                       for_border_thickness);
}

std::unique_ptr<Background> CreateThemedRoundedRectBackground(
    ui::ColorId color_id,
    float top_radius,
    float bottom_radius,
    int for_border_thickness) {
  return std::make_unique<ThemedRoundedRectBackground>(
      color_id, top_radius, bottom_radius, for_border_thickness);
}

std::unique_ptr<Background> CreateThemedVectorIconBackground(
    const ui::ThemedVectorIcon& icon) {
  return std::make_unique<ThemedVectorIconBackground>(icon);
}

std::unique_ptr<Background> CreateThemedSolidBackground(ui::ColorId color_id) {
  return std::make_unique<ThemedSolidBackground>(color_id);
}

std::unique_ptr<Background> CreateBackgroundFromPainter(
    std::unique_ptr<Painter> painter) {
  return std::make_unique<BackgroundPainter>(std::move(painter));
}

}  // namespace views
