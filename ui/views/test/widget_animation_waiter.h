// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WIDGET_ANIMATION_WAITER_H_
#define UI_VIEWS_TEST_WIDGET_ANIMATION_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Rect;
}

namespace views {
class Widget;

// Class which waits until for a widget to finish animating and verifies
// that the layer transform animation was valid.
class WidgetAnimationWaiter : public ui::LayerAnimationObserver,
                              public views::WidgetObserver {
 public:
  explicit WidgetAnimationWaiter(Widget* widget);
  WidgetAnimationWaiter(Widget* widget, const gfx::Rect& target_bounds);
  ~WidgetAnimationWaiter() override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* sequence) override;

  void WaitForAnimation();

  // Returns true if the animation has completed.
  bool WasValidAnimation();

 private:
  gfx::Rect target_bounds_;

  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
  base::ScopedObservation<ui::LayerAnimator, ui::LayerAnimationObserver>
      layer_animation_observation_{this};

  base::RunLoop run_loop_;
  bool is_valid_animation_ = false;
  bool animation_scheduled_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_WIDGET_ANIMATION_WAITER_H_
