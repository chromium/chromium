// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_MONITOR_H_
#define UI_VIEWS_EVENT_MONITOR_H_

#include <memory>
#include <set>

#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace views {

// RAII-style class that forwards events matching the specified |types| to
// |event_observer| before they are dispatched to the intended target.
// EventObservers cannot modify events nor alter dispatch.
class VIEWS_EXPORT EventMonitor {
 public:
  virtual ~EventMonitor() = default;

  // Create an instance for monitoring application events. This includes all
  // events on ChromeOS, but only events targeting Chrome on desktop platforms.
  // |context| is used to determine where to observe events from.
  // |context| may be destroyed before the EventMonitor.
  static std::unique_ptr<EventMonitor> CreateApplicationMonitor(
      ui::EventObserver* event_observer,
      gfx::NativeWindow context,
      const std::set<ui::EventType>& types);

  // Create an instance for monitoring events on a specific window.
  // The EventMonitor instance must be destroyed before |target_window|.
  static std::unique_ptr<EventMonitor> CreateWindowMonitor(
      ui::EventObserver* event_observer,
      gfx::NativeWindow target_window,
      const std::set<ui::EventType>& types);

  // Returns the last recorded mouse event location in screen coordinates.
  virtual gfx::Point GetLastMouseLocation() = 0;
};

}  // namespace views

#endif  // UI_VIEWS_EVENT_MONITOR_H_
