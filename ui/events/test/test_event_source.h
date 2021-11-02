// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_EVENT_SOURCE_H_
#define UI_EVENTS_TEST_TEST_EVENT_SOURCE_H_

#include "base/macros.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_source.h"

namespace ui {
namespace test {

// Trivial testing EventSource that does nothing but send and count events.
// If no sink is provided, it acts as its own sink, which also counts events.
class TestEventSource : public EventSource, public EventSink {
 public:
  explicit TestEventSource(EventSink* sink) : sink_(sink) {}
  TestEventSource() : sink_(this) {}
  ~TestEventSource() override = default;

  EventDispatchDetails Send(Event* event);

  int events_sent() const { return events_sent_; }
  int events_sunk() const { return events_sunk_; }

  // EventSource override:
  EventSink* GetEventSink() override;

  // EventSink override:
  EventDispatchDetails OnEventFromSource(Event* event) override;

 private:
  EventSink* sink_;
  int events_sent_ = 0;
  int events_sunk_ = 0;
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_TEST_EVENT_SOURCE_H_
