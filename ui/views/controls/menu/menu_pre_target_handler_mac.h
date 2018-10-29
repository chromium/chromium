// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_MAC_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_MAC_H_

#include "ui/views/controls/menu/menu_pre_target_handler.h"

#include "ui/base/cocoa/weak_ptr_nsobject.h"

namespace views {

// Stops dispatch of key events when they are handled by MenuController.
// While similar to EventMonitorMac, that class does not allow dispatch changes.
class MenuPreTargetHandlerMac : public MenuPreTargetHandler {
 public:
  MenuPreTargetHandlerMac(MenuController* controller, Widget* widget);
  ~MenuPreTargetHandlerMac() override;

 private:
  MenuController* controller_;  // Weak. Owns |this|.
  id monitor_;
  ui::WeakPtrNSObjectFactory<MenuPreTargetHandlerMac> factory_;

  DISALLOW_COPY_AND_ASSIGN(MenuPreTargetHandlerMac);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_MAC_H_
