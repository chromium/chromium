// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_SCOPED_EVENT_WAITER_H_
#define UI_EVENTS_TEST_SCOPED_EVENT_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "ui/events/event_handler.h"
#include "ui/events/types/event_type.h"

namespace ui {
class EventTarget;

namespace test {

// A test helper class that waits until a specified event target receives the
// target event.
class ScopedEventWaiter : public EventHandler {
 public:
  ScopedEventWaiter(EventTarget* target, EventType target_event_type);
  ScopedEventWaiter(const ScopedEventWaiter&) = delete;
  ScopedEventWaiter& operator=(const ScopedEventWaiter&) = delete;
  ~ScopedEventWaiter() override;

  // Waits until `target_` receives the event of `target_event_type_`.
  void Wait();

 private:
  // EventHandler:
  void OnEvent(Event* event) override;

  // The observed event target.
  const raw_ptr<EventTarget> target_;

  // The type of the event being waited for.
  const EventType target_event_type_;

  base::RunLoop run_loop_;
};

}  // namespace test

}  // namespace ui

#endif  // UI_EVENTS_TEST_SCOPED_EVENT_WAITER_H_
