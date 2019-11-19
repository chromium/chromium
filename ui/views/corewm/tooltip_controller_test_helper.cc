// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller_test_helper.h"

#include "ui/aura/window.h"
#include "ui/views/corewm/tooltip_controller.h"

namespace views {
namespace corewm {
namespace test {

TooltipControllerTestHelper::TooltipControllerTestHelper(
    TooltipController* controller)
    : controller_(controller) {
  controller_->DisableTooltipShowDelay();
}

TooltipControllerTestHelper::~TooltipControllerTestHelper() = default;

base::string16 TooltipControllerTestHelper::GetTooltipText() {
  return controller_->tooltip_text_;
}

aura::Window* TooltipControllerTestHelper::GetTooltipWindow() {
  return controller_->tooltip_window_;
}

void TooltipControllerTestHelper::UpdateIfRequired() {
  controller_->UpdateIfRequired();
}

void TooltipControllerTestHelper::FireTooltipShownTimer() {
  controller_->tooltip_shown_timer_.Stop();
  controller_->TooltipShownTimerFired();
}

bool TooltipControllerTestHelper::IsTooltipShownTimerRunning() {
  return controller_->tooltip_shown_timer_.IsRunning();
}

bool TooltipControllerTestHelper::IsTooltipVisible() {
  return controller_->IsTooltipVisible();
}

TooltipTestView::TooltipTestView() = default;

TooltipTestView::~TooltipTestView() = default;

base::string16 TooltipTestView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_text_;
}

}  // namespace test
}  // namespace corewm
}  // namespace views
