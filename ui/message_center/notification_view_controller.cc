// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_view_controller.h"

#include "ash/constants/ash_features.h"
#include "base/time/time.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"

namespace message_center {

NotificationViewController::NotificationViewController() {
  observer_.Observe(MessageCenter::Get());
}

NotificationViewController::~NotificationViewController() {
  observer_.Reset();
}

void NotificationViewController::PopulateGroupParent(
    const std::string& notification_id) {
  DCHECK(MessageCenter::Get()
             ->FindNotificationById(notification_id)
             ->group_parent());
  MessageView* parent_view = GetMessageViewForNotificationId(notification_id);

  for (auto* notification : MessageCenter::Get()->GetNotifications()) {
    if (notification->notifier_id() == parent_view->notifier_id() &&
        notification->id() != parent_view->notification_id()) {
      child_parent_map_[notification->id()] = parent_view->notification_id();
      parent_view->AddGroupNotification(*notification, /*newest_first=*/true);
    }
  }
}

void NotificationViewController::SetupParentNotification(
    std::string* parent_id) {
  Notification* parent_notification =
      MessageCenter::Get()->FindNotificationById(*parent_id);
  std::unique_ptr<Notification> notification_copy =
      CreateCopyForParentNotification(*parent_notification);

  std::string new_parent_id = notification_copy->id();
  std::string old_parent_id = *parent_id;
  *parent_id = new_parent_id;

  parent_grouped_notification_id_set_.insert(new_parent_id);

  Notification* new_parent_notification = notification_copy.get();
  MessageCenter::Get()->AddNotification(std::move(notification_copy));

  ConvertNotificationViewToGroupedNotificationView(
      /*ungrouped_notification_id=*/old_parent_id,
      /*new_grouped_notification_id=*/new_parent_id);
  UpdateChildParentMap(/*new_parent_id=*/new_parent_id,
                       /*old_parent_id=*/old_parent_id);

  // Add the old parent notification as a group child to the
  // newly created parent notification which will act as a
  // container for this group as long as it exists.
  new_parent_notification->SetGroupParent();
  parent_notification->SetGroupChild();

  GetMessageViewForNotificationId(new_parent_id)
      ->AddGroupNotification(*parent_notification, /*newest_first=*/false);
}

std::unique_ptr<Notification>
NotificationViewController::CreateCopyForParentNotification(
    const Notification& parent_notification) {
  // Create a copy with a timestamp that is older than the copied notification.
  // We need to set an older timestamp so that this notification will become
  // the parent notification for it's notifier_id.
  auto child_copy = std::make_unique<Notification>(
      NotificationType::NOTIFICATION_TYPE_SIMPLE,
      parent_notification.id() + kIdSuffixForGroupContainerNotification,
      parent_notification.title(), parent_notification.message(), gfx::Image(),
      std::u16string(), parent_notification.origin_url(),
      parent_notification.notifier_id(), RichNotificationData(),
      /*delegate=*/nullptr);
  child_copy->set_timestamp(parent_notification.timestamp() -
                            base::TimeDelta::FromMilliseconds(1));
  child_copy->SetGroupChild();

  return child_copy;
}

void NotificationViewController::UpdateChildParentMap(
    const std::string& new_parent_id,
    const std::string& old_parent_id) {
  // Remove entry with `new_parent_id` as a child id and replace with
  // `old_parent_id` as a child id.
  DCHECK(child_parent_map_.find(new_parent_id) != child_parent_map_.end());
  child_parent_map_.erase(child_parent_map_.find(new_parent_id));
  child_parent_map_[old_parent_id] = new_parent_id;

  // Replace all occurrences of `old_parent_id` with `new_parent_id`.
  std::vector<std::string> to_be_updated;
  for (auto& child : child_parent_map_) {
    if (child.second == old_parent_id)
      to_be_updated.push_back(child.first);
  }
  for (auto id : to_be_updated) {
    child_parent_map_.erase(child_parent_map_.find(id));
    child_parent_map_[id] = new_parent_id;
  }
}

void NotificationViewController::OnNotificationAdded(
    const std::string& notification_id) {
  Notification* notification =
      MessageCenter::Get()->FindNotificationById(notification_id);
  // We only need to process notifications that are children of an
  // existing group. So do nothing otherwise.
  if (!notification)
    return;

  if (!notification->group_child())
    return;

  Notification* parent_notification =
      MessageCenter::Get()->FindOldestNotificationByNotiferId(
          notification->notifier_id());
  std::string parent_id = parent_notification->id();

  // If we are creating a new notification group for this `notifier_id`,
  // we must create a copy of the designated parent notification and
  // use it to set up a container notification which will hold all
  // notifications for this group.
  if (!parent_grouped_notification_id_set_.count(parent_id))
    SetupParentNotification(&parent_id);

  child_parent_map_[notification_id] = parent_id;

  MessageView* parent_view = GetMessageViewForNotificationId(parent_id);
  if (parent_view)
    parent_view->AddGroupNotification(*notification, /*newest_first=*/false);
  else
    MessageCenter::Get()->ResetSinglePopup(parent_id);
}

void NotificationViewController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  auto child_it = child_parent_map_.find(notification_id);
  if (child_it != child_parent_map_.end()) {
    MessageView* parent_view =
        GetMessageViewForNotificationId(child_parent_map_[notification_id]);

    child_parent_map_.erase(child_it);

    if (parent_view)
      parent_view->RemoveGroupNotification(notification_id);
  }
  auto parent = parent_grouped_notification_id_set_.find(notification_id);
  if (parent != parent_grouped_notification_id_set_.end()) {
    std::vector<std::string> to_be_deleted;
    for (auto& child : child_parent_map_) {
      if (child.second == notification_id)
        to_be_deleted.push_back(child.first);
    }

    for (auto id : to_be_deleted)
      MessageCenter::Get()->RemoveNotification(id, by_user);
  }
}

}  // namespace message_center
