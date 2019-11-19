// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_bar.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace views {

namespace {

// In DP, the amount to round the corners of the progress bar (both bg and
// fg, aka slice).
constexpr int kCornerRadius = 3;

// Adds a rectangle to the path. The corners will be rounded if there is room.
void AddPossiblyRoundRectToPath(const gfx::Rect& rectangle,
                                bool allow_round_corner,
                                SkPath* path) {
  if (!allow_round_corner || rectangle.height() < kCornerRadius) {
    path->addRect(gfx::RectToSkRect(rectangle));
  } else {
    path->addRoundRect(gfx::RectToSkRect(rectangle), kCornerRadius,
                       kCornerRadius);
  }
}

}  // namespace

ProgressBar::ProgressBar(int preferred_height, bool allow_round_corner)
    : preferred_height_(preferred_height),
      allow_round_corner_(allow_round_corner) {
  EnableCanvasFlippingForRTLUI(true);
}

ProgressBar::~ProgressBar() = default;

void ProgressBar::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kProgressIndicator;
}

gfx::Size ProgressBar::CalculatePreferredSize() const {
  // The width will typically be ignored.
  gfx::Size pref_size(1, preferred_height_);
  gfx::Insets insets = GetInsets();
  pref_size.Enlarge(insets.width(), insets.height());
  return pref_size;
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
    indeterminate_bar_animation_->SetDuration(base::TimeDelta::FromSeconds(2));
    indeterminate_bar_animation_->Start();
  } else {
    indeterminate_bar_animation_.reset();
    OnPropertyChanged(&current_value_, kPropertyEffectsPaint);
  }
}

SkColor ProgressBar::GetForegroundColor() const {
  if (foreground_color_)
    return foreground_color_.value();

  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ProminentButtonColor);
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
  int bar1_end_x = std::round(
      content_bounds.width() * std::min(1.0, bar1_left + bar1_width));
  int bar2_start_x = std::round(content_bounds.width() * bar2_left);
  int bar2_end_x = std::round(
      content_bounds.width() * std::min(1.0, bar2_left + bar2_width));

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

BEGIN_METADATA(ProgressBar)
METADATA_PARENT_CLASS(View)
ADD_PROPERTY_METADATA(ProgressBar, SkColor, ForegroundColor)
ADD_PROPERTY_METADATA(ProgressBar, SkColor, BackgroundColor)
END_METADATA()

}  // namespace views
