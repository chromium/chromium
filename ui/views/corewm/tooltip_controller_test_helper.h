// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_TEST_HELPER_H_
#define UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_TEST_HELPER_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace views {
namespace corewm {

class TooltipController;

namespace test {

// TooltipControllerTestHelper provides access to TooltipControllers private
// state.
class TooltipControllerTestHelper {
 public:
  explicit TooltipControllerTestHelper(TooltipController* controller);
  ~TooltipControllerTestHelper();

  TooltipController* controller() { return controller_; }

  // These are mostly cover methods for TooltipController private methods.
  base::string16 GetTooltipText();
  aura::Window* GetTooltipWindow();
  void UpdateIfRequired();
  void FireTooltipShownTimer();
  bool IsTooltipShownTimerRunning();
  bool IsTooltipVisible();

 private:
  TooltipController* controller_;

  DISALLOW_COPY_AND_ASSIGN(TooltipControllerTestHelper);
};

// Trivial View subclass that lets you set the tooltip text.
class TooltipTestView : public views::View {
 public:
  TooltipTestView();
  ~TooltipTestView() override;

  void set_tooltip_text(base::string16 tooltip_text) {
    tooltip_text_ = tooltip_text;
  }

  // Overridden from views::View
  base::string16 GetTooltipText(const gfx::Point& p) const override;

 private:
  base::string16 tooltip_text_;

  DISALLOW_COPY_AND_ASSIGN(TooltipTestView);
};


}  // namespace test
}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_TEST_HELPER_H_
