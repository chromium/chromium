// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_PROGRESS_BAR_H_
#define UI_VIEWS_CONTROLS_PROGRESS_BAR_H_

#include <memory>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"

namespace gfx {
class LinearAnimation;
}

namespace views {

// Progress bar is a control that indicates progress visually.
class VIEWS_EXPORT ProgressBar : public View, public gfx::AnimationDelegate {
 public:
  METADATA_HEADER(ProgressBar);

  // The preferred height parameter makes it easier to use a ProgressBar with
  // layout managers that size to preferred size.
  explicit ProgressBar(int preferred_height = 5,
                       bool allow_round_corner = true);

  ProgressBar(const ProgressBar&) = delete;
  ProgressBar& operator=(const ProgressBar&) = delete;

  ~ProgressBar() override;

  // View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
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

  // The color of the portion that displays potential progress.
  SkColor GetBackgroundColor() const;
  void SetBackgroundColor(SkColor color);

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

  // In DP, the preferred height of this progress bar.
  const int preferred_height_;

  const bool allow_round_corner_;

  absl::optional<SkColor> foreground_color_;
  absl::optional<SkColor> background_color_;

  std::unique_ptr<gfx::LinearAnimation> indeterminate_bar_animation_;

  int last_announced_percentage_ = -1;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_PROGRESS_BAR_H_
