// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_MONITOR_MAC_H_
#define UI_VIEWS_EVENT_MONITOR_MAC_H_

#include <memory>
#include <set>

#include "base/auto_reset.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/event_monitor.h"
#include "ui/views/views_export.h"

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

  // Causes EventMonitorMac to use the remote cocoa implementation rather than
  // its normal local event monitoring implementation even if the target window
  // isn't hosted out of process.
  VIEWS_EXPORT [[nodiscard]] static base::AutoReset<bool>
  UseRemoteCocoaForTesting();

 private:
  const std::set<ui::EventType> types_;
  raw_ptr<ui::EventObserver> event_observer_;

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;

  base::WeakPtrFactory<EventMonitorMac> factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_EVENT_MONITOR_MAC_H_
