// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_STATS_COLLECTOR_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_STATS_COLLECTOR_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace message_center {

class MessageCenter;

// MessageCenterStatsCollector sends both raw and per-notification statistics
// to the UMA servers, if the user has opted in.  It observes the message center
// to gather its data.
class MessageCenterStatsCollector : public MessageCenterObserver {
 public:
  enum NotificationActionType {
    NOTIFICATION_ACTION_UNKNOWN,
    NOTIFICATION_ACTION_ADD,
    NOTIFICATION_ACTION_UPDATE,
    NOTIFICATION_ACTION_CLICK,
    NOTIFICATION_ACTION_BUTTON_CLICK,
    NOTIFICATION_ACTION_DISPLAY,
    NOTIFICATION_ACTION_CLOSE_BY_USER,
    NOTIFICATION_ACTION_CLOSE_BY_SYSTEM,
    // NOTE: Add new action types only immediately above this line. Also,
    // make sure the enum list in tools/histogram/histograms.xml is
    // updated with any change in here.
    NOTIFICATION_ACTION_COUNT
  };

  explicit MessageCenterStatsCollector(MessageCenter* message_center);
  ~MessageCenterStatsCollector() override;

 private:
  // Represents the aggregate stats for each notification.
  class NotificationStats {
   public:
    // Default constructor for map.
    NotificationStats();

    explicit NotificationStats(const std::string& id);
    virtual ~NotificationStats();

    // Called when we get an action from the message center.
    void CollectAction(NotificationActionType type);

    // Sends aggregate data to UMA.
    void RecordAggregateStats();

   private:
    std::string id_;
    bool actions_[NOTIFICATION_ACTION_COUNT];
  };

  // Sends notifier type to UMA. Called when a notification is added.
  void RecordNotifierType(NotifierType type);

  // MessageCenterObserver
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;
  void OnNotificationUpdated(const std::string& notification_id) override;
  void OnNotificationClicked(
      const std::string& notification_id,
      const base::Optional<int>& button_index,
      const base::Optional<base::string16>& reply) override;
  void OnNotificationSettingsClicked(bool handled) override;
  void OnNotificationDisplayed(const std::string& notification_id,
                               const DisplaySource source) override;
  void OnCenterVisibilityChanged(Visibility visibility) override;
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // Weak, global.
  MessageCenter* message_center_;

  typedef std::map<std::string, NotificationStats> StatsCollection;
  StatsCollection stats_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterStatsCollector);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_STATS_COLLECTOR_H_
