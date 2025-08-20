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
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/event_monitor_remote_cocoa.h"

namespace views {

namespace {
bool g_use_remote_cocoa_for_testing = false;
}

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
  CHECK(target_window)
      << "Use CreateApplicationMonitor to observe events to all windows";

  // For Progressive Web App (PWA) windows, we can't use an in-process NSEvent
  // monitors, as these windows exist in a different process. So depending on
  // if the target window is local or remote we use a different EventMonitor
  // implementation.
  auto* host =
      views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(target_window);
  if (host && (host->application_host() || g_use_remote_cocoa_for_testing)) {
    return std::make_unique<EventMonitorRemoteCocoa>(event_observer,
                                                     target_window, types);
  } else {
    return std::make_unique<EventMonitorMac>(event_observer, target_window,
                                             types);
  }
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

EventMonitorMac::~EventMonitorMac() {
  [NSEvent removeMonitor:objc_storage_->monitor];
}

gfx::Point EventMonitorMac::GetLastMouseLocation() {
  return display::Screen::Get()->GetCursorScreenPoint();
}

// static
base::AutoReset<bool> EventMonitorMac::UseRemoteCocoaForTesting() {
  base::AutoReset<bool> result(&g_use_remote_cocoa_for_testing, true);
  return result;
}

}  // namespace views
