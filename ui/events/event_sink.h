// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_SINK_H_
#define UI_EVENTS_EVENT_SINK_H_

#include "ui/events/event_dispatcher.h"

namespace ui {

class Event;

// EventSink receives events from an EventSource.
class EVENTS_EXPORT EventSink {
 public:
  virtual ~EventSink() {}

  // Receives events from EventSource.
  [[nodiscard]] virtual EventDispatchDetails OnEventFromSource(
      Event* event) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_SINK_H_
