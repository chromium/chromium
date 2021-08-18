// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_view_controller.h"
#include <iterator>
#include <memory>

#include "ash/constants/ash_features.h"
#include "base/debug/stack_trace.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"

namespace message_center {

namespace {

class GroupedNotificationList {
 public:
  GroupedNotificationList() = default;
  GroupedNotificationList(const GroupedNotificationList& other) = delete;
  GroupedNotificationList& operator=(const GroupedNotificationList& other) =
      delete;
  ~GroupedNotificationList() = default;

  void AddGroupedNotification(const std::string& notification_id,
                              const std::string& parent_id) {
    if (notifications_in_parent_map_.find(parent_id) ==
        notifications_in_parent_map_.end()) {
      notifications_in_parent_map_[parent_id] = {};
    }

    child_parent_map_[notification_id] = parent_id;

    if (notification_id != parent_id)
      notifications_in_parent_map_[parent_id].insert(notification_id);
  }

  // Remove a single child notification from a grouped notification.
  void RemoveGroupedChildNotification(const std::string& notification_id) {
    std::string& parent_id = child_parent_map_[notification_id];
    notifications_in_parent_map_[parent_id].erase(notification_id);
    child_parent_map_.erase(notification_id);
  }

  // Clear the entire grouped notification with `parent_id`
  void ClearGroupedNotification(const std::string& parent_id) {
    notifications_in_parent_map_.erase(parent_id);
    std::vector<std::string> to_be_deleted;
    for (const auto& it : child_parent_map_) {
      if (it.second == parent_id)
        to_be_deleted.push_back(it.first);
    }
    for (const auto& child : to_be_deleted)
      child_parent_map_.erase(child);
  }

  const std::string& GetParentForChild(const std::string& child_id) {
    return child_parent_map_[child_id];
  }

  std::set<std::string>& GetGroupedNotificationsForParent(
      const std::string& parent_id) {
    return notifications_in_parent_map_[parent_id];
  }

  bool GroupedChildNotificationExists(const std::string& child_id) {
    return child_parent_map_.find(child_id) != child_parent_map_.end();
  }

  bool ParentNotificationExists(const std::string& parent_id) {
    return notifications_in_parent_map_.find(parent_id) !=
           notifications_in_parent_map_.end();
  }

  // Replaces all instances of `old_parent_id` with `new_parent_id` in
  // the `child_parent_map_`.
  void ReplaceParentId(const std::string& new_parent_id,
                       const std::string& old_parent_id) {
    // Remove entry with `new_parent_id` as a child id and replace with
    // `old_parent_id` as a child id.
    child_parent_map_.erase(new_parent_id);
    child_parent_map_[old_parent_id] = new_parent_id;

    // Replace all occurrences of `old_parent_id` with `new_parent_id`.
    std::vector<std::string> to_be_updated;
    for (const auto& child : child_parent_map_) {
      if (child.second == old_parent_id)
        to_be_updated.push_back(child.first);
    }
    for (const auto& id : to_be_updated) {
      child_parent_map_.erase(child_parent_map_.find(id));
      child_parent_map_[id] = new_parent_id;
    }
  }

 private:
  // Map for looking up the parent `notification_id` for any given notification
  // id.
  std::map<std::string, std::string> child_parent_map_;

  // Map containing a list of child notification ids per each group parent id.
  // Used to keep track of grouped notifications which already have a parent
  // notification view.
  std::map<std::string, std::set<std::string>> notifications_in_parent_map_;
};

// Needs to be a static instance because
// we need a single instance to be shared across MessagePopupCollection and
// UnifiedMessageListView. TODO(crbug/1239033) Refactor
// NotificationViewController so we won't have to do this.
GroupedNotificationList& GetGroupedNotificationListInstance() {
  static base::NoDestructor<GroupedNotificationList> instance;
  return *instance;
}

}  // namespace

NotificationViewController::NotificationViewController()
    : grouped_notification_list_(&GetGroupedNotificationListInstance()) {
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
      grouped_notification_list_->AddGroupedNotification(notification->id(),
                                                         notification_id);
      parent_view->AddGroupNotification(*notification, /*newest_first=*/true);
    }
  }
}

const std::string& NotificationViewController::GetParentIdForChildForTest(
    const std::string& notification_id) {
  return grouped_notification_list_->GetParentForChild(notification_id);
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

  grouped_notification_list_->AddGroupedNotification(old_parent_id,
                                                     new_parent_id);

  Notification* new_parent_notification = notification_copy.get();
  MessageCenter::Get()->AddNotification(std::move(notification_copy));

  ConvertNotificationViewToGroupedNotificationView(
      /*ungrouped_notification_id=*/old_parent_id,
      /*new_grouped_notification_id=*/new_parent_id);

  grouped_notification_list_->ReplaceParentId(
      /*new_parent_id=*/new_parent_id,
      /*old_parent_id=*/old_parent_id);

  // Add the old parent notification as a group child to the
  // newly created parent notification which will act as a
  // container for this group as long as it exists.
  new_parent_notification->SetGroupParent();
  parent_notification->SetGroupChild();

  auto* parent_view = GetMessageViewForNotificationId(new_parent_id);
  if (parent_view)
    parent_view->AddGroupNotification(*parent_notification,
                                      /*newest_first=*/false);
}

void NotificationViewController::SetupSingleNotificationFromGroupedNotification(
    const std::string& group_parent_id,
    const std::string& new_single_notification_id) {
  auto* message_center = MessageCenter::Get();
  MessageView* parent_view = GetMessageViewForNotificationId(group_parent_id);
  auto* new_single_notification =
      message_center->FindNotificationById(new_single_notification_id);

  parent_view->RemoveGroupNotification(new_single_notification_id);
  parent_view->UpdateWithNotification(*new_single_notification);

  ConvertGroupedNotificationViewToNotificationView(
      /*grouped_notification_id=*/group_parent_id,
      /*new_single_notification_id=*/new_single_notification_id);

  message_center->FindNotificationById(group_parent_id)->ClearGroupParent();
  new_single_notification->ClearGroupChild();

  grouped_notification_list_->ClearGroupedNotification(group_parent_id);

  message_center->RemoveNotification(group_parent_id, /*by_user=*/false);
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

void NotificationViewController::RemoveGroupedChild(
    const std::string& notification_id) {
  if (!grouped_notification_list_->GroupedChildNotificationExists(
          notification_id)) {
    return;
  }

  const std::string parent_id =
      grouped_notification_list_->GetParentForChild(notification_id);

  MessageView* parent_view = GetMessageViewForNotificationId(parent_id);
  if (parent_view)
    parent_view->RemoveGroupNotification(notification_id);

  grouped_notification_list_->RemoveGroupedChildNotification(notification_id);

  // Convert back to a single notification if there is only one
  // group child left in the group notification.
  auto grouped_notifications =
      grouped_notification_list_->GetGroupedNotificationsForParent(parent_id);
  if (GetMessageViewForNotificationId(parent_id) &&
      grouped_notifications.size() == 1) {
    SetupSingleNotificationFromGroupedNotification(
        /*group_parent_id=*/parent_id,
        /*new_single_notification_id=*/*grouped_notifications.begin());
  }
}

void NotificationViewController::OnNotificationAdded(
    const std::string& notification_id) {
  auto* message_center = MessageCenter::Get();
  Notification* notification =
      message_center->FindNotificationById(notification_id);

  // We only need to process notifications that are children of an
  // existing group. So do nothing otherwise.
  if (!notification)
    return;

  if (!notification->group_child())
    return;

  Notification* parent_notification =
      message_center->FindOldestNotificationByNotiferId(
          notification->notifier_id());
  std::string parent_id = parent_notification->id();

  // If we are creating a new notification group for this `notifier_id`,
  // we must create a copy of the designated parent notification and
  // use it to set up a container notification which will hold all
  // notifications for this group.
  if (!grouped_notification_list_->ParentNotificationExists(parent_id))
    SetupParentNotification(&parent_id);

  grouped_notification_list_->AddGroupedNotification(notification_id,
                                                     parent_id);

  MessageView* parent_view = GetMessageViewForNotificationId(parent_id);
  if (parent_view)
    parent_view->AddGroupNotification(*notification, /*newest_first=*/false);
  else
    message_center->ResetSinglePopup(parent_id);
}

void NotificationViewController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  if (grouped_notification_list_->GroupedChildNotificationExists(
          notification_id)) {
    RemoveGroupedChild(notification_id);
  }

  if (grouped_notification_list_->ParentNotificationExists(notification_id)) {
    std::vector<std::string> to_be_deleted;
    auto grouped_notifications =
        grouped_notification_list_->GetGroupedNotificationsForParent(
            notification_id);
    std::copy(grouped_notifications.begin(), grouped_notifications.end(),
              std::back_inserter(to_be_deleted));
    grouped_notification_list_->ClearGroupedNotification(notification_id);

    for (const auto& id : to_be_deleted)
      MessageCenter::Get()->RemoveNotification(id, by_user);
  }
}

}  // namespace message_center
