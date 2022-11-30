// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_HANDLER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_HANDLER_H_

#include <stdint.h>

#include "ui/base/ui_base_types.h"

namespace gfx {
class Rect;
}

namespace views {

enum class MenuAnchorPosition;
class MenuButtonController;
class Widget;

// Used internally by MenuRunner to show the menu. Can be set in tests (see
// MenuRunnerTestApi) for mocking running of the menu.
class VIEWS_EXPORT MenuRunnerHandler {
 public:
  virtual ~MenuRunnerHandler() = default;
  virtual void RunMenuAt(Widget* parent,
                         MenuButtonController* button_controller,
                         const gfx::Rect& bounds,
                         MenuAnchorPosition anchor,
                         ui::MenuSourceType source_type,
                         int32_t types) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_HANDLER_H_
