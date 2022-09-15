// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/test_event_targeter.h"

#include "ui/events/test/test_event_target.h"

namespace ui {
namespace test {

TestEventTargeter::TestEventTargeter(TestEventTarget* initial_target,
                                     bool should_bubble)
    : target_(initial_target), should_bubble_(should_bubble) {
}

TestEventTargeter::~TestEventTargeter() {
}

void TestEventTargeter::set_target(TestEventTarget* target) {
  target_ = target;
}

EventTarget* TestEventTargeter::FindTargetForEvent(EventTarget* root,
                                                   Event* event) {
  return target_;
}

EventTarget* TestEventTargeter::FindNextBestTarget(EventTarget* previous_target,
                                                   Event* event) {
  if (!should_bubble_)
    return nullptr;
  return previous_target->GetParentTarget();
}

}  // namespace test
}  // namespace ui
