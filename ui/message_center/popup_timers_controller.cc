// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/popup_timers_controller.h"

#include <algorithm>
#include <memory>

#include "base/containers/contains.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace message_center {

namespace {

bool UseHighPriorityDelay(Notification* notification) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS is going to ignore the `never_timeout` field so all notification
  // popups are automatically dismissed in 6 seconds. System priority
  // notifications with `never_timeout` set will be displayed for 30 minutes.
  const bool use_high_priority_delay =
      notification->never_timeout() &&
      notification->priority() == SYSTEM_PRIORITY;
#else
  // Web Notifications are given a longer on-screen time on non-Chrome OS
  // platforms as there is no notification center to dismiss them to.
  const bool use_high_priority_delay =
      notification->priority() > DEFAULT_PRIORITY ||
      notification->notifier_id().type == NotifierType::WEB_PAGE;
#endif

  return use_high_priority_delay;
}

}  // namespace

// Timeout values used to dismiss notifications automatically after they are
// shown.
int notification_timeout_default_seconds_ = kAutocloseDefaultDelaySeconds;
int notification_timeout_high_priority_seconds_ =
    kAutocloseHighPriorityDelaySeconds;

PopupTimersController::PopupTimersController(MessageCenter* message_center)
    : message_center_(message_center) {
  message_center_->AddObserver(this);
}

PopupTimersController::~PopupTimersController() {
  message_center_->RemoveObserver(this);
}

void PopupTimersController::StartTimer(const std::string& id,
                                       const base::TimeDelta& timeout) {
  PopupTimerCollection::const_iterator iter = popup_timers_.find(id);
  if (iter != popup_timers_.end()) {
    DCHECK(iter->second);
    iter->second->Start();
    return;
  }

  auto timer =
      std::make_unique<PopupTimer>(id, timeout, weak_ptr_factory_.GetWeakPtr());

  timer->Start();
  popup_timers_.emplace(id, std::move(timer));
}

void PopupTimersController::StartAll() {
  for (const auto& iter : popup_timers_)
    iter.second->Start();
}

void PopupTimersController::PauseAll() {
  for (const auto& iter : popup_timers_)
    iter.second->Pause();
}

void PopupTimersController::CancelTimer(const std::string& id) {
  popup_timers_.erase(id);
}

void PopupTimersController::SetNotificationTimeouts(int default_timeout,
                                                    int high_priority_timeout) {
  notification_timeout_default_seconds_ = default_timeout;
  notification_timeout_high_priority_seconds_ = high_priority_timeout;
}

void PopupTimersController::CancelAll() {
  popup_timers_.clear();
}

void PopupTimersController::TimerFinished(const std::string& id) {
  if (!base::Contains(popup_timers_, id))
    return;

  CancelTimer(id);
  message_center_->MarkSinglePopupAsShown(id, false);
}

base::TimeDelta PopupTimersController::GetTimeoutForNotification(
    Notification* notification) {
  return base::Seconds(UseHighPriorityDelay(notification)
                           ? notification_timeout_high_priority_seconds_
                           : notification_timeout_default_seconds_);
}

int PopupTimersController::GetNotificationTimeoutDefault() {
  return notification_timeout_default_seconds_;
}

void PopupTimersController::OnNotificationDisplayed(
    const std::string& id,
    const DisplaySource source) {
  OnNotificationUpdated(id);
}

void PopupTimersController::OnNotificationUpdated(const std::string& id) {
  NotificationList::PopupNotifications popup_notifications =
      message_center_->GetPopupNotifications();

  if (popup_notifications.empty()) {
    CancelAll();
    return;
  }

  auto iter = popup_notifications.begin();
  for (; iter != popup_notifications.end(); ++iter) {
    if ((*iter)->id() == id)
      break;
  }

  if (iter == popup_notifications.end()) {
    CancelTimer(id);
    return;
  }

// ChromeOS is going to ignore the `never_timeout` field for notification
// popups. Only enabled behind the `kNotificationsIgnoreRequireInteraction` flag
// for now.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!features::IsNotificationsIgnoreRequireInteractionEnabled()) {
    if ((*iter)->never_timeout()) {
      CancelTimer(id);
      return;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if ((*iter)->never_timeout()) {
    CancelTimer(id);
    return;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  auto timer = popup_timers_.find(id);
  // The timer must already have been started and not be running. Relies on
  // the invariant that |popup_timers_| only contains timers that have been
  // started.
  bool was_paused = timer != popup_timers_.end() && !timer->second->IsRunning();
  CancelTimer(id);
  StartTimer(id, GetTimeoutForNotification(*iter));

  // If a timer was paused before, pause it afterwards as well.
  // See crbug.com/710298
  if (was_paused) {
    popup_timers_.find(id)->second->Pause();
  }
}

void PopupTimersController::OnNotificationRemoved(const std::string& id,
                                                  bool by_user) {
  CancelTimer(id);
}

}  // namespace message_center
