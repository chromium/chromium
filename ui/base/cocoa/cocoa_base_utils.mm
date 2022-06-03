// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/cocoa_base_utils.h"

#include "ui/events/cocoa/cocoa_event_utils.h"

namespace ui {

WindowOpenDisposition WindowOpenDispositionFromNSEvent(NSEvent* event) {
  NSUInteger modifiers = [event modifierFlags];
  return WindowOpenDispositionFromNSEventWithFlags(event, modifiers);
}

WindowOpenDisposition WindowOpenDispositionFromNSEventWithFlags(
    NSEvent* event, NSUInteger modifiers) {
  int event_flags = EventFlagsFromNSEventWithModifiers(event, modifiers);
  return DispositionFromEventFlags(event_flags);
}

NSPoint ConvertPointFromWindowToScreen(NSWindow* window, NSPoint point) {
  NSRect point_rect = NSMakeRect(point.x, point.y, 0, 0);
  return [window convertRectToScreen:point_rect].origin;
}

NSPoint ConvertPointFromScreenToWindow(NSWindow* window, NSPoint point) {
  NSRect point_rect = NSMakeRect(point.x, point.y, 0, 0);
  return [window convertRectFromScreen:point_rect].origin;
}

}  // namespace ui
