// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_list.h"

#include <stddef.h>

#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using base::UTF8ToUTF16;

namespace message_center {

using NotificationState = NotificationList::NotificationState;

class NotificationListTest : public testing::Test {
 public:
  NotificationListTest() {}
  ~NotificationListTest() override {}

  void SetUp() override {
    message_center_.reset(new FakeMessageCenter());
    notification_list_.reset(new NotificationList(message_center_.get()));
    counter_ = 0;
  }

 protected:
  // Currently NotificationListTest doesn't care about some fields like title or
  // message, so put a simple template on it. Returns the id of the new
  // notification.
  std::string AddNotification(const RichNotificationData& optional_fields) {
    std::string new_id;
    std::unique_ptr<Notification> notification(
        MakeNotification(optional_fields, &new_id));
    notification_list_->AddNotification(std::move(notification));
    counter_++;
    return new_id;
  }

  std::string AddNotification() {
    return AddNotification(RichNotificationData());
  }

  // Construct a new notification for testing, but don't add it to the list yet.
  std::unique_ptr<Notification> MakeNotification(
      const RichNotificationData& optional_fields,
      std::string* id_out) {
    *id_out = base::StringPrintf(kIdFormat, counter_);
    std::unique_ptr<Notification> notification(new Notification(
        NOTIFICATION_TYPE_SIMPLE, *id_out,
        UTF8ToUTF16(base::StringPrintf(kTitleFormat, counter_)),
        UTF8ToUTF16(base::StringPrintf(kMessageFormat, counter_)), gfx::Image(),
        UTF8ToUTF16(kDisplaySource), GURL(),
        NotifierId(NotifierId::APPLICATION, kExtensionId), optional_fields,
        NULL));
    return notification;
  }

  std::unique_ptr<Notification> MakeNotification(std::string* id_out) {
    return MakeNotification(RichNotificationData(), id_out);
  }

  // Utility methods of AddNotification.
  std::string AddPriorityNotification(NotificationPriority priority) {
    RichNotificationData optional;
    optional.priority = priority;
    return AddNotification(optional);
  }

  NotificationList::PopupNotifications GetPopups() {
    return notification_list_->GetPopupNotifications(blockers_, NULL);
  }

  size_t GetPopupCounts() {
    return GetPopups().size();
  }

  Notification* GetNotification(const std::string& id) {
    auto iter = notification_list_->GetNotification(id);
    if (iter == notification_list_->notifications_.end())
      return NULL;
    return iter->first.get();
  }

  NotificationState GetNotificationState(const std::string& id) {
    auto iter = notification_list_->GetNotification(id);
    EXPECT_FALSE(iter == notification_list_->notifications_.end());
    return iter->second;
  }

  static const char kIdFormat[];
  static const char kTitleFormat[];
  static const char kMessageFormat[];
  static const char kDisplaySource[];
  static const char kExtensionId[];

  std::unique_ptr<FakeMessageCenter> message_center_;
  std::unique_ptr<NotificationList> notification_list_;
  NotificationBlockers blockers_;
  size_t counter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationListTest);
};

bool IsInNotifications(const NotificationList::Notifications& notifications,
                       const std::string& id) {
  for (auto iter = notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() == id)
      return true;
  }
  return false;
}

const char NotificationListTest::kIdFormat[] = "id%ld";
const char NotificationListTest::kTitleFormat[] = "id%ld";
const char NotificationListTest::kMessageFormat[] = "message%ld";
const char NotificationListTest::kDisplaySource[] = "source";
const char NotificationListTest::kExtensionId[] = "ext";

TEST_F(NotificationListTest, Basic) {
  ASSERT_EQ(0u, notification_list_->NotificationCount(blockers_));

  std::string id0 = AddNotification();
  EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));
  std::string id1 = AddNotification();
  EXPECT_EQ(2u, notification_list_->NotificationCount(blockers_));

  EXPECT_TRUE(notification_list_->HasPopupNotifications(blockers_));
  EXPECT_TRUE(notification_list_->GetNotificationById(id0));
  EXPECT_TRUE(notification_list_->GetNotificationById(id1));
  EXPECT_FALSE(notification_list_->GetNotificationById(id1 + "foo"));

  EXPECT_EQ(2u, GetPopupCounts());

  notification_list_->MarkSinglePopupAsShown(id0, true);
  notification_list_->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(2u, notification_list_->NotificationCount(blockers_));
  EXPECT_EQ(0u, GetPopupCounts());

  notification_list_->RemoveNotification(id0);
  EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));

  AddNotification();
  EXPECT_EQ(2u, notification_list_->NotificationCount(blockers_));
}

TEST_F(NotificationListTest, MessageCenterVisible) {
  AddNotification();
  EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));
  ASSERT_EQ(1u, GetPopupCounts());

  // Resets the unread count and popup counts.
  notification_list_->SetNotificationsShown(blockers_, NULL);
  ASSERT_EQ(0u, GetPopupCounts());
}

TEST_F(NotificationListTest, UpdateNotification) {
  std::string id0 = AddNotification();
  std::string replaced = id0 + "_replaced";
  EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));
  std::unique_ptr<Notification> notification(new Notification(
      NOTIFICATION_TYPE_SIMPLE, replaced, UTF8ToUTF16("newtitle"),
      UTF8ToUTF16("newbody"), gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(NotifierId::APPLICATION, kExtensionId), RichNotificationData(),
      NULL));
  notification_list_->UpdateNotificationMessage(id0, std::move(notification));
  EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));
  const NotificationList::Notifications notifications =
      notification_list_->GetVisibleNotifications(blockers_);
  EXPECT_EQ(replaced, (*notifications.begin())->id());
  EXPECT_EQ(UTF8ToUTF16("newtitle"), (*notifications.begin())->title());
  EXPECT_EQ(UTF8ToUTF16("newbody"), (*notifications.begin())->message());
}

TEST_F(NotificationListTest, UpdateNotificationWithRenotifyAndQuietMode) {
  for (size_t quiet_mode = 0u; quiet_mode < 2u; ++quiet_mode) {
    // Set Do Not Disturb mode.
    notification_list_->SetQuietMode(static_cast<bool>(quiet_mode));

    // Create notification.
    std::string old_id;
    auto old_notification = MakeNotification(&old_id);
    notification_list_->AddNotification(std::move(old_notification));
    notification_list_->MarkSinglePopupAsShown(old_id, true);
    EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));

    std::string new_id;
    auto new_notification = MakeNotification(&new_id);
    // Set the renotify flag and update.
    new_notification->set_renotify(true);
    notification_list_->UpdateNotificationMessage(old_id,
                                                  std::move(new_notification));
    const NotificationList::Notifications notifications =
        notification_list_->GetVisibleNotifications(blockers_);
    EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));
    EXPECT_EQ(new_id, (*notifications.begin())->id());

    // Normally, |shown_as_popup| should be reset in order to show the popup
    // again.
    // In quiet mode, |shown_as_popup| should not be reset, as popup should not
    // be shown even though renotify was set.
    const NotificationList::PopupNotifications popup_notifications =
        notification_list_->GetPopupNotifications(blockers_, nullptr);
    if (quiet_mode) {
      ASSERT_EQ(0U, popup_notifications.size());
    } else {
      ASSERT_EQ(1U, popup_notifications.size());
      EXPECT_EQ(new_id, (*popup_notifications.begin())->id());
    }
  }
}

TEST_F(NotificationListTest, GetNotificationsByNotifierId) {
  NotifierId id0(NotifierId::APPLICATION, "ext0");
  NotifierId id1(NotifierId::APPLICATION, "ext1");
  NotifierId id2(GURL("http://example.com"));
  NotifierId id3(NotifierId::SYSTEM_COMPONENT, "system-notifier");
  std::unique_ptr<Notification> notification(new Notification(
      NOTIFICATION_TYPE_SIMPLE, "id0", UTF8ToUTF16("title0"),
      UTF8ToUTF16("message0"), gfx::Image(), UTF8ToUTF16("source0"), GURL(),
      id0, RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(notification));
  notification.reset(new Notification(
      NOTIFICATION_TYPE_SIMPLE, "id1", UTF8ToUTF16("title1"),
      UTF8ToUTF16("message1"), gfx::Image(), UTF8ToUTF16("source0"), GURL(),
      id0, RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(notification));
  notification.reset(new Notification(
      NOTIFICATION_TYPE_SIMPLE, "id2", UTF8ToUTF16("title1"),
      UTF8ToUTF16("message1"), gfx::Image(), UTF8ToUTF16("source1"), GURL(),
      id0, RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(notification));
  notification.reset(new Notification(
      NOTIFICATION_TYPE_SIMPLE, "id3", UTF8ToUTF16("title1"),
      UTF8ToUTF16("message1"), gfx::Image(), UTF8ToUTF16("source2"), GURL(),
      id1, RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(notification));
  notification.reset(new Notification(
      NOTIFICATION_TYPE_SIMPLE, "id4", UTF8ToUTF16("title1"),
      UTF8ToUTF16("message1"), gfx::Image(), UTF8ToUTF16("source2"), GURL(),
      id2, RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(notification));
  notification.reset(new Notification(
      NOTIFICATION_TYPE_SIMPLE, "id5", UTF8ToUTF16("title1"),
      UTF8ToUTF16("message1"), gfx::Image(), UTF8ToUTF16("source2"), GURL(),
      id3, RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(notification));

  NotificationList::Notifications by_notifier_id =
      notification_list_->GetNotificationsByNotifierId(id0);
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id0"));
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id1"));
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id2"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id3"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id4"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id5"));

  by_notifier_id = notification_list_->GetNotificationsByNotifierId(id1);
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id0"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id1"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id2"));
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id3"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id4"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id5"));

  by_notifier_id = notification_list_->GetNotificationsByNotifierId(id2);
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id0"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id1"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id2"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id3"));
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id4"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id5"));

  by_notifier_id = notification_list_->GetNotificationsByNotifierId(id3);
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id0"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id1"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id2"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id3"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id4"));
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id5"));
}

TEST_F(NotificationListTest, OldPopupShouldNotBeHidden) {
  std::vector<std::string> ids;
  for (size_t i = 0; i <= kMaxVisiblePopupNotifications; i++)
    ids.push_back(AddNotification());

  NotificationList::PopupNotifications popups = GetPopups();
  // The popup should contain the oldest kMaxVisiblePopupNotifications. Newer
  // one should come earlier in the popup list. It means, the last element
  // of |popups| should be the firstly added one, and so on.
  EXPECT_EQ(kMaxVisiblePopupNotifications, popups.size());
  auto iter = popups.rbegin();
  for (size_t i = 0; i < kMaxVisiblePopupNotifications; ++i, ++iter) {
    EXPECT_EQ(ids[i], (*iter)->id()) << i;
  }

  for (auto iter = popups.begin(); iter != popups.end(); ++iter) {
    notification_list_->MarkSinglePopupAsShown((*iter)->id(), false);
  }
  popups.clear();
  popups = GetPopups();
  ASSERT_EQ(1u, popups.size());
  EXPECT_EQ(ids.back(), (*popups.begin())->id());
}

TEST_F(NotificationListTest, Priority) {
  ASSERT_EQ(0u, notification_list_->NotificationCount(blockers_));

  // Default priority has the limit on the number of the popups.
  for (size_t i = 0; i <= kMaxVisiblePopupNotifications; ++i)
    AddNotification();
  EXPECT_EQ(kMaxVisiblePopupNotifications + 1,
            notification_list_->NotificationCount(blockers_));
  EXPECT_EQ(kMaxVisiblePopupNotifications, GetPopupCounts());

  // Low priority: not visible to popups.
  notification_list_->SetNotificationsShown(blockers_, NULL);
  AddPriorityNotification(LOW_PRIORITY);
  EXPECT_EQ(kMaxVisiblePopupNotifications + 2,
            notification_list_->NotificationCount(blockers_));
  EXPECT_EQ(0u, GetPopupCounts());

  // Minimum priority: doesn't update the unread count.
  AddPriorityNotification(MIN_PRIORITY);
  EXPECT_EQ(kMaxVisiblePopupNotifications + 3,
            notification_list_->NotificationCount(blockers_));
  EXPECT_EQ(0u, GetPopupCounts());

  NotificationList::Notifications notifications =
      notification_list_->GetVisibleNotifications(blockers_);
  for (auto iter = notifications.begin(); iter != notifications.end(); ++iter) {
    notification_list_->RemoveNotification((*iter)->id());
  }

  // Higher priority: no limits to the number of popups.
  for (size_t i = 0; i < kMaxVisiblePopupNotifications * 2; ++i)
    AddPriorityNotification(HIGH_PRIORITY);
  for (size_t i = 0; i < kMaxVisiblePopupNotifications * 2; ++i)
    AddPriorityNotification(MAX_PRIORITY);
  EXPECT_EQ(kMaxVisiblePopupNotifications * 4,
            notification_list_->NotificationCount(blockers_));
  EXPECT_EQ(kMaxVisiblePopupNotifications * 4, GetPopupCounts());
}

// Tests that GetNotificationsByAppId returns notifications regardless of their
// visibility.
TEST_F(NotificationListTest, GetNotificationsByAppId) {
  // Add a notification for |app_id1|.
  const std::string app_id1("app_id1");
  const std::string id1("id1");
  std::unique_ptr<Notification> notification(
      new Notification(NOTIFICATION_TYPE_PROGRESS, id1, UTF8ToUTF16("updated"),
                       UTF8ToUTF16("updated"), gfx::Image(), base::string16(),
                       GURL(), NotifierId(NotifierId::APPLICATION, app_id1),
                       RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(notification));
  EXPECT_EQ(1u, notification_list_->GetNotificationsByAppId(app_id1).size());

  // Mark the popup as shown but not read.
  notification_list_->MarkSinglePopupAsShown(id1, false);
  EXPECT_EQ(1u, notification_list_->GetNotificationsByAppId(app_id1).size());

  // Mark the popup as shown and read.
  notification_list_->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(1u, notification_list_->GetNotificationsByAppId(app_id1).size());

  // Remove the notification.
  notification_list_->RemoveNotification(id1);
  EXPECT_EQ(0u, notification_list_->GetNotificationsByAppId(app_id1).size());

  // Add two notifications for |app_id1| and one for |app_id2|.
  notification.reset(
      new Notification(NOTIFICATION_TYPE_PROGRESS, id1, UTF8ToUTF16("updated"),
                       UTF8ToUTF16("updated"), gfx::Image(), base::string16(),
                       GURL(), NotifierId(NotifierId::APPLICATION, app_id1),
                       RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(notification));

  const std::string id2("id2");
  notification.reset(
      new Notification(NOTIFICATION_TYPE_PROGRESS, id2, UTF8ToUTF16("updated"),
                       UTF8ToUTF16("updated"), gfx::Image(), base::string16(),
                       GURL(), NotifierId(NotifierId::APPLICATION, app_id1),
                       RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(notification));
  EXPECT_EQ(2u, notification_list_->GetNotificationsByAppId(app_id1).size());

  const std::string id3("id3");
  const std::string app_id2("app_id2");
  notification.reset(
      new Notification(NOTIFICATION_TYPE_PROGRESS, id3, UTF8ToUTF16("updated"),
                       UTF8ToUTF16("updated"), gfx::Image(), base::string16(),
                       GURL(), NotifierId(NotifierId::APPLICATION, app_id2),
                       RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(notification));
  EXPECT_EQ(2u, notification_list_->GetNotificationsByAppId(app_id1).size());
  EXPECT_EQ(1u, notification_list_->GetNotificationsByAppId(app_id2).size());

  for (std::string app_id : {app_id1, app_id2}) {
    for (auto* notification :
         notification_list_->GetNotificationsByAppId(app_id)) {
      EXPECT_EQ(app_id, notification->notifier_id().id);
    }
  }
}

TEST_F(NotificationListTest, HasPopupsWithPriority) {
  ASSERT_EQ(0u, notification_list_->NotificationCount(blockers_));

  AddPriorityNotification(MIN_PRIORITY);
  AddPriorityNotification(MAX_PRIORITY);

  EXPECT_EQ(1u, GetPopupCounts());
}

TEST_F(NotificationListTest, HasPopupsWithSystemPriority) {
  ASSERT_EQ(0u, notification_list_->NotificationCount(blockers_));

  std::string normal_id = AddPriorityNotification(DEFAULT_PRIORITY);
  std::string system_id = AddNotification();
  GetNotification(system_id)->SetSystemPriority();

  EXPECT_EQ(2u, GetPopupCounts());

  notification_list_->MarkSinglePopupAsDisplayed(normal_id);
  notification_list_->MarkSinglePopupAsDisplayed(system_id);

  notification_list_->MarkSinglePopupAsShown(normal_id, false);
  notification_list_->MarkSinglePopupAsShown(system_id, false);

  notification_list_->SetNotificationsShown(blockers_, NULL);
  EXPECT_EQ(1u, GetPopupCounts());

  // Mark as read -- emulation of mouse click.
  notification_list_->MarkSinglePopupAsShown(system_id, true);
  EXPECT_EQ(0u, GetPopupCounts());
}

// Verifies that notification updates will re-show the toast when there is no
// message center view (i.e. the bubble anchored to the status bar).
TEST_F(NotificationListTest, UpdateWithoutMessageCenterView) {
  auto run_test = [this](bool has_message_center_view) {
    message_center_->SetHasMessageCenterView(has_message_center_view);
    std::string id0 = AddNotification();
    std::string replaced = id0 + "_replaced";
    notification_list_->MarkSinglePopupAsShown(id0, false);
    EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));
    EXPECT_EQ(0u, GetPopupCounts());

    RichNotificationData optional;
    std::unique_ptr<Notification> notification(new Notification(
        NOTIFICATION_TYPE_SIMPLE, replaced, UTF8ToUTF16("newtitle"),
        UTF8ToUTF16("newbody"), gfx::Image(), UTF8ToUTF16(kDisplaySource),
        GURL(), NotifierId(NotifierId::APPLICATION, kExtensionId), optional,
        NULL));
    notification_list_->UpdateNotificationMessage(id0, std::move(notification));
    EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));
    EXPECT_EQ(has_message_center_view ? 0U : 1U, GetPopupCounts());
    const NotificationList::Notifications notifications =
        notification_list_->GetVisibleNotifications(blockers_);
    EXPECT_EQ(replaced, (*notifications.begin())->id());
    EXPECT_EQ(UTF8ToUTF16("newtitle"), (*notifications.begin())->title());
    EXPECT_EQ(UTF8ToUTF16("newbody"), (*notifications.begin())->message());

    notification_list_->RemoveNotification(replaced);
    EXPECT_EQ(0U,
              notification_list_->GetVisibleNotifications(blockers_).size());
  };

  run_test(false);
  run_test(true);
}

TEST_F(NotificationListTest, Renotify) {
  std::string id0 = AddNotification();
  std::string replaced = id0 + "_replaced";
  notification_list_->MarkSinglePopupAsShown(id0, false);
  EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));
  EXPECT_EQ(0u, GetPopupCounts());
  RichNotificationData optional;
  optional.renotify = true;
  std::unique_ptr<Notification> notification(new Notification(
      NOTIFICATION_TYPE_SIMPLE, replaced, UTF8ToUTF16("newtitle"),
      UTF8ToUTF16("newbody"), gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(NotifierId::APPLICATION, kExtensionId), optional, NULL));
  notification_list_->UpdateNotificationMessage(id0, std::move(notification));
  EXPECT_EQ(1u, notification_list_->NotificationCount(blockers_));
  EXPECT_EQ(1u, GetPopupCounts());
  const NotificationList::Notifications notifications =
      notification_list_->GetVisibleNotifications(blockers_);
  EXPECT_EQ(replaced, (*notifications.begin())->id());
  EXPECT_EQ(UTF8ToUTF16("newtitle"), (*notifications.begin())->title());
  EXPECT_EQ(UTF8ToUTF16("newbody"), (*notifications.begin())->message());
}

TEST_F(NotificationListTest, PriorityAndRenotify) {
  std::string id0 = AddPriorityNotification(LOW_PRIORITY);
  std::string id1 = AddPriorityNotification(DEFAULT_PRIORITY);
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list_->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(0u, GetPopupCounts());

  // id0 promoted to LOW->DEFAULT, it'll appear as toast (popup).
  RichNotificationData priority;
  priority.priority = DEFAULT_PRIORITY;
  std::unique_ptr<Notification> notification(new Notification(
      NOTIFICATION_TYPE_SIMPLE, id0, UTF8ToUTF16("newtitle"),
      UTF8ToUTF16("newbody"), gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(NotifierId::APPLICATION, kExtensionId), priority, NULL));
  notification_list_->UpdateNotificationMessage(id0, std::move(notification));
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list_->MarkSinglePopupAsShown(id0, true);
  EXPECT_EQ(0u, GetPopupCounts());

  // update with no promotion change for id0, it won't appear as a toast.
  notification.reset(new Notification(
      NOTIFICATION_TYPE_SIMPLE, id0, UTF8ToUTF16("newtitle2"),
      UTF8ToUTF16("newbody2"), gfx::Image(), UTF8ToUTF16(kDisplaySource),
      GURL(), NotifierId(NotifierId::APPLICATION, kExtensionId), priority,
      NULL));
  notification_list_->UpdateNotificationMessage(id0, std::move(notification));
  EXPECT_EQ(0u, GetPopupCounts());

  // id1 promoted to DEFAULT->HIGH, it won't reappear as a toast (popup).
  priority.priority = HIGH_PRIORITY;
  notification.reset(new Notification(
      NOTIFICATION_TYPE_SIMPLE, id1, UTF8ToUTF16("newtitle"),
      UTF8ToUTF16("newbody"), gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(NotifierId::APPLICATION, kExtensionId), priority, NULL));
  notification_list_->UpdateNotificationMessage(id1, std::move(notification));
  EXPECT_EQ(0u, GetPopupCounts());

  // |renotify| will make it reappear as a toast (popup).
  priority.renotify = true;
  notification.reset(new Notification(
      NOTIFICATION_TYPE_SIMPLE, id1, UTF8ToUTF16("newtitle"),
      UTF8ToUTF16("newbody"), gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(NotifierId::APPLICATION, kExtensionId), priority, NULL));
  notification_list_->UpdateNotificationMessage(id1, std::move(notification));
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list_->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(0u, GetPopupCounts());
}

TEST_F(NotificationListTest, NotificationOrderAndPriority) {
  base::Time now = base::Time::Now();
  RichNotificationData optional;
  optional.timestamp = now;
  optional.priority = 2;
  std::string max_id = AddNotification(optional);

  now += base::TimeDelta::FromSeconds(1);
  optional.timestamp = now;
  optional.priority = 1;
  std::string high_id = AddNotification(optional);

  now += base::TimeDelta::FromSeconds(1);
  optional.timestamp = now;
  optional.priority = 0;
  std::string default_id = AddNotification(optional);

  {
    // Popups: latest comes first.
    NotificationList::PopupNotifications popups = GetPopups();
    EXPECT_EQ(3u, popups.size());
    auto iter = popups.begin();
    EXPECT_EQ(default_id, (*iter)->id());
    iter++;
    EXPECT_EQ(high_id, (*iter)->id());
    iter++;
    EXPECT_EQ(max_id, (*iter)->id());
  }
  {
    // Notifications: high priority comes earlier.
    const NotificationList::Notifications notifications =
        notification_list_->GetVisibleNotifications(blockers_);
    EXPECT_EQ(3u, notifications.size());
    auto iter = notifications.begin();
    EXPECT_EQ(max_id, (*iter)->id());
    iter++;
    EXPECT_EQ(high_id, (*iter)->id());
    iter++;
    EXPECT_EQ(default_id, (*iter)->id());
  }
}

TEST_F(NotificationListTest, MarkSinglePopupAsShown) {
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  std::string id3 = AddNotification();
  ASSERT_EQ(3u, notification_list_->NotificationCount(blockers_));
  ASSERT_EQ(std::min(static_cast<size_t>(3u), kMaxVisiblePopupNotifications),
            GetPopupCounts());
  notification_list_->MarkSinglePopupAsDisplayed(id1);
  notification_list_->MarkSinglePopupAsDisplayed(id2);
  notification_list_->MarkSinglePopupAsDisplayed(id3);

  notification_list_->MarkSinglePopupAsShown(id2, true);
  notification_list_->MarkSinglePopupAsShown(id3, false);
  EXPECT_EQ(3u, notification_list_->NotificationCount(blockers_));
  EXPECT_EQ(1u, GetPopupCounts());
  NotificationList::PopupNotifications popups = GetPopups();
  EXPECT_EQ(id1, (*popups.begin())->id());

  // The notifications in the NotificationCenter are unaffected by popups shown.
  NotificationList::Notifications notifications =
      notification_list_->GetVisibleNotifications(blockers_);
  auto iter = notifications.begin();
  EXPECT_EQ(id3, (*iter)->id());
  iter++;
  EXPECT_EQ(id2, (*iter)->id());
  iter++;
  EXPECT_EQ(id1, (*iter)->id());
}

TEST_F(NotificationListTest, UpdateAfterMarkedAsShown) {
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  notification_list_->MarkSinglePopupAsDisplayed(id1);
  notification_list_->MarkSinglePopupAsDisplayed(id2);

  EXPECT_EQ(2u, GetPopupCounts());

  NotificationState n1_state = GetNotificationState(id1);
  EXPECT_FALSE(n1_state.shown_as_popup);
  EXPECT_TRUE(n1_state.is_read);

  notification_list_->MarkSinglePopupAsShown(id1, true);

  n1_state = GetNotificationState(id1);
  EXPECT_TRUE(n1_state.shown_as_popup);
  EXPECT_TRUE(n1_state.is_read);

  const std::string replaced("test-replaced-id");
  std::unique_ptr<Notification> notification(new Notification(
      NOTIFICATION_TYPE_SIMPLE, replaced, UTF8ToUTF16("newtitle"),
      UTF8ToUTF16("newbody"), gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(NotifierId::APPLICATION, kExtensionId), RichNotificationData(),
      NULL));
  notification_list_->UpdateNotificationMessage(id1, std::move(notification));
  Notification* n1 = GetNotification(id1);
  EXPECT_TRUE(n1 == NULL);
  const NotificationState nr_state = GetNotificationState(replaced);
  EXPECT_TRUE(nr_state.shown_as_popup);
  EXPECT_TRUE(nr_state.is_read);
}

TEST_F(NotificationListTest, QuietMode) {
  notification_list_->SetQuietMode(true);
  AddNotification();
  AddPriorityNotification(HIGH_PRIORITY);
  AddPriorityNotification(MAX_PRIORITY);
  EXPECT_EQ(3u, notification_list_->NotificationCount(blockers_));
  EXPECT_EQ(0u, GetPopupCounts());

  notification_list_->SetQuietMode(false);
  AddNotification();
  EXPECT_EQ(4u, notification_list_->NotificationCount(blockers_));
  EXPECT_EQ(1u, GetPopupCounts());

  // TODO(mukai): Add test of quiet mode with expiration.
}

TEST_F(NotificationListTest, TestHasNotificationOfType) {
  std::string id = AddNotification();

  EXPECT_TRUE(
      notification_list_->HasNotificationOfType(id, NOTIFICATION_TYPE_SIMPLE));
  EXPECT_FALSE(notification_list_->HasNotificationOfType(
      id, NOTIFICATION_TYPE_PROGRESS));

  std::unique_ptr<Notification> updated_notification(
      new Notification(NOTIFICATION_TYPE_PROGRESS, id, UTF8ToUTF16("updated"),
                       UTF8ToUTF16("updated"), gfx::Image(), base::string16(),
                       GURL(), NotifierId(), RichNotificationData(), NULL));
  notification_list_->AddNotification(std::move(updated_notification));

  EXPECT_FALSE(
      notification_list_->HasNotificationOfType(id, NOTIFICATION_TYPE_SIMPLE));
  EXPECT_TRUE(notification_list_->HasNotificationOfType(
      id, NOTIFICATION_TYPE_PROGRESS));
}

}  // namespace message_center
