// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_

#include <stddef.h>

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace base {
class TimeDelta;
}

namespace gfx {
class Image;
}

namespace message_center {

class Notification;
class NotificationDelegate;
struct NotifierId;

enum class ExpandState { DEFAULT = 0, USER_EXPANDED = 1, USER_COLLAPSED = 2 };

// Comparers used to auto-sort the lists of Notifications.
struct MESSAGE_CENTER_EXPORT ComparePriorityTimestampSerial {
  bool operator()(Notification* n1, Notification* n2) const;
};

struct MESSAGE_CENTER_EXPORT CompareTimestampSerial {
  bool operator()(Notification* n1, Notification* n2) const;
};

// An adapter to allow use of the comparers above with std::unique_ptr.
template <typename PlainCompare>
struct UniquePtrCompare {
  template <typename T>
  bool operator()(const std::unique_ptr<T>& n1,
                  const std::unique_ptr<T>& n2) const {
    return PlainCompare()(n1.get(), n2.get());
  }
};

// A helper class to manage the list of notifications.
class MESSAGE_CENTER_EXPORT NotificationList {
 public:
  struct NotificationState {
    bool operator!=(const NotificationState& other) const;

    bool shown_as_popup = false;
    bool is_read = false;
    ExpandState expand_state = ExpandState::DEFAULT;
  };

  // Auto-sorted set. Matches the order in which Notifications are shown in
  // Notification Center.
  using Notifications = std::set<raw_ptr<Notification, SetExperimental>,
                                 ComparePriorityTimestampSerial>;
  using OwnedNotifications =
      std::map<std::unique_ptr<Notification>,
               NotificationState,
               UniquePtrCompare<ComparePriorityTimestampSerial>>;

  // Auto-sorted set used to return the Notifications to be shown as popup
  // toasts.
  using PopupNotifications = std::set<Notification*, CompareTimestampSerial>;

  explicit NotificationList(MessageCenter* message_center);

  NotificationList(const NotificationList&) = delete;
  NotificationList& operator=(const NotificationList&) = delete;

  virtual ~NotificationList();

  int size() const { return notifications_.size(); }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns the notification IDs prioritized for removal as follows:
  // 1. Lower priority notifications are removed before higher priority ones.
  // 2. For notifications with equal priority, older ones are removed first.
  // 3. The most recent notifications are kept.
  // NOTE:
  // 1. This function is used only if the notification limit feature is enabled.
  // 2. The returned array's size could be less than `count`.
  std::vector<std::string> GetTopKRemovableNotificationIds(size_t count) const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Makes a message "read". Collects the set of ids whose state have changed
  // and set to |udpated_ids|. NULL if updated ids don't matter.
  void SetNotificationsShown(const NotificationBlockers& blockers,
                             std::set<std::string>* updated_ids);

  void AddNotification(std::unique_ptr<Notification> notification);

  void UpdateNotificationMessage(
      const std::string& old_id,
      std::unique_ptr<Notification> new_notification);

  void RemoveNotification(const std::string& id);

  // Returns all notifications in this list.
  Notifications GetNotifications() const;

  // Returns all notifications that have a matching |notifier_id|.
  Notifications GetNotificationsByNotifierId(
      const NotifierId& notifier_id) const;

  // Returns all notifications that have a matching |app_id|.
  Notifications GetNotificationsByAppId(const std::string& app_id) const;

  // Returns all notifications that have a matching `origin_url`.
  Notifications GetNotificationsByOriginUrl(const GURL& origin_url) const;

  // Returns true if the notification exists and was updated.
  bool SetNotificationIcon(const std::string& notification_id,
                           const ui::ImageModel& image);

  // Returns true if the notification exists and was updated.
  bool SetNotificationImage(const std::string& notification_id,
                            const gfx::Image& image);

  // Returns true if |id| matches a notification in the list and that
  // notification's type matches the given type.
  bool HasNotificationOfType(const std::string& id,
                             const NotificationType type) const;

  // Returns false if the first notification has been shown as a popup (which
  // means that all notifications have been shown).
  bool HasPopupNotifications(const NotificationBlockers& blockers) const;

  // Returns the recent notifications of the priority higher then LOW,
  // that have not been shown as a popup. kMaxVisiblePopupNotifications are
  // used to limit the number of notifications for the DEFAULT priority.
  // It also stores the list of notifications which are blocked by |blockers|
  // to |blocked|. |blocked| can be NULL if the caller doesn't care which
  // notifications are blocked.
  PopupNotifications GetPopupNotifications(const NotificationBlockers& blockers,
                                           std::list<std::string>* blocked);

  // Lists all notifications (even those that aren't shown due to shown popup
  // limits) that would qualify as popups with the given list of blockers.
  // Doesn't mark popups as shown.
  PopupNotifications GetPopupNotificationsWithoutBlocker(
      const NotificationBlockers& blockers,
      const NotificationBlocker& blocker) const;

  // Marks a specific popup item as shown. Set |mark_notification_as_read| to
  // true in case marking the notification as read too.
  void MarkSinglePopupAsShown(const std::string& id,
                              bool mark_notification_as_read);

  // Marks a specific popup item as displayed.
  void MarkSinglePopupAsDisplayed(const std::string& id);

  // Resets the state for a pop up so that it can be shown again. Used to
  // bring up a grouped notification when a new item is added to it.
  void ResetSinglePopup(const std::string& id);

  // `ExpandState` signifies whether the notification with the specified `id`
  // has been manually expanded or collapsed by the user.
  ExpandState GetNotificationExpandState(const std::string& id);
  void SetNotificationExpandState(const std::string& id,
                                  ExpandState expand_state);

  NotificationDelegate* GetNotificationDelegate(const std::string& id);

  bool quiet_mode() const { return quiet_mode_; }

  // Sets the current quiet mode status to |quiet_mode|.
  void SetQuietMode(bool quiet_mode);

  // Sets the current quiet mode to true. The quiet mode will expire in the
  // specified time-delta from now.
  void EnterQuietModeWithExpire(const base::TimeDelta& expires_in);

  // Returns the notification with the corresponding id. If not found, returns
  // NULL. Notification instance is owned by this list.
  Notification* GetNotificationById(const std::string& id);

  void PopupBlocked(const std::string& id);

  // Returns all visible notifications, in a (priority-timestamp) order.
  // Suitable for rendering notifications in a MessageCenter.
  Notifications GetVisibleNotifications(
      const NotificationBlockers& blockers) const;

  // Returns all visible notifications if not for the provided blocker.
  Notifications GetVisibleNotificationsWithoutBlocker(
      const NotificationBlockers& blockers,
      const NotificationBlocker* ignored_blocker) const;

  size_t NotificationCount(const NotificationBlockers& blockers) const;

 private:
  friend class NotificationListTest;
  FRIEND_TEST_ALL_PREFIXES(NotificationListTest, TestPushingShownNotification);

  // Iterates through the list and returns the first notification matching |id|.
  OwnedNotifications::iterator GetNotification(const std::string& id);
  OwnedNotifications::const_iterator GetNotification(
      const std::string& id) const;

  void EraseNotification(OwnedNotifications::iterator iter);

  void PushNotification(std::unique_ptr<Notification> notification);

  raw_ptr<MessageCenter> message_center_;  // owner
  OwnedNotifications notifications_;
  bool quiet_mode_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_
