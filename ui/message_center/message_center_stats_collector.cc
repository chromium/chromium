// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/message_center/message_center_stats_collector.h"

#include <stddef.h>

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/not_fatal_until.h"
#include "ui/message_center/message_center.h"

namespace message_center {

MessageCenterStatsCollector::NotificationStats::NotificationStats() {}

MessageCenterStatsCollector::NotificationStats::NotificationStats(
    const std::string& id)
    : id_(id) {
  for (size_t i = 0; i < NOTIFICATION_ACTION_COUNT; i++) {
    actions_[i] = false;
  }
}

MessageCenterStatsCollector::NotificationStats::~NotificationStats() {}

void MessageCenterStatsCollector::NotificationStats::CollectAction(
    NotificationActionType type) {
  DCHECK(!id_.empty());

  UMA_HISTOGRAM_ENUMERATION("Notifications.Actions", type,
                            NOTIFICATION_ACTION_COUNT);
  actions_[type] = true;
}

void MessageCenterStatsCollector::NotificationStats::RecordAggregateStats() {
  DCHECK(!id_.empty());

  for (size_t i = 0; i < NOTIFICATION_ACTION_COUNT; i++) {
    if (!actions_[i])
      continue;
    UMA_HISTOGRAM_ENUMERATION("Notifications.PerNotificationActions",
                              static_cast<NotificationActionType>(i),
                              NOTIFICATION_ACTION_COUNT);
  }
}

void MessageCenterStatsCollector::RecordNotifierType(NotifierType type) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.NotifierType", type);
}

MessageCenterStatsCollector::MessageCenterStatsCollector(
    MessageCenter* message_center)
    : message_center_(message_center) {
  message_center_->AddObserver(this);
}

MessageCenterStatsCollector::~MessageCenterStatsCollector() {
  message_center_->RemoveObserver(this);
}

void MessageCenterStatsCollector::OnNotificationAdded(
    const std::string& notification_id) {
  stats_[notification_id] = NotificationStats(notification_id);

  auto iter = stats_.find(notification_id);
  CHECK(iter != stats_.end(), base::NotFatalUntil::M130);

  stats_[notification_id].CollectAction(NOTIFICATION_ACTION_ADD);

  const auto* notification =
      message_center_->FindVisibleNotificationById(notification_id);
  if (notification)
    RecordNotifierType(notification->notifier_id().type);
}

void MessageCenterStatsCollector::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  auto iter = stats_.find(notification_id);
  if (iter == stats_.end())
    return;
  NotificationStats& notification_stat = iter->second;
  notification_stat.CollectAction(by_user
                                      ? NOTIFICATION_ACTION_CLOSE_BY_USER
                                      : NOTIFICATION_ACTION_CLOSE_BY_SYSTEM);
  notification_stat.RecordAggregateStats();
  stats_.erase(notification_id);
}

void MessageCenterStatsCollector::OnNotificationUpdated(
    const std::string& notification_id) {
  auto iter = stats_.find(notification_id);
  if (iter == stats_.end())
    return;
  NotificationStats& notification_stat = iter->second;

  notification_stat.CollectAction(NOTIFICATION_ACTION_UPDATE);
}

void MessageCenterStatsCollector::OnNotificationClicked(
    const std::string& notification_id,
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  auto iter = stats_.find(notification_id);
  if (iter == stats_.end())
    return;
  NotificationStats& notification_stat = iter->second;

  notification_stat.CollectAction(button_index
                                      ? NOTIFICATION_ACTION_BUTTON_CLICK
                                      : NOTIFICATION_ACTION_CLICK);
}

void MessageCenterStatsCollector::OnNotificationSettingsClicked(bool handled) {
  base::RecordAction(base::UserMetricsAction("Notifications.ShowSiteSettings"));
}

void MessageCenterStatsCollector::OnNotificationDisplayed(
    const std::string& notification_id,
    const DisplaySource source) {
  auto iter = stats_.find(notification_id);
  if (iter == stats_.end())
    return;
  NotificationStats& notification_stat = iter->second;

  notification_stat.CollectAction(NOTIFICATION_ACTION_DISPLAY);
}

void MessageCenterStatsCollector::OnCenterVisibilityChanged(
    Visibility visibility) {
  switch (visibility) {
    case VISIBILITY_TRANSIENT:
      break;
    case VISIBILITY_MESSAGE_CENTER:
      base::RecordAction(
          base::UserMetricsAction("Notifications.ShowMessageCenter"));
      break;
  }
}

void MessageCenterStatsCollector::OnQuietModeChanged(bool in_quiet_mode) {
  if (in_quiet_mode) {
    base::RecordAction(base::UserMetricsAction("Notifications.Mute"));
  } else {
    base::RecordAction(base::UserMetricsAction("Notifications.Unmute"));
  }
}

}  // namespace message_center
