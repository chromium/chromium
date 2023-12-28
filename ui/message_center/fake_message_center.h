// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_
#define UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"

namespace message_center {

// MessageCenter implementation of doing nothing. Useful for tests.
class FakeMessageCenter : public MessageCenter {
 public:
  FakeMessageCenter();

  FakeMessageCenter(const FakeMessageCenter&) = delete;
  FakeMessageCenter& operator=(const FakeMessageCenter&) = delete;

  ~FakeMessageCenter() override;

  // Overridden from MessageCenter.
  void AddObserver(MessageCenterObserver* observer) override;
  void RemoveObserver(MessageCenterObserver* observer) override;
  void AddNotificationBlocker(NotificationBlocker* blocker) override;
  void RemoveNotificationBlocker(NotificationBlocker* blocker) override;
  size_t NotificationCount() const override;
  bool HasPopupNotifications() const override;
  bool IsQuietMode() const override;
  bool IsSpokenFeedbackEnabled() const override;
  Notification* FindNotificationById(const std::string& id) override;
  Notification* FindParentNotification(Notification* notification) override;
  Notification* FindPopupNotificationById(const std::string& id) override;
  Notification* FindVisibleNotificationById(const std::string& id) override;
  NotificationList::Notifications FindNotificationsByAppId(
      const std::string& app_id) override;
  NotificationList::Notifications GetNotifications() override;
  const NotificationList::Notifications& GetVisibleNotifications() override;
  NotificationList::Notifications GetVisibleNotificationsWithoutBlocker(
      const NotificationBlocker* blocker) const override;
  NotificationList::PopupNotifications GetPopupNotifications() override;
  NotificationList::PopupNotifications GetPopupNotificationsWithoutBlocker(
      const NotificationBlocker& blocker) const override;
  void AddNotification(std::unique_ptr<Notification> notification) override;
  void UpdateNotification(
      const std::string& old_id,
      std::unique_ptr<Notification> new_notification) override;

  void RemoveNotification(const std::string& id, bool by_user) override;
  void RemoveNotificationsForNotifierId(const NotifierId& notifier_id) override;
  void RemoveAllNotifications(bool by_user, RemoveType type) override;
  void SetNotificationIcon(const std::string& notification_id,
                           const ui::ImageModel& image) override;

  void SetNotificationImage(const std::string& notification_id,
                            const gfx::Image& image) override;

  void ClickOnNotification(const std::string& id) override;
  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override;
  void ClickOnNotificationButtonWithReply(const std::string& id,
                                          int button_index,
                                          const std::u16string& reply) override;
  void ClickOnSettingsButton(const std::string& id) override;
  void ClickOnSnoozeButton(const std::string& id) override;
  void DisableNotification(const std::string& id) override;
  void MarkSinglePopupAsShown(const std::string& id,
                              bool mark_notification_as_read) override;
  void ResetPopupTimer(const std::string& id) override;
  void ResetSinglePopup(const std::string& id) override;
  void DisplayedNotification(const std::string& id,
                             const DisplaySource source) override;
  void SetQuietMode(
      bool in_quiet_mode,
      QuietModeSourceType type = QuietModeSourceType::kUserAction) override;
  QuietModeSourceType GetLastQuietModeChangeSourceType() const override;
  void SetSpokenFeedbackEnabled(bool enabled) override;
  void EnterQuietModeWithExpire(const base::TimeDelta& expires_in) override;
  void SetVisibility(Visibility visible) override;
  bool IsMessageCenterVisible() const override;
  ExpandState GetNotificationExpandState(const std::string& id) override;
  void SetNotificationExpandState(const std::string& id,
                                  const ExpandState state) override;
  void OnSetExpanded(const std::string& id, bool expanded) override;
  void SetHasMessageCenterView(bool has_message_center_view) override;
  bool HasMessageCenterView() const override;
  void RestartPopupTimers() override;
  void PausePopupTimers() override;
  const std::u16string& GetSystemNotificationAppName() const override;
  void SetSystemNotificationAppName(const std::u16string& name) override;
  void OnMessageViewHovered(const std::string& notification_id) override;

 protected:
  void DisableTimersForTest() override;
  const base::ObserverList<MessageCenterObserver>& observer_list() const {
    return observers_;
  }

 private:
  base::ObserverList<MessageCenterObserver> observers_;
  NotificationList notifications_;
  NotificationList::Notifications visible_notifications_;
  std::vector<raw_ptr<NotificationBlocker, VectorExperimental>> blockers_;
  bool has_message_center_view_ = true;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_
