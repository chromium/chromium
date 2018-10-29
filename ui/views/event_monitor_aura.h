// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_MONITOR_AURA_H_
#define UI_VIEWS_EVENT_MONITOR_AURA_H_

#include "base/macros.h"
#include "ui/views/event_monitor.h"

namespace aura {
class Env;
}

namespace ui {
class EventTarget;
}

namespace views {

class EventMonitorAura : public EventMonitor {
 public:
  EventMonitorAura(aura::Env* env,
                   ui::EventObserver* event_observer,
                   ui::EventTarget* event_target,
                   const std::set<ui::EventType>& types);
  ~EventMonitorAura() override;

  // EventMonitor:
  gfx::Point GetLastMouseLocation() override;

 private:
  aura::Env* env_;                     // Weak.
  ui::EventObserver* event_observer_;  // Weak. Owned by our owner.
  ui::EventTarget* event_target_;      // Weak.

  DISALLOW_COPY_AND_ASSIGN(EventMonitorAura);
};

}  // namespace views

#endif  // UI_VIEWS_EVENT_MONITOR_AURA_H_
