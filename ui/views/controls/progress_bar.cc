// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_bar.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/i18n/number_formatting.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Adds a rectangle to the path.
void AddPossiblyRoundRectToPath(
    const gfx::Rect& rectangle,
    const gfx::RoundedCornersF& preferred_corner_radii,
    SkPath* path) {
  if (preferred_corner_radii.IsEmpty() || rectangle.height() == 0) {
    path->addRect(gfx::RectToSkRect(rectangle));
    return;
  }
  SkVector radii[4] = {{preferred_corner_radii.upper_left(),
                        preferred_corner_radii.upper_left()},
                       {preferred_corner_radii.upper_right(),
                        preferred_corner_radii.upper_right()},
                       {preferred_corner_radii.lower_right(),
                        preferred_corner_radii.lower_right()},
                       {preferred_corner_radii.lower_left(),
                        preferred_corner_radii.lower_left()}};

  SkRRect rr;
  rr.setRectRadii(gfx::RectToSkRect(rectangle), radii);
  path->addRRect(rr);
}

int RoundToPercent(double fractional_value) {
  return static_cast<int>(fractional_value * 100);
}

}  // namespace

ProgressBar::ProgressBar() {
  SetFlipCanvasOnPaintForRTLUI(true);
  GetViewAccessibility().SetRole(ax::mojom::Role::kProgressIndicator);
}

ProgressBar::~ProgressBar() = default;

gfx::Size ProgressBar::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  // The width will typically be ignored.
  gfx::Size pref_size(1, preferred_height_);
  gfx::Insets insets = GetInsets();
  pref_size.Enlarge(insets.width(), insets.height());
  return pref_size;
}

void ProgressBar::VisibilityChanged(View* starting_from, bool is_visible) {
  MaybeNotifyAccessibilityValueChanged();
}

void ProgressBar::AddedToWidget() {
  MaybeNotifyAccessibilityValueChanged();
}

void ProgressBar::OnPaint(gfx::Canvas* canvas) {
  if (IsIndeterminate()) {
    return OnPaintIndeterminate(canvas);
  }

  gfx::Rect content_bounds = GetContentsBounds();

  // Draw background.
  SkPath background_path;
  gfx::RoundedCornersF rounded_corners = GetPreferredCornerRadii();
  AddPossiblyRoundRectToPath(content_bounds, rounded_corners, &background_path);
  cc::PaintFlags background_flags;
  background_flags.setStyle(cc::PaintFlags::kFill_Style);
  background_flags.setAntiAlias(true);
  background_flags.setColor(GetBackgroundColor());
  canvas->DrawPath(background_path, background_flags);

  // Draw slice.
  SkPath slice_path;
  const int slice_width = static_cast<int>(
      content_bounds.width() * std::min(current_value_, 1.0) + 0.5);
  if (slice_width < 1) {
    return;
  }

  gfx::Rect slice_bounds = content_bounds;
  slice_bounds.set_width(slice_width);
  AddPossiblyRoundRectToPath(slice_bounds, rounded_corners, &slice_path);

  cc::PaintFlags slice_flags;
  slice_flags.setStyle(cc::PaintFlags::kFill_Style);
  slice_flags.setAntiAlias(true);
  slice_flags.setColor(GetForegroundColor());
  canvas->DrawPath(slice_path, slice_flags);
}

double ProgressBar::GetValue() const {
  return current_value_;
}

void ProgressBar::SetValue(double value) {
  double adjusted_value = (value < 0.0 || value > 1.0) ? -1.0 : value;

  if (adjusted_value == current_value_) {
    return;
  }

  current_value_ = adjusted_value;
  if (IsIndeterminate()) {
    indeterminate_bar_animation_ = std::make_unique<gfx::LinearAnimation>(this);
    indeterminate_bar_animation_->SetDuration(base::Seconds(2));
    indeterminate_bar_animation_->Start();
  } else {
    indeterminate_bar_animation_.reset();
    OnPropertyChanged(&current_value_, kPropertyEffectsPaint);
  }

  MaybeNotifyAccessibilityValueChanged();
}

void ProgressBar::SetPaused(bool is_paused) {
  if (is_paused_ == is_paused) {
    return;
  }

  is_paused_ = is_paused;
  OnPropertyChanged(&is_paused_, kPropertyEffectsPaint);
}

SkColor ProgressBar::GetForegroundColor() const {
  if (foreground_color_) {
    return foreground_color_.value();
  }

  return GetColorProvider()->GetColor(foreground_color_id_.value_or(
      GetPaused() ? ui::kColorProgressBarPaused : ui::kColorProgressBar));
}

void ProgressBar::SetForegroundColor(SkColor color) {
  if (foreground_color_ == color) {
    return;
  }

  foreground_color_ = color;
  foreground_color_id_ = std::nullopt;
  OnPropertyChanged(&foreground_color_, kPropertyEffectsPaint);
}

std::optional<ui::ColorId> ProgressBar::GetForegroundColorId() const {
  return foreground_color_id_;
}

void ProgressBar::SetForegroundColorId(std::optional<ui::ColorId> color_id) {
  if (foreground_color_id_ == color_id) {
    return;
  }

  foreground_color_id_ = color_id;
  foreground_color_ = std::nullopt;
  OnPropertyChanged(&foreground_color_id_, kPropertyEffectsPaint);
}

SkColor ProgressBar::GetBackgroundColor() const {
  if (background_color_id_) {
    return GetColorProvider()->GetColor(background_color_id_.value());
  }

  return background_color_.value_or(
      GetColorProvider()->GetColor(ui::kColorProgressBarBackground));
}

void ProgressBar::SetBackgroundColor(SkColor color) {
  if (background_color_ == color) {
    return;
  }

  background_color_ = color;
  background_color_id_ = std::nullopt;
  OnPropertyChanged(&background_color_, kPropertyEffectsPaint);
}

std::optional<ui::ColorId> ProgressBar::GetBackgroundColorId() const {
  return background_color_id_;
}

void ProgressBar::SetBackgroundColorId(std::optional<ui::ColorId> color_id) {
  if (background_color_id_ == color_id) {
    return;
  }

  background_color_id_ = color_id;
  background_color_ = std::nullopt;
  OnPropertyChanged(&background_color_id_, kPropertyEffectsPaint);
}

int ProgressBar::GetPreferredHeight() const {
  return preferred_height_;
}

void ProgressBar::SetPreferredHeight(int preferred_height) {
  if (preferred_height_ == preferred_height) {
    return;
  }
  preferred_height_ = preferred_height;
  OnPropertyChanged(&preferred_height_, kPropertyEffectsPreferredSizeChanged);
}

gfx::RoundedCornersF ProgressBar::GetPreferredCornerRadii() const {
  if (!preferred_corner_radii_) {
    return gfx::RoundedCornersF(0);
  }
  const float max_radius = GetContentsBounds().height();

  // No corner should have a radius greater than the height of the bar.
  return gfx::RoundedCornersF(
      std::min(max_radius, preferred_corner_radii_->upper_left()),
      std::min(max_radius, preferred_corner_radii_->upper_right()),
      std::min(max_radius, preferred_corner_radii_->lower_right()),
      std::min(max_radius, preferred_corner_radii_->lower_left()));
}

void ProgressBar::SetPreferredCornerRadii(
    std::optional<gfx::RoundedCornersF> preferred_corner_radii) {
  if (preferred_corner_radii_ == preferred_corner_radii) {
    return;
  }
  preferred_corner_radii_ = preferred_corner_radii;
  OnPropertyChanged(&preferred_corner_radii_, kPropertyEffectsPaint);
}

void ProgressBar::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, indeterminate_bar_animation_.get());
  DCHECK(IsIndeterminate());
  SchedulePaint();
}

void ProgressBar::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, indeterminate_bar_animation_.get());
  // Restarts animation.
  if (IsIndeterminate()) {
    indeterminate_bar_animation_->Start();
  }
}

bool ProgressBar::IsIndeterminate() {
  return current_value_ < 0.0;
}

void ProgressBar::OnPaintIndeterminate(gfx::Canvas* canvas) {
  gfx::Rect content_bounds = GetContentsBounds();

  // Draw background.
  SkPath background_path;
  gfx::RoundedCornersF rounded_corners = GetPreferredCornerRadii();
  AddPossiblyRoundRectToPath(content_bounds, rounded_corners, &background_path);
  cc::PaintFlags background_flags;
  background_flags.setStyle(cc::PaintFlags::kFill_Style);
  background_flags.setAntiAlias(true);
  background_flags.setColor(GetBackgroundColor());
  canvas->DrawPath(background_path, background_flags);

  // Draw slice.
  SkPath slice_path;
  double time = indeterminate_bar_animation_->GetCurrentValue();

  // The animation spec corresponds to the material design lite's parameter.
  // (cf. https://github.com/google/material-design-lite/)
  double bar1_left;
  double bar1_width;
  double bar2_left;
  double bar2_width;
  if (time < 0.50) {
    bar1_left = time / 2;
    bar1_width = time * 1.5;
    bar2_left = 0;
    bar2_width = 0;
  } else if (time < 0.75) {
    bar1_left = time * 3 - 1.25;
    bar1_width = 0.75 - (time - 0.5) * 3;
    bar2_left = 0;
    bar2_width = time - 0.5;
  } else {
    bar1_left = 1;
    bar1_width = 0;
    bar2_left = (time - 0.75) * 4;
    bar2_width = 0.25 - (time - 0.75);
  }

  int bar1_start_x = std::round(content_bounds.width() * bar1_left);
  int bar1_end_x = std::round(content_bounds.width() *
                              std::min(1.0, bar1_left + bar1_width));
  int bar2_start_x = std::round(content_bounds.width() * bar2_left);
  int bar2_end_x = std::round(content_bounds.width() *
                              std::min(1.0, bar2_left + bar2_width));

  gfx::Rect slice_bounds = content_bounds;
  slice_bounds.set_x(content_bounds.x() + bar1_start_x);
  slice_bounds.set_width(bar1_end_x - bar1_start_x);
  AddPossiblyRoundRectToPath(slice_bounds, rounded_corners, &slice_path);
  slice_bounds.set_x(content_bounds.x() + bar2_start_x);
  slice_bounds.set_width(bar2_end_x - bar2_start_x);
  AddPossiblyRoundRectToPath(slice_bounds, rounded_corners, &slice_path);

  cc::PaintFlags slice_flags;
  slice_flags.setStyle(cc::PaintFlags::kFill_Style);
  slice_flags.setAntiAlias(true);
  slice_flags.setColor(GetForegroundColor());
  canvas->DrawPath(slice_path, slice_flags);
}

void ProgressBar::MaybeNotifyAccessibilityValueChanged() {
  // Exit early if ProgressBar is Indeterminate or not visible.
  if (IsIndeterminate()) {
    GetViewAccessibility().RemoveValue();
    return;
  }
  if (!GetWidget() || !GetWidget()->IsVisible() ||
      RoundToPercent(current_value_) == last_announced_percentage_) {
    return;
  }
  last_announced_percentage_ = RoundToPercent(current_value_);
  GetViewAccessibility().SetValue(
      base::FormatPercent(last_announced_percentage_));
}

BEGIN_METADATA(ProgressBar)
ADD_PROPERTY_METADATA(int, PreferredHeight)
ADD_PROPERTY_METADATA(std::optional<gfx::RoundedCornersF>, PreferredCornerRadii)
ADD_PROPERTY_METADATA(SkColor, ForegroundColor, ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(SkColor, BackgroundColor, ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(std::optional<ui::ColorId>, ForegroundColorId);
ADD_PROPERTY_METADATA(std::optional<ui::ColorId>, BackgroundColorId);
ADD_PROPERTY_METADATA(bool, Paused)
END_METADATA

}  // namespace views
