// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_impl.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/message_center/lock_screen/lock_screen_controller.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/popup_timers_controller.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#endif  //  BUILDFLAG(IS_CHROMEOS_ASH)

namespace message_center {
namespace {

bool IsNotificationsGroupingEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

ScopedNotificationLimitOverrider* g_limit_overrider_instance_ = nullptr;

// Constants -------------------------------------------------------------------

// Indicates the notification count limit.
// NOTE: Used only when the notification limit feature is enabled.
constexpr int kChromeOSNotificationLimit = 75;

// Target notification count for the cleaning task triggered when the
// notification count exceeds `kChromeOSNotificationLimit`. This value is
// lower than `kChromeOSNotificationLimit` to reduce the frequency of hitting
// the limit. Because of unremovable notifications, the actual count after
// cleaning could exceed this target count.
// NOTE: Used only when the notification limit feature is enabled.
constexpr int kNotificationTargetCountAfterRemoval = 65;

// Helpers ---------------------------------------------------------------------

int GetNotificationLimit() {
  return g_limit_overrider_instance_
             ? g_limit_overrider_instance_->overriding_limit
             : kChromeOSNotificationLimit;
}

int GetTargetCountAfterRemoval() {
  return g_limit_overrider_instance_
             ? g_limit_overrider_instance_->overriding_target_count
             : kNotificationTargetCountAfterRemoval;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// MessageCenterImpl

MessageCenterImpl::MessageCenterImpl(
    std::unique_ptr<LockScreenController> lock_screen_controller)
    : lock_screen_controller_(std::move(lock_screen_controller)),
      popup_timers_controller_(std::make_unique<PopupTimersController>(this)),
      notifications_grouping_enabled_(IsNotificationsGroupingEnabled()),
      stats_collector_(this) {
  notification_list_ = std::make_unique<NotificationList>(this);
}

MessageCenterImpl::~MessageCenterImpl() = default;

void MessageCenterImpl::AddObserver(MessageCenterObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.AddObserver(observer);
}

void MessageCenterImpl::RemoveObserver(MessageCenterObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.RemoveObserver(observer);
}

void MessageCenterImpl::AddNotificationBlocker(NotificationBlocker* blocker) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (base::Contains(blockers_, blocker)) {
    return;
  }

  blocker->AddObserver(this);
  blockers_.push_back(blocker);
  OnBlockingStateChanged(blocker);
}

void MessageCenterImpl::RemoveNotificationBlocker(
    NotificationBlocker* blocker) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = base::ranges::find(blockers_, blocker);
  if (iter == blockers_.end()) {
    return;
  }
  blocker->RemoveObserver(this);
  blockers_.erase(iter);
  OnBlockingStateChanged(blocker);
}

void MessageCenterImpl::OnBlockingStateChanged(NotificationBlocker* blocker) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::list<std::string> blocked;
  NotificationList::PopupNotifications popups =
      notification_list_->GetPopupNotifications(blockers_, &blocked);

  visible_notifications_ =
      notification_list_->GetVisibleNotifications(blockers_);

  for (const std::string& notification_id : blocked) {
    observer_list_.Notify(&MessageCenterObserver::OnNotificationUpdated,
                          notification_id);
  }
  observer_list_.Notify(&MessageCenterObserver::OnBlockingStateChanged,
                        blocker);
}

void MessageCenterImpl::SetVisibility(Visibility visibility) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  visible_ = (visibility == VISIBILITY_MESSAGE_CENTER);

  if (visible_) {
    std::set<std::string> updated_ids;
    notification_list_->SetNotificationsShown(blockers_, &updated_ids);

    for (const auto& id : updated_ids) {
      observer_list_.Notify(&MessageCenterObserver::OnNotificationUpdated, id);
    }

    for (Notification* notification : GetPopupNotifications()) {
      MarkSinglePopupAsShown(notification->id(), false);
    }
  }

  observer_list_.Notify(&MessageCenterObserver::OnCenterVisibilityChanged,
                        visibility);
}

bool MessageCenterImpl::IsMessageCenterVisible() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return visible_;
}

ExpandState MessageCenterImpl::GetNotificationExpandState(
    const std::string& id) {
  return notification_list_->GetNotificationExpandState(id);
}

void MessageCenterImpl::SetNotificationExpandState(
    const std::string& id,
    const ExpandState expand_state) {
  DCHECK(FindVisibleNotificationById(id));

  notification_list_->SetNotificationExpandState(id, expand_state);
}

void MessageCenterImpl::OnSetExpanded(const std::string& id, bool expanded) {
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);

  if (delegate) {
    delegate->ExpandStateChanged(expanded);
  }
}

void MessageCenterImpl::SetHasMessageCenterView(bool has_message_center_view) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  has_message_center_view_ = has_message_center_view;
}

bool MessageCenterImpl::HasMessageCenterView() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return has_message_center_view_;
}

size_t MessageCenterImpl::NotificationCount() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return visible_notifications_.size();
}

bool MessageCenterImpl::HasPopupNotifications() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !IsMessageCenterVisible() &&
         notification_list_->HasPopupNotifications(blockers_);
}

bool MessageCenterImpl::IsQuietMode() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return notification_list_->quiet_mode();
}

bool MessageCenterImpl::IsSpokenFeedbackEnabled() const {
  return spoken_feedback_enabled_;
}

Notification* MessageCenterImpl::FindNotificationById(const std::string& id) {
  return notification_list_->GetNotificationById(id);
}

Notification* MessageCenterImpl::FindParentNotification(
    Notification* notification) {
  // For a notification to have a parent notification, they must have identical
  // notifier_ids. To make sure that the notifications come from
  // the same website for the same user. Also make sure to only group
  // notifications from web pages with valid origin urls. For system
  // notifications, currently we only group privacy indicators notification.
  // For ARC notifications, only group them when the flag
  // IsRenderArcNotificationsByChromeEnabled() is enabled.
  bool is_privacy_indicators_notification = false;
  bool render_arc_notifications_by_chrome = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  is_privacy_indicators_notification =
      notification->notifier_id().id == ash::kPrivacyIndicatorsNotifierId;
  render_arc_notifications_by_chrome =
      ash::features::IsRenderArcNotificationsByChromeEnabled();
#endif

  if (!is_privacy_indicators_notification &&
      (notification->origin_url().is_empty() ||
       notification->notifier_id().type != NotifierType::WEB_PAGE) &&
      notification->notifier_id().type != NotifierType::ARC_APPLICATION) {
    return nullptr;
  }

  NotificationList::Notifications notifications =
      notification_list_->GetNotificationsByNotifierId(
          notification->notifier_id());

  // Handle ARC notification grouping in Chrome
  if (notification->notifier_id().type == NotifierType::ARC_APPLICATION) {
    // If render_arc_notifications_by_chrome flag is not enabled,
    // use Android grouping and do not apply grouping rules from the chrome
    // side.
    if (!render_arc_notifications_by_chrome) {
      return nullptr;
    }

    // To stay consistent with Android, ARC notifications with group key
    // are grouped using notifier_id() where id and group keys are checked.
    // For ARC notifications without a group key,
    // only group them when there are more than 4 notifications
    if (!notification->notifier_id().group_key.has_value()) {
      if (notifications.size() < 4) {
        return nullptr;
      }
      for (Notification* n : notifications) {
        if (n->group_parent() || n->group_child()) {
          continue;
        }
        n->SetGroupChild();
      }
    }
  }

  auto parent_notification_it = base::ranges::find_if(
      notifications,
      [](Notification* notification) { return notification->group_parent(); });

  // If there's already a notification assigned to be the group parent,
  // returns that notification immediately.
  if (parent_notification_it != notifications.cend()) {
    return *parent_notification_it;
  }

  // Otherwise, the parent notification should be the oldest one. Since
  // `notifications` keeps notifications ordered with the most recent one in
  // the front, the oldest one should be the last in the list.
  return notifications.size() ? *notifications.rbegin() : nullptr;
}

Notification* MessageCenterImpl::FindPopupNotificationById(
    const std::string& id) {
  auto notifications = GetPopupNotifications();
  auto notification = base::ranges::find(notifications, id, &Notification::id);

  return notification == notifications.end() ? nullptr : *notification;
}

Notification* MessageCenterImpl::FindVisibleNotificationById(
    const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const auto& notifications = GetVisibleNotifications();
  for (Notification* notification : notifications) {
    if (notification->id() == id) {
      return notification;
    }
  }

  return nullptr;
}

NotificationList::Notifications MessageCenterImpl::FindNotificationsByAppId(
    const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return notification_list_->GetNotificationsByAppId(app_id);
}

NotificationList::Notifications MessageCenterImpl::GetNotifications() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return notification_list_->GetNotifications();
}

const NotificationList::Notifications&
MessageCenterImpl::GetVisibleNotifications() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return visible_notifications_;
}

NotificationList::Notifications
MessageCenterImpl::GetVisibleNotificationsWithoutBlocker(
    const NotificationBlocker* blocker) const {
  return notification_list_->GetVisibleNotificationsWithoutBlocker(blockers_,
                                                                   blocker);
}

NotificationList::PopupNotifications
MessageCenterImpl::GetPopupNotifications() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return notification_list_->GetPopupNotifications(blockers_, nullptr);
}

NotificationList::PopupNotifications
MessageCenterImpl::GetPopupNotificationsWithoutBlocker(
    const NotificationBlocker& blocker) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return notification_list_->GetPopupNotificationsWithoutBlocker(blockers_,
                                                                 blocker);
}

//------------------------------------------------------------------------------
// Client code interface.
void MessageCenterImpl::AddNotification(
    std::unique_ptr<Notification> notification) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(notification);

  notification->set_allow_group(notifications_grouping_enabled_);

  const std::string id = notification->id();
  for (NotificationBlocker* blocker : blockers_) {
    blocker->CheckState();
  }

  // Sometimes the notifications can be added with the same id and the
  // |notification_list| will replace the notification instead of adding new.
  // This is essentially an update rather than addition.
  if (notification_list_->GetNotificationById(id)) {
    UpdateNotification(id, std::move(notification));
    return;
  }

  if (auto* const parent = FindParentNotification(notification.get());
      notification->allow_group() && parent && !notification->group_parent()) {
    parent->SetGroupParent();
    notification->SetGroupChild();
  }

  notification_list_->AddNotification(std::move(notification));

  visible_notifications_ =
      notification_list_->GetVisibleNotifications(blockers_);
  observer_list_.Notify(&MessageCenterObserver::OnNotificationAdded, id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ScheduleCleaningTaskIfCountOverLimit();
#endif  // IS_CHROMEOS_ASH
}

void MessageCenterImpl::UpdateNotification(
    const std::string& old_id,
    std::unique_ptr<Notification> new_notification) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (NotificationBlocker* blocker : blockers_) {
    blocker->CheckState();
  }

  auto* old_notification = notification_list_->GetNotificationById(old_id);
  if (old_notification &&
      old_notification->notifier_id() == new_notification->notifier_id()) {
    // Copy grouping metadata to the new notification.
    if (old_notification->group_parent()) {
      new_notification->SetGroupParent();
    }
    if (old_notification->group_child()) {
      new_notification->SetGroupChild();
    }
  }

  std::string new_id = new_notification->id();
  notification_list_->UpdateNotificationMessage(old_id,
                                                std::move(new_notification));
  visible_notifications_ =
      notification_list_->GetVisibleNotifications(blockers_);
  for (MessageCenterObserver& observer : observer_list_) {
    if (old_id == new_id) {
      observer.OnNotificationUpdated(new_id);
    } else {
      observer.OnNotificationRemoved(old_id, false);
      observer.OnNotificationAdded(new_id);
    }
  }
}

void MessageCenterImpl::RemoveNotification(const std::string& id,
                                           bool by_user) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Notification* notification = notification_list_->GetNotificationById(id);
  if (!notification) {
    return;
  }

  if (by_user && notification->pinned()) {
    // When pinned, a popup will not be removed completely but moved into the
    // message center bubble.
    MarkSinglePopupAsShown(id, true);
    return;
  }

  // In many cases |id| is a reference to an existing notification instance
  // but the instance can be destructed in this method. Hence copies the id
  // explicitly here.
  std::string copied_id(id);

  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(copied_id);

  // Remove notification before calling the Close method in case it calls
  // RemoveNotification reentrantly.
  notification_list_->RemoveNotification(copied_id);

  if (delegate.get()) {
    delegate->Close(by_user);
  }

  visible_notifications_ =
      notification_list_->GetVisibleNotifications(blockers_);
  for (MessageCenterObserver& observer : observer_list_) {
    observer.OnNotificationRemoved(copied_id, by_user);
  }
}

void MessageCenterImpl::RemoveNotificationsForNotifierId(
    const NotifierId& notifier_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NotificationList::Notifications notifications =
      notification_list_->GetNotificationsByNotifierId(notifier_id);
  for (Notification* notification : notifications) {
    RemoveNotification(notification->id(), false);
  }
  if (!notifications.empty()) {
    visible_notifications_ =
        notification_list_->GetVisibleNotifications(blockers_);
  }
}

void MessageCenterImpl::RemoveAllNotifications(bool by_user, RemoveType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool remove_pinned = (type == RemoveType::ALL);

  const NotificationBlockers& blockers =
      remove_pinned ? NotificationBlockers() /* empty blockers */
                    : blockers_;             /* use default blockers */

  const NotificationList::Notifications notifications =
      notification_list_->GetVisibleNotifications(blockers);
  std::set<std::string> ids;
  for (Notification* notification : notifications) {
    if (!remove_pinned && notification->pinned()) {
      continue;
    }

    ids.insert(notification->id());
    scoped_refptr<NotificationDelegate> delegate = notification->delegate();

    // Remove notification before calling the Close method in case it calls
    // RemoveNotification reentrantly.
    notification_list_->RemoveNotification(notification->id());

    if (delegate.get()) {
      delegate->Close(by_user);
    }
  }

  if (!ids.empty()) {
    visible_notifications_ =
        notification_list_->GetVisibleNotifications(blockers_);
  }
  for (const auto& id : ids) {
    for (MessageCenterObserver& observer : observer_list_) {
      observer.OnNotificationRemoved(id, by_user);
    }
  }
}

void MessageCenterImpl::SetNotificationIcon(const std::string& notification_id,
                                            const ui::ImageModel& image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (notification_list_->SetNotificationIcon(notification_id, image)) {
    for (MessageCenterObserver& observer : observer_list_) {
      observer.OnNotificationUpdated(notification_id);
    }
  }
}

void MessageCenterImpl::SetNotificationImage(const std::string& notification_id,
                                             const gfx::Image& image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (notification_list_->SetNotificationImage(notification_id, image)) {
    for (MessageCenterObserver& observer : observer_list_) {
      observer.OnNotificationUpdated(notification_id);
    }
  }
}

void MessageCenterImpl::ClickOnNotification(const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!FindVisibleNotificationById(id)) {
    return;
  }

  lock_screen_controller_->DismissLockScreenThenExecute(
      base::BindOnce(&MessageCenterImpl::ClickOnNotificationUnlocked,
                     base::Unretained(this), id, std::nullopt, std::nullopt),
      base::OnceClosure());
}

void MessageCenterImpl::ClickOnNotificationButton(const std::string& id,
                                                  int button_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!FindVisibleNotificationById(id)) {
    return;
  }

  lock_screen_controller_->DismissLockScreenThenExecute(
      base::BindOnce(&MessageCenterImpl::ClickOnNotificationUnlocked,
                     base::Unretained(this), id, button_index, std::nullopt),
      base::OnceClosure());
}

void MessageCenterImpl::ClickOnNotificationButtonWithReply(
    const std::string& id,
    int button_index,
    const std::u16string& reply) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!FindVisibleNotificationById(id)) {
    return;
  }

  lock_screen_controller_->DismissLockScreenThenExecute(
      base::BindOnce(&MessageCenterImpl::ClickOnNotificationUnlocked,
                     base::Unretained(this), id, button_index, reply),
      base::OnceClosure());
}

void MessageCenterImpl::ClickOnNotificationUnlocked(
    const std::string& id,
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This method must be called under unlocked screen.
  DCHECK(!lock_screen_controller_->IsScreenLocked());

  // Ensure the notification is still visible.
  if (!FindVisibleNotificationById(id)) {
    return;
  }

  if (HasMessageCenterView() && HasPopupNotifications()) {
    MarkSinglePopupAsShown(id, true);
  }
  for (MessageCenterObserver& observer : observer_list_) {
    observer.OnNotificationClicked(id, button_index, reply);
  }

  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate) {
    delegate->Click(button_index, reply);
  }

  if (const Notification* notification =
          notification_list_->GetNotificationById(id);
      notification && notification->rich_notification_data().remove_on_click) {
    RemoveNotification(id, /*by_user=*/true);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void MessageCenterImpl::ScheduleCleaningTaskIfCountOverLimit() {
  if (!ash::features::IsNotificationLimitEnabled() ||
      notification_list_->size() <= GetNotificationLimit()) {
    return;
  }

  if (!overlimit_handler_timer_.IsRunning()) {
    overlimit_handler_timer_.Start(
        FROM_HERE, base::TimeDelta(), /*receiver=*/this,
        &MessageCenterImpl::RemoveNotificationsIfOverLimit);
  }
}

void MessageCenterImpl::RemoveNotificationsIfOverLimit() {
  CHECK(ash::features::IsNotificationLimitEnabled());

  if (int notification_count = notification_list_->size();
      notification_count > GetNotificationLimit()) {
    for (const std::string& id :
         notification_list_->GetTopKRemovableNotificationIds(
             notification_count - GetTargetCountAfterRemoval())) {
      RemoveNotification(id, /*by_user=*/false);
    }

    base::UmaHistogramBoolean("Ash.Notification.RemovedByLimitEnforcement",
                              true);
  }
}

#endif  // IS_CHROMEOS_ASH

void MessageCenterImpl::ClickOnSettingsButton(const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Notification* notification = notification_list_->GetNotificationById(id);

  bool handled_by_delegate =
      notification && notification->delegate() &&
      (notification->rich_notification_data().settings_button_handler ==
       SettingsButtonHandler::DELEGATE);
  if (handled_by_delegate) {
    notification->delegate()->SettingsClick();
  }

  for (MessageCenterObserver& observer : observer_list_) {
    observer.OnNotificationSettingsClicked(handled_by_delegate);
  }
}

void MessageCenterImpl::ClickOnSnoozeButton(const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Notification* notification = notification_list_->GetNotificationById(id);

  bool handled_by_delegate =
      notification && notification_list_->GetNotificationDelegate(id);
  if (handled_by_delegate) {
    notification->delegate()->SnoozeButtonClicked();
  }
}

void MessageCenterImpl::DisableNotification(const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Notification* notification = notification_list_->GetNotificationById(id);

  if (notification && notification->delegate()) {
    notification->delegate()->DisableNotification();
    RemoveNotificationsForNotifierId(notification->notifier_id());
  }
}

void MessageCenterImpl::MarkSinglePopupAsShown(const std::string& id,
                                               bool mark_notification_as_read) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!FindNotificationById(id)) {
    return;
  }

  if (HasMessageCenterView()) {
    notification_list_->MarkSinglePopupAsShown(id, mark_notification_as_read);
    for (MessageCenterObserver& observer : observer_list_) {
      observer.OnNotificationUpdated(id);
      observer.OnNotificationPopupShown(id, mark_notification_as_read);
    }
  } else {
    RemoveNotification(id, false);
  }
}

void MessageCenterImpl::ResetPopupTimer(const std::string& id) {
  DCHECK(FindPopupNotificationById(id));

  popup_timers_controller_->CancelTimer(id);
  popup_timers_controller_->StartTimer(
      id, popup_timers_controller_->GetTimeoutForNotification(
              FindPopupNotificationById(id)));
}

void MessageCenterImpl::ResetSinglePopup(const std::string& id) {
  notification_list_->ResetSinglePopup(id);
  for (MessageCenterObserver& observer : observer_list_) {
    observer.OnNotificationUpdated(id);
  }
}

void MessageCenterImpl::DisplayedNotification(const std::string& id,
                                              const DisplaySource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This method may be called from the handlers, so we shouldn't manipulate
  // notifications in this method.

  if (!FindVisibleNotificationById(id)) {
    return;
  }

  if (HasPopupNotifications()) {
    notification_list_->MarkSinglePopupAsDisplayed(id);
  }
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  for (MessageCenterObserver& observer : observer_list_) {
    observer.OnNotificationDisplayed(id, source);
  }
}

void MessageCenterImpl::SetQuietMode(bool in_quiet_mode,
                                     QuietModeSourceType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (in_quiet_mode != notification_list_->quiet_mode()) {
    last_quiet_mode_change_source_type_ = type;
    notification_list_->SetQuietMode(in_quiet_mode);
    for (MessageCenterObserver& observer : observer_list_) {
      observer.OnQuietModeChanged(in_quiet_mode);
    }
  }
  quiet_mode_timer_.Stop();
}

QuietModeSourceType MessageCenterImpl::GetLastQuietModeChangeSourceType()
    const {
  return last_quiet_mode_change_source_type_;
}

void MessageCenterImpl::SetSpokenFeedbackEnabled(bool enabled) {
  spoken_feedback_enabled_ = enabled;
}

void MessageCenterImpl::EnterQuietModeWithExpire(
    const base::TimeDelta& expires_in) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!quiet_mode_timer_.IsRunning()) {
    notification_list_->SetQuietMode(true);
    for (MessageCenterObserver& observer : observer_list_) {
      observer.OnQuietModeChanged(true);
    }
  }

  // This will restart the timer if it is already running.
  quiet_mode_timer_.Start(
      FROM_HERE, expires_in,
      base::BindOnce(&MessageCenterImpl::SetQuietMode, base::Unretained(this),
                     false, QuietModeSourceType::kUserAction));
}

void MessageCenterImpl::RestartPopupTimers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (popup_timers_controller_) {
    popup_timers_controller_->StartAll();
  }
}

void MessageCenterImpl::PausePopupTimers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (popup_timers_controller_) {
    popup_timers_controller_->PauseAll();
  }
}

const std::u16string& MessageCenterImpl::GetSystemNotificationAppName() const {
  return system_notification_app_name_;
}

void MessageCenterImpl::SetSystemNotificationAppName(
    const std::u16string& name) {
  system_notification_app_name_ = name;
}

void MessageCenterImpl::OnMessageViewHovered(
    const std::string& notification_id) {
  for (MessageCenterObserver& observer : observer_list_) {
    observer.OnMessageViewHovered(notification_id);
  }
}

void MessageCenterImpl::DisableTimersForTest() {
  popup_timers_controller_.reset();
}

// ScopedNotificationLimitOverrider --------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_ASH)
ScopedNotificationLimitOverrider::ScopedNotificationLimitOverrider(
    size_t limit,
    size_t target_count)
    : overriding_limit(limit), overriding_target_count(target_count) {
  CHECK(!g_limit_overrider_instance_);
  g_limit_overrider_instance_ = this;
}

ScopedNotificationLimitOverrider::~ScopedNotificationLimitOverrider() {
  CHECK(g_limit_overrider_instance_);
  g_limit_overrider_instance_ = nullptr;
}
#endif  // IS_CHROMEOS_ASH

}  // namespace message_center
