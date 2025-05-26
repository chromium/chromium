// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/background.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_variant.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
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
  explicit SolidBackground(ui::ColorVariant color) { SetColor(color); }

  SolidBackground(const SolidBackground&) = delete;
  SolidBackground& operator=(const SolidBackground&) = delete;

  void Paint(gfx::Canvas* canvas, View* view) const override {
    // Fill the background. Note that we don't constrain to the bounds as
    // canvas is already clipped for us.
    canvas->DrawColor(color().ResolveToSkColor(view->GetColorProvider()));
  }

  void OnViewThemeChanged(View* view) override {
    if (color().IsSemantic()) {
      view->SchedulePaint();
    }
  }
};

// Shared class for RoundedRectBackground and ThemedRoundedRectBackground.
class RoundedRectBackground : public Background {
 public:
  RoundedRectBackground(ui::ColorVariant color,
                        const gfx::RoundedCornersF& radii,
                        const gfx::Insets& insets)
      : radii_(radii), insets_(insets) {
    SetColor(color);
  }

  RoundedRectBackground(const RoundedRectBackground&) = delete;
  RoundedRectBackground& operator=(const RoundedRectBackground&) = delete;

  void Paint(gfx::Canvas* canvas, View* view) const override {
    gfx::Rect rect(view->GetLocalBounds());
    rect.Inset(insets_);
    SkPath path;
    SkScalar radii[8] = {radii_.upper_left(),  radii_.upper_left(),
                         radii_.upper_right(), radii_.upper_right(),
                         radii_.lower_right(), radii_.lower_right(),
                         radii_.lower_left(),  radii_.lower_left()};
    path.addRoundRect(gfx::RectToSkRect(rect), radii);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color().ResolveToSkColor(view->GetColorProvider()));
    canvas->DrawPath(path, flags);
  }

  std::optional<gfx::RoundedCornersF> GetRoundedCornerRadii() const override {
    return radii_;
  }

  void OnViewThemeChanged(View* view) override {
    if (color().IsSemantic()) {
      view->SchedulePaint();
    }
  }

 private:
  const gfx::RoundedCornersF radii_;
  const gfx::Insets insets_;
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

  void SetColor(ui::ColorVariant color) override {
    NOTREACHED() << "It does not make sense to `SetColor()` for a painter "
                    "based background.";
  }

 private:
  std::unique_ptr<Painter> painter_;
};

Background::Background() = default;

Background::~Background() = default;

void Background::SetColor(ui::ColorVariant color) {
  color_ = color;
}

void Background::OnViewThemeChanged(View* view) {}

std::optional<gfx::RoundedCornersF> Background::GetRoundedCornerRadii() const {
  return std::nullopt;
}

/////////////////////////////////////////////////////////////////////////////
// Factory methods implementations:
/////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Background> CreateSolidBackground(ui::ColorVariant color) {
  return std::make_unique<SolidBackground>(color);
}

std::unique_ptr<Background> CreateRoundedRectBackground(
    ui::ColorVariant color,
    float radius,
    int for_border_thickness) {
  return CreateRoundedRectBackground(color, gfx::RoundedCornersF{radius},
                                     for_border_thickness);
}

std::unique_ptr<Background> CreateRoundedRectBackground(
    ui::ColorVariant color,
    float top_radius,
    float bottom_radius,
    int for_border_thickness) {
  return CreateRoundedRectBackground(
      color,
      gfx::RoundedCornersF{top_radius, top_radius, bottom_radius,
                           bottom_radius},
      for_border_thickness);
}

std::unique_ptr<Background> CreateRoundedRectBackground(
    ui::ColorVariant color,
    const gfx::RoundedCornersF& radii,
    int for_border_thickness) {
  return CreateRoundedRectBackground(color, radii,
                                     gfx::Insets(for_border_thickness / 2.0f));
}

std::unique_ptr<Background> CreateRoundedRectBackground(
    ui::ColorVariant color,
    const gfx::RoundedCornersF& radii,
    const gfx::Insets& insets) {
  // If the radii is not set, fallback to SolidBackground since it results in
  // more efficient tiling by cc. See crbug.com/1464128.
  if (radii.IsEmpty() && insets.IsEmpty()) {
    return CreateSolidBackground(color);
  }
  return std::make_unique<RoundedRectBackground>(color, radii, insets);
}

std::unique_ptr<Background> CreateThemedVectorIconBackground(
    const ui::ThemedVectorIcon& icon) {
  return std::make_unique<ThemedVectorIconBackground>(icon);
}

std::unique_ptr<Background> CreateBackgroundFromPainter(
    std::unique_ptr<Painter> painter) {
  return std::make_unique<BackgroundPainter>(std::move(painter));
}

}  // namespace views
