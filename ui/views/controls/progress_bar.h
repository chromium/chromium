// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_PROGRESS_BAR_H_
#define UI_VIEWS_CONTROLS_PROGRESS_BAR_H_

#include <memory>
#include <optional>

#include "ui/color/color_id.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace gfx {
class LinearAnimation;
}

namespace views {

// Progress bar is a control that indicates progress visually.
class VIEWS_EXPORT ProgressBar : public View, public gfx::AnimationDelegate {
  METADATA_HEADER(ProgressBar, View)

 public:
  ProgressBar();

  ProgressBar(const ProgressBar&) = delete;
  ProgressBar& operator=(const ProgressBar&) = delete;

  ~ProgressBar() override;

  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void AddedToWidget() override;
  void OnPaint(gfx::Canvas* canvas) override;

  double GetValue() const;
  // Sets the current value. Values outside of the display range of 0.0-1.0 will
  // be displayed with an infinite loading animation.
  void SetValue(double value);

  // Sets whether the progress bar is paused.
  void SetPaused(bool is_paused);

  // The color of the progress portion.
  SkColor GetForegroundColor() const;
  void SetForegroundColor(SkColor color);
  std::optional<ui::ColorId> GetForegroundColorId() const;
  void SetForegroundColorId(std::optional<ui::ColorId> color_id);

  // The color of the portion that displays potential progress.
  SkColor GetBackgroundColor() const;
  void SetBackgroundColor(SkColor color);
  std::optional<ui::ColorId> GetBackgroundColorId() const;
  void SetBackgroundColorId(std::optional<ui::ColorId> color_id);

  int GetPreferredHeight() const;
  void SetPreferredHeight(int preferred_height);

  // Calculates the rounded corners of the view based on
  // `preferred_corner_radii_`. If `preferred_corner_radii_` was not provided,
  // empty corners will be returned . If any corner radius in
  // `preferred_corner_radii_` is greater than the height of the bar, its value
  // will be capped to the height of the bar.
  gfx::RoundedCornersF GetPreferredCornerRadii() const;

  void SetPreferredCornerRadii(
      std::optional<gfx::RoundedCornersF> preferred_corner_radii);

 protected:
  int preferred_height() const { return preferred_height_; }

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  bool IsIndeterminate();
  bool GetPaused() const { return is_paused_; }
  void OnPaintIndeterminate(gfx::Canvas* canvas);

  // Fire an accessibility event if visible and the progress has changed.
  void MaybeNotifyAccessibilityValueChanged();

  // Current progress to display, should be in the range 0.0 to 1.0.
  double current_value_ = 0.0;

  // Is the progress bar paused.
  bool is_paused_ = false;

  // In DP, the preferred height of this progress bar. This makes it easier to
  // use a ProgressBar with layout managers that size to preferred size.
  int preferred_height_ = 5;

  // The radii to round the progress bar corners with. A value of
  // `std::nullopt` will produce a bar with no rounded corners, otherwise a
  // default value of 3 on all corners will be used.
  std::optional<gfx::RoundedCornersF> preferred_corner_radii_ =
      gfx::RoundedCornersF(3);

  std::optional<SkColor> foreground_color_;
  std::optional<ui::ColorId> foreground_color_id_;
  std::optional<SkColor> background_color_;
  std::optional<ui::ColorId> background_color_id_;

  std::unique_ptr<gfx::LinearAnimation> indeterminate_bar_animation_;

  int last_announced_percentage_ = -1;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, ProgressBar, View)
VIEW_BUILDER_PROPERTY(double, Value)
VIEW_BUILDER_PROPERTY(bool, Paused)
VIEW_BUILDER_PROPERTY(SkColor, ForegroundColor)
VIEW_BUILDER_PROPERTY(std::optional<ui::ColorId>, ForegroundColorId)
VIEW_BUILDER_PROPERTY(SkColor, BackgroundColor)
VIEW_BUILDER_PROPERTY(std::optional<ui::ColorId>, BackgroundColorId)
VIEW_BUILDER_PROPERTY(int, PreferredHeight)
VIEW_BUILDER_PROPERTY(std::optional<gfx::RoundedCornersF>, PreferredCornerRadii)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ProgressBar)

#endif  // UI_VIEWS_CONTROLS_PROGRESS_BAR_H_
