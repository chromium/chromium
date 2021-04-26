// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_MONITOR_AURA_H_
#define UI_VIEWS_EVENT_MONITOR_AURA_H_

#include <set>

#include "base/macros.h"
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
  ~EventMonitorAura() override;

  // EventMonitor:
  gfx::Point GetLastMouseLocation() override;

 protected:
  // Removes the pre-target handler. Called by window monitors on window close.
  void TearDown();

 private:
  ui::EventObserver* event_observer_;  // Weak. Owned by our owner.
  ui::EventTarget* event_target_;      // Weak.

  DISALLOW_COPY_AND_ASSIGN(EventMonitorAura);
};

}  // namespace views

#endif  // UI_VIEWS_EVENT_MONITOR_AURA_H_
