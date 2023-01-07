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
#include "third_party/skia/include/core/SkPath.h"
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
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// In DP, the amount to round the corners of the progress bar (both bg and
// fg, aka slice).
constexpr int kCornerRadius = 3;
constexpr int kSmallCornerRadius = 1;

// Adds a rectangle to the path. The corners will be rounded with regular corner
// radius if the progress bar height is larger than the regular corner radius.
// Otherwise the corners will be rounded with the small corner radius if there
// is room for it.
void AddPossiblyRoundRectToPath(const gfx::Rect& rectangle,
                                bool allow_round_corner,
                                SkPath* path) {
  if (!allow_round_corner || rectangle.height() < kSmallCornerRadius) {
    path->addRect(gfx::RectToSkRect(rectangle));
  } else if (rectangle.height() < kCornerRadius) {
    path->addRoundRect(gfx::RectToSkRect(rectangle), kSmallCornerRadius,
                       kSmallCornerRadius);
  } else {
    path->addRoundRect(gfx::RectToSkRect(rectangle), kCornerRadius,
                       kCornerRadius);
  }
}

int RoundToPercent(double fractional_value) {
  return static_cast<int>(fractional_value * 100);
}

}  // namespace

ProgressBar::ProgressBar(int preferred_height, bool allow_round_corner)
    : preferred_height_(preferred_height),
      allow_round_corner_(allow_round_corner) {
  SetFlipCanvasOnPaintForRTLUI(true);
}

ProgressBar::~ProgressBar() = default;

void ProgressBar::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kProgressIndicator;
  if (IsIndeterminate())
    node_data->RemoveStringAttribute(ax::mojom::StringAttribute::kValue);
  else
    node_data->SetValue(base::FormatPercent(RoundToPercent(current_value_)));
}

gfx::Size ProgressBar::CalculatePreferredSize() const {
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
  if (IsIndeterminate())
    return OnPaintIndeterminate(canvas);

  gfx::Rect content_bounds = GetContentsBounds();

  // Draw background.
  SkPath background_path;
  AddPossiblyRoundRectToPath(content_bounds, allow_round_corner_,
                             &background_path);
  cc::PaintFlags background_flags;
  background_flags.setStyle(cc::PaintFlags::kFill_Style);
  background_flags.setAntiAlias(true);
  background_flags.setColor(GetBackgroundColor());
  canvas->DrawPath(background_path, background_flags);

  // Draw slice.
  SkPath slice_path;
  const int slice_width = static_cast<int>(
      content_bounds.width() * std::min(current_value_, 1.0) + 0.5);
  if (slice_width < 1)
    return;

  gfx::Rect slice_bounds = content_bounds;
  slice_bounds.set_width(slice_width);
  AddPossiblyRoundRectToPath(slice_bounds, allow_round_corner_, &slice_path);

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

  if (adjusted_value == current_value_)
    return;

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
  if (is_paused_ == is_paused)
    return;

  is_paused_ = is_paused;
  OnPropertyChanged(&is_paused_, kPropertyEffectsPaint);
}

SkColor ProgressBar::GetForegroundColor() const {
  if (foreground_color_)
    return foreground_color_.value();

  return GetColorProvider()->GetColor(GetPaused() ? ui::kColorProgressBarPaused
                                                  : ui::kColorProgressBar);
}

void ProgressBar::SetForegroundColor(SkColor color) {
  if (foreground_color_ == color)
    return;

  foreground_color_ = color;
  OnPropertyChanged(&foreground_color_, kPropertyEffectsPaint);
}

SkColor ProgressBar::GetBackgroundColor() const {
  return background_color_.value_or(
      color_utils::BlendTowardMaxContrast(GetForegroundColor(), 0xCC));
}

void ProgressBar::SetBackgroundColor(SkColor color) {
  if (background_color_ == color)
    return;

  background_color_ = color;
  OnPropertyChanged(&background_color_, kPropertyEffectsPaint);
}

void ProgressBar::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, indeterminate_bar_animation_.get());
  DCHECK(IsIndeterminate());
  SchedulePaint();
}

void ProgressBar::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, indeterminate_bar_animation_.get());
  // Restarts animation.
  if (IsIndeterminate())
    indeterminate_bar_animation_->Start();
}

bool ProgressBar::IsIndeterminate() {
  return current_value_ < 0.0;
}

void ProgressBar::OnPaintIndeterminate(gfx::Canvas* canvas) {
  gfx::Rect content_bounds = GetContentsBounds();

  // Draw background.
  SkPath background_path;
  AddPossiblyRoundRectToPath(content_bounds, allow_round_corner_,
                             &background_path);
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
  AddPossiblyRoundRectToPath(slice_bounds, allow_round_corner_, &slice_path);
  slice_bounds.set_x(content_bounds.x() + bar2_start_x);
  slice_bounds.set_width(bar2_end_x - bar2_start_x);
  AddPossiblyRoundRectToPath(slice_bounds, allow_round_corner_, &slice_path);

  cc::PaintFlags slice_flags;
  slice_flags.setStyle(cc::PaintFlags::kFill_Style);
  slice_flags.setAntiAlias(true);
  slice_flags.setColor(GetForegroundColor());
  canvas->DrawPath(slice_path, slice_flags);
}

void ProgressBar::MaybeNotifyAccessibilityValueChanged() {
  if (!GetWidget() || !GetWidget()->IsVisible() ||
      RoundToPercent(current_value_) == last_announced_percentage_) {
    return;
  }
  last_announced_percentage_ = RoundToPercent(current_value_);
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

BEGIN_METADATA(ProgressBar, View)
ADD_PROPERTY_METADATA(SkColor, ForegroundColor, ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(SkColor, BackgroundColor, ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(bool, Paused)
END_METADATA

}  // namespace views
