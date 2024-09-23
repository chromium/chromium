// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

class DownloadNotification;
class DownloadNotificationTestBase;

// Interface to manage the NotificationList. The client (e.g. Chrome) calls
// [Add|Remove|Update]Notification to create and update notifications in the
// list. It also sends those changes to its observers when a notification
// is shown, closed, or clicked on.
//
// MessageCenter is agnostic of profiles; it uses the string returned by
// Notification::id() to uniquely identify a notification. It is
// the caller's responsibility to formulate the id so that 2 different
// notification should have different ids. For example, if the caller supports
// multiple profiles, then caller should encode both profile characteristics and
// notification front end's notification id into a new id and set it into the
// notification instance before passing that in. Consequently the id passed to
// observers will be this unique id, which can be used with MessageCenter
// interface but probably not higher level interfaces.

namespace message_center {

namespace test {
class MessagePopupCollectionTest;
}

class LockScreenController;
class MessageCenterObserver;
class MessageCenterImplTest;
class NotificationBlocker;

class MESSAGE_CENTER_EXPORT MessageCenter {
 public:
  enum class RemoveType {
    // Remove all notifications.
    ALL,
    // Remove non-pinned notification (don't remove invisible ones).
    NON_PINNED,
  };

  // Creates the global message center object with default LockScreenController.
  static void Initialize();
  // Creates the global message center object with custom LockScreenController.
  static void Initialize(std::unique_ptr<LockScreenController> controller);

  // Returns the global message center object. Returns null if Initialize is
  // not called.
  static MessageCenter* Get();

  // Destroys the global message_center object.
  static void Shutdown();

  MessageCenter(const MessageCenter&) = delete;
  MessageCenter& operator=(const MessageCenter&) = delete;

  // Management of the observer list.
  virtual void AddObserver(MessageCenterObserver* observer) = 0;
  virtual void RemoveObserver(MessageCenterObserver* observer) = 0;

  // Queries of current notification list status.
  virtual size_t NotificationCount() const = 0;
  virtual bool HasPopupNotifications() const = 0;
  virtual bool IsQuietMode() const = 0;

  // Returns true if chrome vox spoken feedback is enabled.
  virtual bool IsSpokenFeedbackEnabled() const = 0;

  // Returns the notification with the corresponding id. If not found, returns
  // nullptr. Notification instance is owned by this list.
  virtual Notification* FindNotificationById(const std::string& id) = 0;

  // Find the parent notification for the corresponding notification. This is
  // the oldest notification with the same url. Returns nullptr if not found.
  // The returned instance is owned by the message center.
  virtual Notification* FindParentNotification(Notification* notification) = 0;

  virtual Notification* FindPopupNotificationById(const std::string& id) = 0;

  // Find the notification with the corresponding id. Returns null if not
  // found. The returned instance is owned by the message center.
  virtual Notification* FindVisibleNotificationById(const std::string& id) = 0;

  // Find all notifications with the corresponding |app_id|. Returns an
  // empty set if none are found.
  virtual NotificationList::Notifications FindNotificationsByAppId(
      const std::string& app_id) = 0;

  // Gets all notifications the message center knows about. These might contain
  // currently hidden ones due to any active NotificationBlockers.
  virtual NotificationList::Notifications GetNotifications() = 0;

  // Gets all notifications to be shown to the user in the message center.  Note
  // that queued changes due to the message center being open are not reflected
  // in this list.
  virtual const NotificationList::Notifications& GetVisibleNotifications() = 0;

  // Gets all notifications to be shown to the user in the message center if not
  // for the given blocker.
  virtual NotificationList::Notifications GetVisibleNotificationsWithoutBlocker(
      const NotificationBlocker* blocker) const = 0;

  // Gets all notifications being shown as popups. This should not be affected
  // by the change queue since notifications are not held up while the state is
  // VISIBILITY_TRANSIENT or VISIBILITY_SETTINGS.
  //
  // Popups returned by this method are assumed to have now been shown to the
  // user.
  virtual NotificationList::PopupNotifications GetPopupNotifications() = 0;

  // Gets all notifications that would be popups if not for the given blocker.
  // Ignores limits in the number of popups (e.g. for screen space).
  virtual NotificationList::PopupNotifications
  GetPopupNotificationsWithoutBlocker(
      const NotificationBlocker& blocker) const = 0;

  // Management of NotificationBlockers.
  virtual void AddNotificationBlocker(NotificationBlocker* blocker) = 0;
  virtual void RemoveNotificationBlocker(NotificationBlocker* blocker) = 0;

  // Basic operations of notification: add/remove/update.

  // Adds a new notification.
  virtual void AddNotification(std::unique_ptr<Notification> notification) = 0;

  // Updates an existing notification with id = old_id and set its id to new_id.
  virtual void UpdateNotification(
      const std::string& old_id,
      std::unique_ptr<Notification> new_notification) = 0;

  // Removes an existing notification.
  virtual void RemoveNotification(const std::string& id, bool by_user) = 0;
  virtual void RemoveNotificationsForNotifierId(
      const NotifierId& notifier_id) = 0;
  virtual void RemoveAllNotifications(bool by_user, RemoveType type) = 0;

  // Sets the icon image. Icon appears at the top-left of the notification.
  virtual void SetNotificationIcon(const std::string& notification_id,
                                   const ui::ImageModel& image) = 0;

  // Sets the large image for the notifications of type == TYPE_IMAGE. Specified
  // image will appear below of the notification.
  virtual void SetNotificationImage(const std::string& notification_id,
                                    const gfx::Image& image) = 0;

  // This should be called by UI classes when a notification is clicked to
  // trigger the notification's delegate callback and also update the message
  // center observers.
  virtual void ClickOnNotification(const std::string& id) = 0;

  // This should be called by UI classes when a notification button is clicked
  // to trigger the notification's delegate callback and also update the message
  // center observers.
  virtual void ClickOnNotificationButton(const std::string& id,
                                         int button_index) = 0;

  // This should be called by UI classes when a notification button with an
  // input is clicked to trigger the notification's delegate callback and also
  // update the message center observers.
  virtual void ClickOnNotificationButtonWithReply(
      const std::string& id,
      int button_index,
      const std::u16string& reply) = 0;

  // Called by the UI classes when the settings buttons is clicked
  // to trigger the notification's delegate and update the message
  // center observers.
  virtual void ClickOnSettingsButton(const std::string& id) = 0;

  // Called when the snooze buttons is clicked to trigger the notification's
  // delegate.
  virtual void ClickOnSnoozeButton(const std::string& id) = 0;

  // This should be called by UI classes when a user select from notification
  // inline settings to disable notifications from the same origin of the
  // notification.
  virtual void DisableNotification(const std::string& id) = 0;

  // Called by the UI classes to mark a popup as shown, preventing it from being
  // shown in the future. `mark_notification_as_read`, if false, will unset the
  // read bit on a notification, increasing the unread count of the center.
  virtual void MarkSinglePopupAsShown(const std::string& id,
                                      bool mark_notification_as_read) = 0;

  // Resets the timer for the popup associated with the provided notification
  // id.
  virtual void ResetPopupTimer(const std::string& id) = 0;

  // Resets the state for a popup so it is shown again.
  virtual void ResetSinglePopup(const std::string& id) = 0;

  // This should be called by UI classes when a notification is first displayed
  // to the user, in order to decrement the unread_count for the tray, and to
  // notify observers that the notification is visible.
  virtual void DisplayedNotification(const std::string& id,
                                     const DisplaySource source) = 0;

  // This can be called to change the quiet mode state (without a timeout).
  virtual void SetQuietMode(
      bool in_quiet_mode,
      QuietModeSourceType type = QuietModeSourceType::kUserAction) = 0;
  virtual QuietModeSourceType GetLastQuietModeChangeSourceType() const = 0;

  // Used to set the spoken feedback state.
  virtual void SetSpokenFeedbackEnabled(bool enabled) = 0;

  // Temporarily enables quiet mode for |expires_in| time.
  virtual void EnterQuietModeWithExpire(const base::TimeDelta& expires_in) = 0;

  // Informs the notification list whether the message center is visible.
  // This affects whether or not a message has been "read".
  virtual void SetVisibility(Visibility visible) = 0;

  // Allows querying the visibility of the center.
  virtual bool IsMessageCenterVisible() const = 0;

  // Access for the `ExpandState` stored for each notification in the
  // `NotificationList`. The `ExpandState` is kept alongside other
  // notifications' state information in the `NotificationState` struct. The
  // `ExpandState signifies whether the notification has been manually expanded
  // or collapsed by the user.
  virtual ExpandState GetNotificationExpandState(const std::string& id) = 0;
  virtual void SetNotificationExpandState(const std::string& id,
                                          const ExpandState state) = 0;

  // Called when `MessageView::SetExpanded` or the overrides are called. It
  // will trigger 'ExpandStateChanged' in the notification's delegate.
  virtual void OnSetExpanded(const std::string& id, bool expanded) = 0;

  // Informs the MessageCenter whether there's a bubble anchored to a system
  // tray which holds notifications. If false, only toasts are shown (e.g. on
  // desktop Linux and Windows). When there's no message center view, updated
  // notifications will be re-appear as toasts even if they've already been
  // shown.
  virtual void SetHasMessageCenterView(bool has_message_center_view) = 0;
  virtual bool HasMessageCenterView() const = 0;

  // UI classes should call this when there is cause to leave popups visible for
  // longer than the default (for example, when the mouse hovers over a popup).
  virtual void PausePopupTimers() = 0;

  // UI classes should call this when the popup timers should restart (for
  // example, after the mouse leaves the popup.)
  virtual void RestartPopupTimers() = 0;

  // The user-visible "app name" for system-generated notifications, which is
  // used to identify the application that generated a notification. Only used
  // for MD style notifications, which means that currently it's only set and
  // used on Chrome OS. On Chrome OS, this is "Chrome OS".
  virtual const std::u16string& GetSystemNotificationAppName() const = 0;
  virtual void SetSystemNotificationAppName(const std::u16string& name) = 0;

  // Called when a message view associated with `notification_id` is hovered on.
  virtual void OnMessageViewHovered(const std::string& notification_id) = 0;

 protected:
  friend class ::DownloadNotification;
  friend class ::DownloadNotificationTestBase;
  friend class MessageCenterImplTest;
  friend class MessageCenterImplTestWithChangeQueue;
  friend class MessageCenterImplTestWithoutChangeQueue;
  friend class NotificationViewControllerTest;
  friend class UiControllerTest;
  friend class TrayViewControllerTest;
  friend class MessagePopupCollectionTest;
  friend class MessagePopupViewTest;
  virtual void DisableTimersForTest() = 0;

  MessageCenter();
  virtual ~MessageCenter();
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_H_
