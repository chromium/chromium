// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_VIEW_CONTROLLER_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_VIEW_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_observer.h"

namespace message_center {

class MessageView;

// A controller class to manage adding, removing and updating group
// notifications.
class MESSAGE_CENTER_EXPORT NotificationViewController
    : public MessageCenterObserver {
 public:
  NotificationViewController();
  NotificationViewController(const NotificationViewController& other) = delete;
  NotificationViewController& operator=(
      const NotificationViewController& other) = delete;
  ~NotificationViewController() override;

  // MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

 protected:
  // Adds grouped child notifications that belong to a parent message
  // view.
  void PopulateGroupParent(const std::string& notification_id);

 private:
  virtual MessageView* GetMessageViewForNotificationId(
      const std::string& notification_id) = 0;

  // Updates the notification id associated with a message center view and
  // popup if required. We do this to covert an existing message view into
  // a message view that acts as a container for grouped notifications.
  // Creating a new view for this would make the code simpler but we need
  // to do it in place to make it easier to animate the conversion between
  // grouped and non-grouped notifications.
  virtual void ConvertNotificationViewToGroupedNotificationView(
      const std::string& ungrouped_notification_id,
      const std::string& new_grouped_notification_id) = 0;

  // Sets up a parent view to hold all message views for
  // a grouped notification. Does this by creating a copy of the
  // parent notification and switching the notification_ids of the
  // current message view associated with the parent notification.
  void SetupParentNotification(std::string* parent_id);

  // Creates a copy notification that will act as a parent notification
  // for its group.
  std::unique_ptr<Notification> CreateCopyForParentNotification(
      const Notification& parent_notification);

  // Replaces all instances of `old_parent_id` with `new_parent_id` in
  // the `child_parent_map_`.
  void UpdateChildParentMap(const std::string& new_parent_id,
                            const std::string& old_parent_id);

  // Map for looking up the parent notification_id for any given notification
  // id.
  std::map<std::string, std::string> child_parent_map_;

  // Set of current parent notification ids. Used to keep track of grouped
  // notifications which already have a parent notification view.
  std::set<std::string> parent_grouped_notification_id_set_;

  base::ScopedObservation<MessageCenter, MessageCenterObserver> observer_{this};
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
