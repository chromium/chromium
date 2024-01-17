// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_MONITOR_AURA_H_
#define UI_VIEWS_EVENT_MONITOR_AURA_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "ui/views/event_monitor.h"

namespace ui {
class EventTarget;
}

namespace views {

// Observes events by installing a pre-target handler on the ui::EventTarget.
class EventMonitorAura : public EventMonitor {
 public:
  EventMonitorAura(ui::EventObserver* event_observer,
                   ui::EventTarget* event_target,
                   const std::set<ui::EventType>& types);

  EventMonitorAura(const EventMonitorAura&) = delete;
  EventMonitorAura& operator=(const EventMonitorAura&) = delete;

  ~EventMonitorAura() override;

  // EventMonitor:
  gfx::Point GetLastMouseLocation() override;

 protected:
  // Removes the pre-target handler. Called by window monitors on window close.
  void TearDown();

 private:
  raw_ptr<ui::EventObserver> event_observer_ = nullptr;  // Owned by our owner.
};

}  // namespace views

#endif  // UI_VIEWS_EVENT_MONITOR_AURA_H_
