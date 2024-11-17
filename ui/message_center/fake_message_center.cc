// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/fake_message_center.h"

#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "ui/message_center/notification_list.h"

namespace message_center {

FakeMessageCenter::FakeMessageCenter() : notifications_(this) {}

FakeMessageCenter::~FakeMessageCenter() = default;

void FakeMessageCenter::AddObserver(MessageCenterObserver* observer) {
  observers_.AddObserver(observer);
}

void FakeMessageCenter::RemoveObserver(MessageCenterObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FakeMessageCenter::AddNotificationBlocker(NotificationBlocker* blocker) {}

void FakeMessageCenter::RemoveNotificationBlocker(
    NotificationBlocker* blocker) {}

size_t FakeMessageCenter::NotificationCount() const {
  return notifications_.GetNotifications().size();
}

bool FakeMessageCenter::HasPopupNotifications() const {
  return false;
}

bool FakeMessageCenter::IsQuietMode() const {
  return false;
}

bool FakeMessageCenter::IsSpokenFeedbackEnabled() const {
  return false;
}

Notification* FakeMessageCenter::FindNotificationById(const std::string& id) {
  // stub
  return nullptr;
}

Notification* FakeMessageCenter::FindParentNotification(
    Notification* notification) {
  // stub
  return nullptr;
}

Notification* FakeMessageCenter::FindPopupNotificationById(
    const std::string& id) {
  // stub
  return nullptr;
}

Notification* FakeMessageCenter::FindVisibleNotificationById(
    const std::string& id) {
  const auto& notifications = GetVisibleNotifications();
  for (Notification* notification : notifications) {
    if (notification->id() == id) {
      return notification;
    }
  }

  return nullptr;
}

NotificationList::Notifications FakeMessageCenter::FindNotificationsByAppId(
    const std::string& app_id) {
  return notifications_.GetNotificationsByAppId(app_id);
}

NotificationList::Notifications FakeMessageCenter::GetNotifications() {
  return notifications_.GetNotifications();
}

const NotificationList::Notifications&
FakeMessageCenter::GetVisibleNotifications() {
  return visible_notifications_;
}

NotificationList::Notifications
FakeMessageCenter::GetVisibleNotificationsWithoutBlocker(
    const NotificationBlocker* blocker) const {
  return visible_notifications_;
}

NotificationList::PopupNotifications
FakeMessageCenter::GetPopupNotifications() {
  return NotificationList::PopupNotifications();
}

NotificationList::PopupNotifications
FakeMessageCenter::GetPopupNotificationsWithoutBlocker(
    const NotificationBlocker& blocker) const {
  return NotificationList::PopupNotifications();
}

void FakeMessageCenter::AddNotification(
    std::unique_ptr<Notification> notification) {
  std::string id = notification->id();
  notifications_.AddNotification(std::move(notification));
  visible_notifications_ = notifications_.GetVisibleNotifications(blockers_);
  observers_.Notify(&MessageCenterObserver::OnNotificationAdded, id);
}

void FakeMessageCenter::UpdateNotification(
    const std::string& old_id,
    std::unique_ptr<Notification> new_notification) {
  std::string new_id = new_notification->id();
  notifications_.UpdateNotificationMessage(old_id, std::move(new_notification));
  visible_notifications_ = notifications_.GetVisibleNotifications(blockers_);
  for (auto& observer : observers_) {
    if (old_id == new_id) {
      observer.OnNotificationUpdated(old_id);
    } else {
      observer.OnNotificationRemoved(old_id, false /* by_user */);
      observer.OnNotificationAdded(new_id);
    }
  }
}

void FakeMessageCenter::RemoveNotification(const std::string& id,
                                           bool by_user) {
  notifications_.RemoveNotification(id);
  visible_notifications_ = notifications_.GetVisibleNotifications(blockers_);
  observers_.Notify(&MessageCenterObserver::OnNotificationRemoved, id, by_user);
}

void FakeMessageCenter::RemoveNotificationsForNotifierId(
    const NotifierId& notifier_id) {}

void FakeMessageCenter::RemoveAllNotifications(bool by_user, RemoveType type) {
  // Only removing all is supported.
  DCHECK_EQ(type, RemoveType::ALL);
  for (const Notification* notification : notifications_.GetNotifications()) {
    // This is safe to remove since GetNotifications() returned a copy.
    RemoveNotification(notification->id(), by_user);
  }
}

void FakeMessageCenter::SetNotificationIcon(const std::string& notification_id,
                                            const ui::ImageModel& image) {}

void FakeMessageCenter::SetNotificationImage(const std::string& notification_id,
                                             const gfx::Image& image) {}

void FakeMessageCenter::ClickOnNotification(const std::string& id) {}

void FakeMessageCenter::ClickOnNotificationButton(const std::string& id,
                                                  int button_index) {}

void FakeMessageCenter::ClickOnNotificationButtonWithReply(
    const std::string& id,
    int button_index,
    const std::u16string& reply) {}

void FakeMessageCenter::ClickOnSettingsButton(const std::string& id) {}

void FakeMessageCenter::ClickOnSnoozeButton(const std::string& id) {}

void FakeMessageCenter::DisableNotification(const std::string& id) {}

void FakeMessageCenter::MarkSinglePopupAsShown(const std::string& id,
                                               bool mark_notification_as_read) {
}

void FakeMessageCenter::ResetPopupTimer(const std::string& id) {}

void FakeMessageCenter::ResetSinglePopup(const std::string& id) {}

void FakeMessageCenter::DisplayedNotification(const std::string& id,
                                              const DisplaySource source) {}

void FakeMessageCenter::SetQuietMode(bool in_quiet_mode,
                                     QuietModeSourceType type) {}

QuietModeSourceType FakeMessageCenter::GetLastQuietModeChangeSourceType()
    const {
  return QuietModeSourceType::kUserAction;
}

void FakeMessageCenter::SetSpokenFeedbackEnabled(bool enabled) {}

void FakeMessageCenter::EnterQuietModeWithExpire(
    const base::TimeDelta& expires_in) {}

void FakeMessageCenter::SetVisibility(Visibility visible) {}

bool FakeMessageCenter::IsMessageCenterVisible() const {
  return false;
}

ExpandState FakeMessageCenter::GetNotificationExpandState(
    const std::string& id) {
  return ExpandState::DEFAULT;
}

void FakeMessageCenter::SetNotificationExpandState(const std::string& id,
                                                   const ExpandState state) {}

void FakeMessageCenter::OnSetExpanded(const std::string& id, bool expanded) {}

void FakeMessageCenter::SetHasMessageCenterView(bool has_message_center_view) {
  has_message_center_view_ = has_message_center_view;
}

bool FakeMessageCenter::HasMessageCenterView() const {
  return has_message_center_view_;
}

void FakeMessageCenter::RestartPopupTimers() {}

void FakeMessageCenter::PausePopupTimers() {}

const std::u16string& FakeMessageCenter::GetSystemNotificationAppName() const {
  return base::EmptyString16();
}

void FakeMessageCenter::SetSystemNotificationAppName(
    const std::u16string& product_os_name) {}

void FakeMessageCenter::OnMessageViewHovered(
    const std::string& notification_id) {}

void FakeMessageCenter::DisableTimersForTest() {}

}  // namespace message_center
