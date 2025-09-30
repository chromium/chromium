// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_monitor_remote_cocoa.h"

#include <memory>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/events/event_utils.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"

namespace views {

EventMonitorRemoteCocoa::EventMonitorRemoteCocoa(
    ui::EventObserver* event_observer,
    gfx::NativeWindow target_native_window,
    const std::set<ui::EventType>& types)
    : types_(types), event_observer_(event_observer) {
  CHECK(event_observer);

  auto* host = views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      target_native_window);
  CHECK(host);
  event_monitor_ = host->AddEventMonitor(this);
}

void EventMonitorRemoteCocoa::NativeWidgetMacEventMonitorOnEvent(
    ui::Event* ui_event,
    bool target_is_this_window,
    bool* was_handled) {
  if (*was_handled || !ui_event) {
    return;
  }

  if (target_is_this_window && base::Contains(types_, ui_event->type())) {
    event_observer_->OnEvent(*ui_event);
  }
}

EventMonitorRemoteCocoa::~EventMonitorRemoteCocoa() = default;

gfx::Point EventMonitorRemoteCocoa::GetLastMouseLocation() {
  return display::Screen::Get()->GetCursorScreenPoint();
}

}  // namespace views
