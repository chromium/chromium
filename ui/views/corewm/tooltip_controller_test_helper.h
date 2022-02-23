// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_TEST_HELPER_H_
#define UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_TEST_HELPER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/corewm/tooltip_state_manager.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace views {
namespace corewm {


namespace test {

// TooltipControllerTestHelper provides access to TooltipControllers private
// state.
class TooltipControllerTestHelper {
 public:
  explicit TooltipControllerTestHelper(TooltipController* controller);

  TooltipControllerTestHelper(const TooltipControllerTestHelper&) = delete;
  TooltipControllerTestHelper& operator=(const TooltipControllerTestHelper&) =
      delete;

  ~TooltipControllerTestHelper();

  TooltipController* controller() { return controller_; }

  TooltipStateManager* state_manager() {
    return controller_->state_manager_.get();
  }

  // These are mostly cover methods for TooltipController private methods.
  const std::u16string& GetTooltipText();
  const aura::Window* GetTooltipParentWindow();
  const aura::Window* GetObservedWindow();
  const gfx::Point& GetTooltipPosition();
  void HideAndReset();
  void UpdateIfRequired(TooltipTrigger trigger);
  void FireHideTooltipTimer();
  bool IsHideTooltipTimerRunning();
  bool IsTooltipVisible();
  void SetTooltipShowDelayEnable(bool tooltip_show_delay);
  void MockWindowActivated(aura::Window* window, bool active);

 private:
  raw_ptr<TooltipController> controller_;
};

// Trivial View subclass that lets you set the tooltip text.
class TooltipTestView : public views::View {
 public:
  TooltipTestView();

  TooltipTestView(const TooltipTestView&) = delete;
  TooltipTestView& operator=(const TooltipTestView&) = delete;

  ~TooltipTestView() override;

  void set_tooltip_text(std::u16string tooltip_text) {
    tooltip_text_ = tooltip_text;
  }

  // Overridden from views::View
  std::u16string GetTooltipText(const gfx::Point& p) const override;

 private:
  std::u16string tooltip_text_;
};

}  // namespace test
}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_TEST_HELPER_H_
