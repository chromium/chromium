// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_MONITOR_MAC_H_
#define UI_VIEWS_EVENT_MONITOR_MAC_H_

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/event_monitor.h"

namespace views {

class EventMonitorMac : public EventMonitor,
                        public views::NativeWidgetMacEventMonitor::Client {
 public:
  EventMonitorMac(ui::EventObserver* event_observer,
                  gfx::NativeWindow target_window,
                  const std::set<ui::EventType>& types);

  EventMonitorMac(const EventMonitorMac&) = delete;
  EventMonitorMac& operator=(const EventMonitorMac&) = delete;

  ~EventMonitorMac() override;

  // EventMonitor:
  gfx::Point GetLastMouseLocation() override;

 private:
  // views::NativeWidgetMacEventMonitor::Client
  void NativeWidgetMacEventMonitorOnEvent(ui::Event* event,
                                          bool* event_handled) override;

  const std::set<ui::EventType> types_;
  raw_ptr<ui::EventObserver> event_observer_;

  // For remote cocoa use.
  std::unique_ptr<views::NativeWidgetMacEventMonitor> event_monitor_;

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;

  base::WeakPtrFactory<EventMonitorMac> factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_EVENT_MONITOR_MAC_H_
