// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/test_event_source.h"

#include "ui/events/event.h"

namespace ui {
namespace test {

EventDispatchDetails TestEventSource::Send(Event* event) {
  CHECK(event);
  ++events_sent_;
  return SendEventToSink(event);
}

EventSink* TestEventSource::GetEventSink() {
  return sink_;
}

EventDispatchDetails TestEventSource::OnEventFromSource(Event* event) {
  CHECK(event);
  ++events_sunk_;
  return EventDispatchDetails();
}

}  // namespace test
}  // namespace ui
