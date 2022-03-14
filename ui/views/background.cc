// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/background.h"

#include <utility>

#include "base/check.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

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

// RoundedRectBackground is a filled solid colored background that has
// rounded corners.
class RoundedRectBackground : public Background {
 public:
  RoundedRectBackground(SkColor color, float radius, int for_border_thickness)
      : radius_(radius), half_thickness_(for_border_thickness / 2.0f) {
    SetNativeControlColor(color);
  }

  RoundedRectBackground(const RoundedRectBackground&) = delete;
  RoundedRectBackground& operator=(const RoundedRectBackground&) = delete;

  void Paint(gfx::Canvas* canvas, View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    gfx::RectF bounds(view->GetLocalBounds());
    bounds.Inset(half_thickness_);
    canvas->DrawRoundRect(bounds, radius_ - half_thickness_, flags);
  }

 private:
  float radius_;
  float half_thickness_;
};

// ThemedVectorIconBackground is an image drawn on the view's background using
// ThemedVectorIcon to react to theme changes.
class ThemedVectorIconBackground : public Background, public ViewObserver {
 public:
  explicit ThemedVectorIconBackground(View* view,
                                      const ui::ThemedVectorIcon& icon)
      : icon_(icon) {
    DCHECK(!icon_.empty());
    observation_.Observe(view);
    OnViewThemeChanged(view);
  }

  ThemedVectorIconBackground(const ThemedVectorIconBackground&) = delete;
  ThemedVectorIconBackground& operator=(const ThemedVectorIconBackground&) =
      delete;

  // ViewObserver:
  void OnViewThemeChanged(View* view) override { view->SchedulePaint(); }
  void OnViewIsDeleting(View* view) override {
    DCHECK(observation_.IsObservingSource(view));
    observation_.Reset();
  }

  void Paint(gfx::Canvas* canvas, View* view) const override {
    canvas->DrawImageInt(icon_.GetImageSkia(view->GetColorProvider()), 0, 0);
  }

 private:
  const ui::ThemedVectorIcon icon_;
  base::ScopedObservation<View, ViewObserver> observation_{this};
};

// ThemedSolidBackground is a solid background that stays in sync with a view's
// native theme.
class ThemedSolidBackground : public SolidBackground, public ViewObserver {
 public:
  explicit ThemedSolidBackground(View* view, ui::ColorId color_id)
      : SolidBackground(gfx::kPlaceholderColor), color_id_(color_id) {
    observation_.Observe(view);
    if (view->GetWidget())
      OnViewThemeChanged(view);
  }

  ThemedSolidBackground(const ThemedSolidBackground&) = delete;
  ThemedSolidBackground& operator=(const ThemedSolidBackground&) = delete;

  ~ThemedSolidBackground() override = default;

  // ViewObserver:
  void OnViewThemeChanged(View* view) override {
    SetNativeControlColor(view->GetColorProvider()->GetColor(color_id_));
    view->SchedulePaint();
  }
  void OnViewIsDeleting(View* view) override {
    DCHECK(observation_.IsObservingSource(view));
    observation_.Reset();
  }

 private:
  base::ScopedObservation<View, ViewObserver> observation_{this};
  ui::ColorId color_id_;
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

std::unique_ptr<Background> CreateThemedVectorIconBackground(
    View* view,
    const ui::ThemedVectorIcon& icon) {
  return std::make_unique<ThemedVectorIconBackground>(view, icon);
}

std::unique_ptr<Background> CreateThemedSolidBackground(View* view,
                                                        ui::ColorId color_id) {
  return std::make_unique<ThemedSolidBackground>(view, color_id);
}

std::unique_ptr<Background> CreateBackgroundFromPainter(
    std::unique_ptr<Painter> painter) {
  return std::make_unique<BackgroundPainter>(std::move(painter));
}

}  // namespace views
