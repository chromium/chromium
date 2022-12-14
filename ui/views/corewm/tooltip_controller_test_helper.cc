// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller_test_helper.h"

#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/tooltip_observer.h"

namespace views::corewm::test {

TooltipControllerTestHelper::TooltipControllerTestHelper(
    TooltipController* controller)
    : controller_(controller) {
  SkipTooltipShowDelay(true);
}

TooltipControllerTestHelper::~TooltipControllerTestHelper() = default;

bool TooltipControllerTestHelper::UseServerSideTooltip() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformRuntimeProperties()
      .supports_tooltip;
#else
  return false;
#endif
}

const std::u16string& TooltipControllerTestHelper::GetTooltipText() {
  return state_manager()->tooltip_text();
}

aura::Window* TooltipControllerTestHelper::GetTooltipParentWindow() {
  return state_manager()->tooltip_parent_window_;
}

const aura::Window* TooltipControllerTestHelper::GetObservedWindow() {
  return controller_->observed_window_;
}

const gfx::Point& TooltipControllerTestHelper::GetTooltipPosition() {
  return state_manager()->position_;
}

base::TimeDelta TooltipControllerTestHelper::GetShowTooltipDelay() {
  return controller_->GetShowTooltipDelay();
}

void TooltipControllerTestHelper::HideAndReset() {
  controller_->HideAndReset();
}

void TooltipControllerTestHelper::UpdateIfRequired(TooltipTrigger trigger) {
  controller_->UpdateIfRequired(trigger);
}

void TooltipControllerTestHelper::FireHideTooltipTimer() {
  state_manager()->HideAndReset();
}

void TooltipControllerTestHelper::AddObserver(wm::TooltipObserver* observer) {
  controller_->AddObserver(observer);
}

void TooltipControllerTestHelper::RemoveObserver(
    wm::TooltipObserver* observer) {
  controller_->RemoveObserver(observer);
}

bool TooltipControllerTestHelper::IsWillShowTooltipTimerRunning() {
  return state_manager()->IsWillShowTooltipTimerRunningForTesting();
}

bool TooltipControllerTestHelper::IsWillHideTooltipTimerRunning() {
  return state_manager()->IsWillHideTooltipTimerRunningForTesting();
}

bool TooltipControllerTestHelper::IsTooltipVisible() {
  return state_manager()->IsVisible();
}

void TooltipControllerTestHelper::SkipTooltipShowDelay(bool enable) {
  controller_->skip_show_delay_for_testing_ = enable;
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
