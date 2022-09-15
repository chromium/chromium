// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_pre_target_handler_mac.h"

#import <Cocoa/Cocoa.h>

#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"

namespace views {

MenuPreTargetHandlerMac::MenuPreTargetHandlerMac(MenuController* controller,
                                                 Widget* widget)
    : controller_(controller) {
  gfx::NativeWindow target_window = widget->GetNativeWindow();
  auto* host =
      views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(target_window);
  CHECK(host);
  monitor_ = host->AddEventMonitor(this);
}

MenuPreTargetHandlerMac::~MenuPreTargetHandlerMac() = default;

void MenuPreTargetHandlerMac::NativeWidgetMacEventMonitorOnEvent(
    ui::Event* ui_event,
    bool* was_handled) {
  if (*was_handled)
    return;
  if (!ui_event->IsKeyEvent())
    return;
  *was_handled = controller_->OnWillDispatchKeyEvent(ui_event->AsKeyEvent()) !=
                 ui::POST_DISPATCH_PERFORM_DEFAULT;
}

// static
std::unique_ptr<MenuPreTargetHandler> MenuPreTargetHandler::Create(
    MenuController* controller,
    Widget* widget) {
  return std::make_unique<MenuPreTargetHandlerMac>(controller, widget);
}

}  // namespace views
