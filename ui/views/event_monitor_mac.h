// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_MONITOR_MAC_H_
#define UI_VIEWS_EVENT_MONITOR_MAC_H_

#include <set>

#include "ui/base/cocoa/weak_ptr_nsobject.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/event_monitor.h"

namespace views {

class EventMonitorMac : public EventMonitor {
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
  id monitor_;
  ui::WeakPtrNSObjectFactory<EventMonitorMac> factory_;
  const std::set<ui::EventType> types_;
};

}  // namespace views

#endif  // UI_VIEWS_EVENT_MONITOR_MAC_H_
