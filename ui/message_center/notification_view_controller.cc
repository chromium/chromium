// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_view_controller.h"

#include "ui/message_center/views/message_view.h"

namespace message_center {

NotificationViewController::NotificationViewController() {
  observer_.Observe(MessageCenter::Get());
}

NotificationViewController::~NotificationViewController() {
  observer_.Reset();
}

void NotificationViewController::OnNotificationAdded(
    const std::string& notification_id) {
  Notification* notification =
      MessageCenter::Get()->FindNotificationById(notification_id);

  // We only need to process notifications that are children of an
  // existing group. So do nothing otherwise.
  if (!notification->group_child())
    return;

  Notification* parent_notification =
      MessageCenter::Get()->FindOldestNotificationByNotiferId(
          notification->notifier_id());

  child_parent_map_[notification_id] = parent_notification->id();

  MessageView* parent_view =
      GetMessageViewForNotificationId(parent_notification->id());
  if (parent_view)
    parent_view->AddGroupNotification(*notification);
}

void NotificationViewController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  auto it = child_parent_map_.find(notification_id);
  if (it != child_parent_map_.end()) {
    MessageView* parent_view =
        GetMessageViewForNotificationId(child_parent_map_[notification_id]);

    child_parent_map_.erase(it);

    if (parent_view)
      parent_view->RemoveGroupNotification(notification_id);
  }
}

}  // namespace message_center
