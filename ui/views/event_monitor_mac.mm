// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/event_monitor_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/events/event_utils.h"

namespace views {

// static
std::unique_ptr<EventMonitor> EventMonitor::CreateApplicationMonitor(
    ui::EventObserver* event_observer,
    gfx::NativeWindow context,
    const std::set<ui::EventType>& types) {
  // |context| is not needed on Mac.
  return std::make_unique<EventMonitorMac>(event_observer, nullptr, types);
}

// static
std::unique_ptr<EventMonitor> EventMonitor::CreateWindowMonitor(
    ui::EventObserver* event_observer,
    gfx::NativeWindow target_window,
    const std::set<ui::EventType>& types) {
  return std::make_unique<EventMonitorMac>(event_observer, target_window,
                                           types);
}

EventMonitorMac::EventMonitorMac(ui::EventObserver* event_observer,
                                 gfx::NativeWindow target_native_window,
                                 const std::set<ui::EventType>& types)
    : factory_(this), types_(types) {
  DCHECK(event_observer);
  NSWindow* target_window = target_native_window.GetNativeNSWindow();

  // Capture a WeakPtr via NSObject. This allows the block to detect another
  // event monitor for the same event deleting |this|.
  WeakPtrNSObject* handle = factory_.handle();

  auto block = ^NSEvent*(NSEvent* event) {
    if (!ui::WeakPtrNSObjectFactory<EventMonitorMac>::Get(handle))
      return event;

    if (!target_window || [event window] == target_window) {
      std::unique_ptr<ui::Event> ui_event = ui::EventFromNative(event);
      if (ui_event && types_.find(ui_event->type()) != types_.end())
        event_observer->OnEvent(*ui_event);
    }
    return event;
  };

  monitor_ = [NSEvent addLocalMonitorForEventsMatchingMask:NSAnyEventMask
                                                   handler:block];
}

EventMonitorMac::~EventMonitorMac() {
  [NSEvent removeMonitor:monitor_];
}

gfx::Point EventMonitorMac::GetLastMouseLocation() {
  return display::Screen::GetScreen()->GetCursorScreenPoint();
}

}  // namespace views
