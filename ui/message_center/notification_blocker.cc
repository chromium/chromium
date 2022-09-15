// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_blocker.h"

#include "base/observer_list.h"
#include "ui/message_center/message_center.h"

namespace message_center {

NotificationBlocker::NotificationBlocker(MessageCenter* message_center)
    : message_center_(message_center) {
  if (message_center_)
    message_center_->AddNotificationBlocker(this);
}

NotificationBlocker::~NotificationBlocker() {
  if (message_center_)
    message_center_->RemoveNotificationBlocker(this);
}

void NotificationBlocker::AddObserver(NotificationBlocker::Observer* observer) {
  observers_.AddObserver(observer);
}

void NotificationBlocker::RemoveObserver(
    NotificationBlocker::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool NotificationBlocker::ShouldShowNotification(
    const Notification& notification) const {
  return true;
}

void NotificationBlocker::NotifyBlockingStateChanged() {
  for (auto& observer : observers_)
    observer.OnBlockingStateChanged(this);
}

}  // namespace message_center
