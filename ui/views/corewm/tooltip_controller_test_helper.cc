// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller_test_helper.h"

#include "ui/aura/window.h"
#include "ui/wm/public/activation_change_observer.h"

namespace views::corewm::test {

TooltipControllerTestHelper::TooltipControllerTestHelper(
    TooltipController* controller)
    : controller_(controller) {
  controller_->state_manager_->SetTooltipShowDelayedForTesting(false);
}

TooltipControllerTestHelper::~TooltipControllerTestHelper() = default;

const std::u16string& TooltipControllerTestHelper::GetTooltipText() {
  return controller_->state_manager_->tooltip_text();
}

const aura::Window* TooltipControllerTestHelper::GetTooltipParentWindow() {
  return controller_->state_manager_->tooltip_parent_window();
}

const aura::Window* TooltipControllerTestHelper::GetObservedWindow() {
  return controller_->observed_window_;
}

const gfx::Point& TooltipControllerTestHelper::GetTooltipPosition() {
  return controller_->state_manager_->position_;
}

void TooltipControllerTestHelper::HideAndReset() {
  controller_->HideAndReset();
}

void TooltipControllerTestHelper::UpdateIfRequired(TooltipTrigger trigger) {
  controller_->UpdateIfRequired(trigger);
}

void TooltipControllerTestHelper::FireHideTooltipTimer() {
  controller_->state_manager_->HideAndReset();
}

bool TooltipControllerTestHelper::IsHideTooltipTimerRunning() {
  return controller_->state_manager_->IsWillHideTooltipTimerRunningForTesting();
}

bool TooltipControllerTestHelper::IsTooltipVisible() {
  return controller_->state_manager_->IsVisible();
}

void TooltipControllerTestHelper::SetTooltipShowDelayEnable(
    bool tooltip_show_delay) {
  controller_->state_manager_->SetTooltipShowDelayedForTesting(
      tooltip_show_delay);
}

void TooltipControllerTestHelper::MockWindowActivated(aura::Window* window,
                                                      bool active) {
  aura::Window* gained_active = active ? window : nullptr;
  aura::Window* lost_active = active ? nullptr : window;
  controller_->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      gained_active, lost_active);
}

TooltipTestView::TooltipTestView() = default;

TooltipTestView::~TooltipTestView() = default;

std::u16string TooltipTestView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_text_;
}

}  // namespace views::corewm::test
