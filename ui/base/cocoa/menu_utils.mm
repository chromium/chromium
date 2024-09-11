// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/menu_utils.h"

#include <optional>

#import <AppKit/AppKit.h>

#import "base/mac/scoped_sending_event.h"
#import "base/message_loop/message_pump_apple.h"
#include "base/task/current_thread.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace ui {

NSEvent* EventForPositioningContextMenu(const gfx::Point& anchor,
                                        NSWindow* window) {
  NSPoint location_in_window =
      [window convertPointFromScreen:gfx::ScreenPointToNSPoint(anchor)];
  return EventForPositioningContextMenuRelativeToWindow(location_in_window,
                                                        window);
}

NSEvent* EventForPositioningContextMenuRelativeToWindow(const NSPoint& anchor,
                                                        NSWindow* window) {
  NSEvent* event = NSApp.currentEvent;
  switch (event.type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
      return event;
    default:
      break;
  }
  return [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                            location:anchor
                       modifierFlags:0
                           timestamp:0
                        windowNumber:window.windowNumber
                             context:nil
                         eventNumber:0
                          clickCount:1
                            pressure:0];
}

void ShowContextMenu(NSMenu* menu,
                     NSEvent* event,
                     NSView* view,
                     bool allow_nested_tasks,
                     ElementContext context) {
  // Make sure events can be pumped while the menu is up.
  std::optional<
      base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop>
      allow;
  if (allow_nested_tasks) {
    allow.emplace();
  }

  // One of the events that could be pumped is |window.close()|.
  // User-initiated event-tracking loops protect against this by
  // setting flags in -[CrApplication sendEvent:], but since
  // web-content menus are initiated by IPC message the setup has to
  // be done manually.
  std::optional<base::mac::ScopedSendingEvent> sendingEventScoper;
  if (allow_nested_tasks) {
    sendingEventScoper.emplace();
  }

  // Ensure the UI can update while the menu is fading out.
  base::ScopedPumpMessagesInPrivateModes pump_private;

  if (context) {
    ui::ElementTrackerMac::GetInstance()->NotifyMenuWillShow(menu, context);
  }

  // Show the menu.
  [NSMenu popUpContextMenu:menu withEvent:event forView:view];

  if (context) {
    // We expect to see the following order of events:
    //
    // - menu will show
    // - element shown
    // - element activated (optional)
    // - element hidden
    // - menu completed
    //
    // However, the OS notification for "element activated" fires *after* the OS
    // notification for "element hidden", so Chromium code handling the "element
    // hidden" callback responds by doing a post to the main dispatch queue.
    // Therefore, because there's already a post on the main dispatch queue,
    // this event must be posted to the main dispatch queue as well to ensure
    // correct ordering.
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC),
        dispatch_get_main_queue(), ^{
          ui::ElementTrackerMac::GetInstance()->NotifyMenuDoneShowing(menu);
        });
  }
}

}  // namespace ui
