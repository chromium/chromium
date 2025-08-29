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

// A helper class that waits for a notification to be added to the message
// center.
class MessageCenterWaiter : public message_center::MessageCenterObserver {
 public:
  explicit MessageCenterWaiter(const std::string& notification_id);
  MessageCenterWaiter(const MessageCenterWaiter&) = delete;
  MessageCenterWaiter& operator=(const MessageCenterWaiter&) = delete;
  ~MessageCenterWaiter() override;

  // Waits for the notification to be added.
  void Wait();

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;

 private:
  const std::string notification_id_;
  bool notification_added_ = false;
  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      observation_{this};
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_TEST_MESSAGE_CENTER_WAITER_H_
