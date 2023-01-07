// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_H_

#include <memory>

namespace views {

class MenuController;
class Widget;

// A MenuPreTargetHandler is responsible for intercepting events destined for
// another widget (the menu's owning widget) and letting the menu's controller
// try dispatching them first.
class MenuPreTargetHandler {
 public:
  MenuPreTargetHandler(const MenuPreTargetHandler&) = delete;
  MenuPreTargetHandler& operator=(const MenuPreTargetHandler&) = delete;

  virtual ~MenuPreTargetHandler() = default;

  // There are separate implementations of this method for Aura platforms and
  // for Mac.
  static std::unique_ptr<MenuPreTargetHandler> Create(
      MenuController* controller,
      Widget* owner);

 protected:
  MenuPreTargetHandler() = default;
};

}  // namespace views

#endif  //  UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_H_
