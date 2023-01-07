// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_EVENT_TARGETER_H_
#define UI_EVENTS_TEST_TEST_EVENT_TARGETER_H_

#include "base/memory/raw_ptr.h"
#include "ui/events/event_targeter.h"

namespace ui {
namespace test {

class TestEventTarget;

// An EventTargeter which is used to allow a bubbling behaviour in event
// dispatch: if an event is not handled after being dispatched to its
// |initial_target|, the event is dispatched to the next-best target as
// specified by FindNextBestTarget().
// Bubbling behaviour is controlled by |should_bubble| at creation time.
class TestEventTargeter : public EventTargeter {
 public:
  TestEventTargeter(TestEventTarget* initial_target, bool should_bubble);

  TestEventTargeter(const TestEventTargeter&) = delete;
  TestEventTargeter& operator=(const TestEventTargeter&) = delete;

  ~TestEventTargeter() override;

  void set_target(TestEventTarget* target);

 private:
  // EventTargeter:
  EventTarget* FindTargetForEvent(EventTarget* root, Event* event) override;
  EventTarget* FindNextBestTarget(EventTarget* previous_target,
                                  Event* event) override;

  raw_ptr<TestEventTarget> target_;
  bool should_bubble_;
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_TEST_EVENT_TARGETER_H_
