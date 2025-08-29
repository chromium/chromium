// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/test/message_center_waiter.h"

#include "base/check.h"
#include "base/test/run_until.h"

namespace message_center {

MessageCenterWaiter::MessageCenterWaiter(const std::string& notification_id)
    : notification_id_(notification_id) {
  observation_.Observe(message_center::MessageCenter::Get());
}

MessageCenterWaiter::~MessageCenterWaiter() = default;

void MessageCenterWaiter::Wait() {
  if (notification_added_) {
    return;
  }

  // This waiter uses `base::test::RunUntil` instead of a manual
  // `base::RunLoop`. This is the modern and preferred approach for waiting on
  // asynchronous conditions in tests.
  //
  // `RunUntil` is superior for several reasons. First, it has a built-in
  // timeout. If the condition (the lambda returning true) is not met within a
  // reasonable time, it will cause a `CHECK` failure. This prevents the test
  // from hanging indefinitely and provides a clear failure point.
  //
  // Second, it is designed to work predictably with mock time environments
  // (`TaskEnvironment::TimeSource::MOCK_TIME`). In contrast, a manual
  // `RunLoop` can sometimes cause unexpected side effects, such as prematurely
  // flushing delayed tasks, leading to tests that pass for the wrong reasons
  // (false positives).
  //
  // Finally, the waiting condition is expressed cleanly and directly within the
  // lambda. This avoids the boilerplate of managing a `RunLoop` pointer, a
  // `QuitClosure`, and the associated `Run()` and `Quit()` calls.
  CHECK(base::test::RunUntil([&]() { return notification_added_; }));
}

void MessageCenterWaiter::OnNotificationAdded(
    const std::string& notification_id) {
  if (notification_id == notification_id_) {
    notification_added_ = true;
  }
}

}  // namespace message_center
