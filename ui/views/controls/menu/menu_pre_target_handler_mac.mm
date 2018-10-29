// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_pre_target_handler_mac.h"

#import <Cocoa/Cocoa.h>

#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"

namespace views {

MenuPreTargetHandlerMac::MenuPreTargetHandlerMac(MenuController* controller,
                                                 Widget* widget)
    : controller_(controller), factory_(this) {
  gfx::NativeWindow target_window = widget->GetNativeWindow();

  // Capture a WeakPtr via NSObject. This allows the block to detect another
  // event monitor for the same event deleting |this|.
  WeakPtrNSObject* handle = factory_.handle();

  auto block = ^NSEvent*(NSEvent* event) {
    if (!ui::WeakPtrNSObjectFactory<MenuPreTargetHandlerMac>::Get(handle))
      return event;

    if (!target_window || [event window] == target_window.GetNativeNSWindow()) {
      std::unique_ptr<ui::Event> ui_event = ui::EventFromNative(event);
      if (ui_event && ui_event->IsKeyEvent() &&
          controller_->OnWillDispatchKeyEvent(ui_event->AsKeyEvent()) !=
              ui::POST_DISPATCH_PERFORM_DEFAULT) {
        // Return nil so the event will not proceed through normal dispatch.
        return nil;
      }
    }
    return event;
  };

  monitor_ = [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask
                                                   handler:block];
}

MenuPreTargetHandlerMac::~MenuPreTargetHandlerMac() {
  [NSEvent removeMonitor:monitor_];
}

// static
std::unique_ptr<MenuPreTargetHandler> MenuPreTargetHandler::Create(
    MenuController* controller,
    Widget* widget) {
  return std::make_unique<MenuPreTargetHandlerMac>(controller, widget);
}

}  // namespace views
