// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_OBSERVER_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_OBSERVER_H_

#include <optional>
#include <string>

#include "base/observer_list_types.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_types.h"

namespace message_center {
class NotificationBlocker;

// An observer class for the change of notifications in the MessageCenter.
// WARNING: It is not safe to modify the message center from within these
// callbacks.
class MESSAGE_CENTER_EXPORT MessageCenterObserver
    : public base::CheckedObserver {
 public:
  // Called when the notification associated with |notification_id| is added
  // to the notification_list.
  virtual void OnNotificationAdded(const std::string& notification_id) {}

  // Called when the notification associated with |notification_id| is removed
  // from the notification_list.
  virtual void OnNotificationRemoved(const std::string& notification_id,
                                     bool by_user) {}

  // Called when the contents of the notification associated with
  // |notification_id| is updated.
  virtual void OnNotificationUpdated(const std::string& notification_id) {}

  // Called when a click event happens on the notification associated with
  // |notification_id|. |button_index| will be nullopt if the click occurred on
  // the body of the notification. |reply| will be filled in only if there was
  // an input field associated with the button.
  virtual void OnNotificationClicked(
      const std::string& notification_id,
      const std::optional<int>& button_index,
      const std::optional<std::u16string>& reply) {}

  // Called when notification settings button is clicked. The |handled| argument
  // indicates whether the notification delegate already handled the operation.
  virtual void OnNotificationSettingsClicked(bool handled) {}

  // Called when the notification associated with |notification_id| is actually
  // displayed.
  virtual void OnNotificationDisplayed(
      const std::string& notification_id,
      const DisplaySource source) {}

  // Called when the message view associated with `notification_id` is hovered.
  virtual void OnMessageViewHovered(const std::string& notification_id) {}

  // Called when the notification center is shown or hidden.
  virtual void OnCenterVisibilityChanged(Visibility visibility) {}

  // Called whenever the quiet mode changes as a result of user action or when
  // quiet mode expires.
  virtual void OnQuietModeChanged(bool in_quiet_mode) {}

  // Called when the blocking state of |blocker| is changed.
  virtual void OnBlockingStateChanged(NotificationBlocker* blocker) {}

  // Called after a visible notification popup closes, indicating it has been
  // shown to the user, and whether the notification is marked as read.
  virtual void OnNotificationPopupShown(const std::string& notification_id,
                                        bool mark_notification_as_read) {}
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_OBSERVER_H_
