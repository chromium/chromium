// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/event_monitor_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/apple/owned_objc.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
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
  return std::make_unique<EventMonitorMac>(event_observer, gfx::NativeWindow(),
                                           types);
}

// static
std::unique_ptr<EventMonitor> EventMonitor::CreateWindowMonitor(
    ui::EventObserver* event_observer,
    gfx::NativeWindow target_window,
    const std::set<ui::EventType>& types) {
  return std::make_unique<EventMonitorMac>(event_observer, target_window,
                                           types);
}

struct EventMonitorMac::ObjCStorage {
  id __strong monitor = nil;
};

EventMonitorMac::EventMonitorMac(ui::EventObserver* event_observer,
                                 gfx::NativeWindow target_native_window,
                                 const std::set<ui::EventType>& types)
    : types_(types),
      event_observer_(event_observer),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  DCHECK(event_observer);
  NSWindow* target_window = target_native_window.GetNativeNSWindow();

  // For Progressive Web App (PWA) windows, we use a different event monitoring
  // path. When the target window is inside PWA, we register
  // `NativeWidgetMacEventMonitor` for remote cocoa. These events are processed
  // through the NativeWidgetMacEventMonitorOnEvent() method defined below,
  // bypassing the NSEvent block-based monitoring approach that follows.
  auto* host = views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      target_native_window);
  if (host && host->application_host()) {
    event_monitor_ = host->AddEventMonitor(this);
    return;
  }

  // Capture a WeakPtr. This allows the block to detect another event monitor
  // for the same event deleting |this|.
  base::WeakPtr<EventMonitorMac> weak_ptr = factory_.GetWeakPtr();

  auto block = ^NSEvent*(NSEvent* event) {
    if (!weak_ptr) {
      return event;
    }

    if (!target_window || [event window] == target_window) {
      std::unique_ptr<ui::Event> ui_event =
          ui::EventFromNative(base::apple::OwnedNSEvent(event));
      if (ui_event && types_.find(ui_event->type()) != types_.end()) {
        event_observer->OnEvent(*ui_event);
      }
    }
    return event;
  };

  objc_storage_->monitor =
      [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskAny
                                            handler:block];
}

void EventMonitorMac::NativeWidgetMacEventMonitorOnEvent(ui::Event* ui_event,
                                                         bool* was_handled) {
  if (*was_handled || !ui_event) {
    return;
  }

  if (types_.find(ui_event->type()) != types_.end()) {
    event_observer_->OnEvent(*ui_event);
  }
}

EventMonitorMac::~EventMonitorMac() {
  [NSEvent removeMonitor:objc_storage_->monitor];
}

gfx::Point EventMonitorMac::GetLastMouseLocation() {
  return display::Screen::GetScreen()->GetCursorScreenPoint();
}

}  // namespace views
