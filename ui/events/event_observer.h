// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_OBSERVER_H_
#define UI_EVENTS_EVENT_OBSERVER_H_

#include "base/macros.h"
#include "ui/events/events_export.h"

namespace ui {

class Event;

// EventObservers are notified of events but are unable to modify the events or
// mark them as handled before they are dispatched to EventHandlers.
//
// Window service clients may use this interface for observation of events that
// target the window manager or other clients. Clients should limit the types
// and duration of observation, as there is a system-wide perf/battery penalty,
// especially for frequently occurring events, like mouse moves. Events with
// targets outside of the client's scope will have a null target.
class EVENTS_EXPORT EventObserver {
 public:
  // Called for all events matching the requested event types.
  // The root location of located events is always in screen coordinates.
  virtual void OnEvent(const Event& event) = 0;

 protected:
  virtual ~EventObserver() {}
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_OBSERVER_H_
