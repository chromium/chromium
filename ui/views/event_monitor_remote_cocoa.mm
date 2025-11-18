// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_monitor_remote_cocoa.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"

namespace views {

EventMonitorRemoteCocoa::EventMonitorRemoteCocoa(
    ui::EventObserver* event_observer,
    gfx::NativeWindow target_native_window,
    std::set<ui::EventType> types)
    : event_observer_(event_observer), types_(std::move(types)) {
  CHECK(event_observer_);

  auto* host = NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      target_native_window);
  CHECK(host) << "No NativeWidgetMacNSWindowHost for the given window.";

  event_monitor_ = host->AddEventMonitor(this);
  DCHECK(event_monitor_);
}

EventMonitorRemoteCocoa::~EventMonitorRemoteCocoa() {
  if (event_monitor_) {
    event_monitor_->RemoveMonitor();
  }
}

void EventMonitorRemoteCocoa::OnEvent(ui::Event* event,
                                      bool target_is_this_window,
                                      bool* was_handled) {
  if (!event || *was_handled) {
    return;
  }

  if (target_is_this_window && base::Contains(types_, event->type())) {
    event_observer_->OnEvent(*event);
  }
}

gfx::Point EventMonitorRemoteCocoa::GetLastMouseLocation() const {
  return display::Screen::GetScreen()->GetCursorScreenPoint();
}

}  
