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

  virtual MessageView* GetMessageViewForNotificationId(
      const std::string& notification_id) = 0;

  // MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

 private:
  // Map for looking up the parent notification_id for any given notification
  // id.
  std::map<std::string, std::string> child_parent_map_;

  base::ScopedObservation<MessageCenter, MessageCenterObserver> observer_{this};
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
