// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_PROGRESS_BAR_H_
#define UI_VIEWS_CONTROLS_PROGRESS_BAR_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
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
  ~ProgressBar() override;

  // Overridden from View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  double GetValue() const;
  // Sets the current value. Values outside of the display range of 0.0-1.0 will
  // be displayed with an infinite loading animation.
  void SetValue(double value);

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
  void OnPaintIndeterminate(gfx::Canvas* canvas);

  // Current progress to display, should be in the range 0.0 to 1.0.
  double current_value_ = 0.0;

  // In DP, the preferred height of this progress bar.
  const int preferred_height_;

  const bool allow_round_corner_;

  base::Optional<SkColor> foreground_color_;
  base::Optional<SkColor> background_color_;

  std::unique_ptr<gfx::LinearAnimation> indeterminate_bar_animation_;

  DISALLOW_COPY_AND_ASSIGN(ProgressBar);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_PROGRESS_BAR_H_
