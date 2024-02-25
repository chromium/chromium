// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_animation_waiter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/widget/widget.h"

namespace views {

WidgetAnimationWaiter::WidgetAnimationWaiter(Widget* widget)
    : WidgetAnimationWaiter(widget, /*target_bounds=*/gfx::Rect()) {}

WidgetAnimationWaiter::WidgetAnimationWaiter(Widget* widget,
                                             const gfx::Rect& target_bounds)
    : target_bounds_(target_bounds) {
  widget_observation_.Observe(widget);
  layer_animation_observation_.Observe(widget->GetLayer()->GetAnimator());
}

WidgetAnimationWaiter::~WidgetAnimationWaiter() = default;

void WidgetAnimationWaiter::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  Widget* widget = widget_observation_.GetSource();
  if (!widget->GetLayer()->GetAnimator()->is_animating() &&
      animation_scheduled_) {
    if (!target_bounds_.IsEmpty()) {
      EXPECT_EQ(widget->GetWindowBoundsInScreen(), target_bounds_);
      EXPECT_EQ(widget->GetLayer()->transform(), gfx::Transform());
    }

    is_valid_animation_ = true;
    layer_animation_observation_.Reset();
    run_loop_.Quit();
  }
}

void WidgetAnimationWaiter::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {}

void WidgetAnimationWaiter::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
  animation_scheduled_ = true;
  if (!target_bounds_.IsEmpty())
    EXPECT_NE(widget_observation_.GetSource()->GetLayer()->transform(),
              gfx::Transform());
}

void WidgetAnimationWaiter::WaitForAnimation() {
  run_loop_.Run();
}

bool WidgetAnimationWaiter::WasValidAnimation() {
  return animation_scheduled_ && is_valid_animation_;
}

void WidgetAnimationWaiter::OnWidgetDestroying(Widget* widget) {
  widget_observation_.Reset();
  layer_animation_observation_.Reset();
}

}  // namespace views
