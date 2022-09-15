// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_MAC_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_MAC_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/controls/menu/menu_pre_target_handler.h"

namespace ui {
class Event;
}  // namespace ui

namespace views {

// Stops dispatch of key events when they are handled by MenuController.
// While similar to EventMonitorMac, that class does not allow dispatch changes.
class MenuPreTargetHandlerMac : public MenuPreTargetHandler,
                                public NativeWidgetMacEventMonitor::Client {
 public:
  MenuPreTargetHandlerMac(MenuController* controller, Widget* widget);

  MenuPreTargetHandlerMac(const MenuPreTargetHandlerMac&) = delete;
  MenuPreTargetHandlerMac& operator=(const MenuPreTargetHandlerMac&) = delete;

  ~MenuPreTargetHandlerMac() override;

 private:
  // public:
  void NativeWidgetMacEventMonitorOnEvent(ui::Event* event,
                                          bool* was_handled) final;

  std::unique_ptr<NativeWidgetMacEventMonitor> monitor_;
  const raw_ptr<MenuController> controller_;  // Weak. Owns |this|.
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_MAC_H_
