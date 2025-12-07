// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_MONITOR_REMOTE_COCOA_H_
#define UI_VIEWS_EVENT_MONITOR_REMOTE_COCOA_H_

#include <memory>
#include <set>

#include "ui/gfx/native_ui_types.h"
#include "ui/views/cocoa/native_widget_mac_event_monitor.h"
#include "ui/views/event_monitor.h"

namespace views {

// EventMonitor implementation that unlike EventMonitorMac is able to operate
// on windows in other processes, such as those for Progressive Web Apps. This
// wraps around the event monitoring support in NativeWidgetMacNSWindowHost.
// This class lives in the browser process.
class EventMonitorRemoteCocoa
    : public EventMonitor,
      public views::NativeWidgetMacEventMonitor::Client {
 public:
  EventMonitorRemoteCocoa(ui::EventObserver* event_observer,
                          gfx::NativeWindow target_window,
                          const std::set<ui::EventType>& types);

  EventMonitorRemoteCocoa(const EventMonitorRemoteCocoa&) = delete;
  EventMonitorRemoteCocoa& operator=(const EventMonitorRemoteCocoa&) = delete;

  ~EventMonitorRemoteCocoa() override;

  // EventMonitor:
  gfx::Point GetLastMouseLocation() override;

 private:
  // views::NativeWidgetMacEventMonitor::Client
  void NativeWidgetMacEventMonitorOnEvent(ui::Event* event,
                                          bool target_is_this_window,
                                          bool* event_handled) override;

  const std::set<ui::EventType> types_;
  raw_ptr<ui::EventObserver> event_observer_;
  std::unique_ptr<views::NativeWidgetMacEventMonitor> event_monitor_;
};

}  // namespace views

#endif  // UI_VIEWS_EVENT_MONITOR_REMOTE_COCOA_H_
