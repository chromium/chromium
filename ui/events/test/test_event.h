// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_EVENT_H_
#define UI_EVENTS_TEST_TEST_EVENT_H_

#include <memory>

#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"

namespace ui::test {

class TestEvent : public Event {
 public:
  explicit TestEvent(EventType type)
      : Event(type, base::TimeTicks(), EF_NONE) {}

  TestEvent() : TestEvent(EventType::kUnknown) {}

  ~TestEvent() override = default;

  // Event:
  std::unique_ptr<Event> Clone() const override;
};

}  // namespace ui::test

#endif  // UI_EVENTS_TEST_TEST_EVENT_H_
