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

void MessageCenterWaiter::WaitUntilAdded() {
  if (added_) {
    return;
  }
  CHECK(base::test::RunUntil([&]() { return added_; }));
}

void MessageCenterWaiter::WaitUntilUpdated() {
  if (updated_) {
    return;
  }
  CHECK(base::test::RunUntil([&]() { return updated_; }));
}

void MessageCenterWaiter::WaitUntilRemoved() {
  if (removed_) {
    return;
  }
  CHECK(base::test::RunUntil([&]() { return removed_; }));
}

void MessageCenterWaiter::OnNotificationAdded(
    const std::string& notification_id) {
  if (notification_id == notification_id_) {
    added_ = true;
  }
}

// `MessageCenter::AddNotification` triggers `OnNotificationUpdated` if a
// notification with the same ID already exists. This handler ensures that the
// waiter correctly captures this case as the notification being "present". A
// concrete example is `LowDiskNotificationTest.HighLevelReplacesMedium`, where
// a second call to show a notification replaces the first one.
void MessageCenterWaiter::OnNotificationUpdated(
    const std::string& notification_id) {
  if (notification_id == notification_id_) {
    updated_ = true;
  }
}

void MessageCenterWaiter::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  if (notification_id == notification_id_) {
    removed_ = true;
  }
}

}  // namespace message_center
