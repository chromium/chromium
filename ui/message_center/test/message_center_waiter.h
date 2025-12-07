// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_TEST_MESSAGE_CENTER_WAITER_H_
#define UI_MESSAGE_CENTER_TEST_MESSAGE_CENTER_WAITER_H_

#include <string>

#include "base/scoped_observation.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace message_center {

// A helper class that waits for a notification to be added to, updated in, or
// removed from the message center.
//
// This waiter uses `base::test::RunUntil` instead of a manual `base::RunLoop`.
// This is the modern and preferred approach for waiting on asynchronous
// conditions in tests.
//
// `RunUntil` is superior for several reasons. First, it has a built-in
// timeout. If the condition (the lambda returning true) is not met within a
// reasonable time, it will cause a `CHECK` failure. This prevents the test
// from hanging indefinitely and provides a clear failure point.
//
// Second, it is designed to work predictably with mock time environments
// (`TaskEnvironment::TimeSource::MOCK_TIME`). In contrast, a manual `RunLoop`
// can sometimes cause unexpected side effects, such as prematurely flushing
// delayed tasks, leading to tests that pass for the wrong reasons (false
// positives).
//
// Finally, the waiting condition is expressed cleanly and directly within the
// lambda. This avoids the boilerplate of managing a `RunLoop` pointer, a
// `QuitClosure`, and the associated `Run()` and `Quit()` calls.
class MessageCenterWaiter : public message_center::MessageCenterObserver {
 public:
  explicit MessageCenterWaiter(const std::string& notification_id);
  MessageCenterWaiter(const MessageCenterWaiter&) = delete;
  MessageCenterWaiter& operator=(const MessageCenterWaiter&) = delete;
  ~MessageCenterWaiter() override;

  // Waits for the notification to be added for the first time.
  void WaitUntilAdded();

  // Waits for the notification to be updated.
  void WaitUntilUpdated();

  // Waits for the notification to be removed.
  void WaitUntilRemoved();

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationUpdated(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

 private:
  const std::string notification_id_;
  bool added_ = false;
  bool updated_ = false;
  bool removed_ = false;
  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      observation_{this};
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_TEST_MESSAGE_CENTER_WAITER_H_
