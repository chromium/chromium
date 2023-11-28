// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_VIEW_CONTROLLER_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_VIEW_CONTROLLER_H_

#include <string>

#include "ui/message_center/message_center_export.h"

namespace message_center {

class MessageView;

// A controller class to manage adding, removing and updating group
// notifications.
class MESSAGE_CENTER_EXPORT NotificationViewController {
 public:
  virtual ~NotificationViewController() = default;

  // Returns the `MessageView` associated with `notification_id`
  virtual MessageView* GetMessageViewForNotificationId(
      const std::string& notification_id) = 0;

  // Animate all notification views after a resize.
  virtual void AnimateResize() = 0;

  // Updates the notification id associated with a `MessageCenterView` and
  // popup if required. We do this to covert an existing message view into
  // a message view that acts as a container for grouped notifications.
  // Creating a new view for this would make the code simpler but we need
  // to do it in place to make it easier to animate the conversion between
  // grouped and non-grouped notifications.
  virtual void ConvertNotificationViewToGroupedNotificationView(
      const std::string& ungrouped_notification_id,
      const std::string& new_grouped_notification_id) = 0;

  // Updates the notification id associated with a `MessageCenterView` and
  // popup if needed. This is done to convert an existing grouped notification
  // view back into a single notification view.
  virtual void ConvertGroupedNotificationViewToNotificationView(
      const std::string& grouped_notification_id,
      const std::string& new_single_notification_id) = 0;

  // Called to update child notification view inside a parent view.
  virtual void OnChildNotificationViewUpdated(
      const std::string& parent_notification_id,
      const std::string& child_notification_id) = 0;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFICATION_VIEW_CONTROLLER_H_
