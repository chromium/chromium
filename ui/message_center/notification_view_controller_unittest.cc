// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_view_controller.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_view.h"

namespace message_center {

using NotificationState = NotificationList::NotificationState;

class MockMessageView : public MessageView {
 public:
  explicit MockMessageView(const Notification& notification)
      : MessageView(notification) {}

  NotificationControlButtonsView* GetControlButtonsView() const override {
    return nullptr;
  }
};

class MockNotificationViewController : public NotificationViewController {
 public:
  MessageView* GetMessageViewForNotificationId(
      const std::string& notification_id) override {
    auto it = std::find_if(fake_message_views_.begin(),
                           fake_message_views_.end(), [&](const auto& child) {
                             return child->notification_id() == notification_id;
                           });

    if (it == fake_message_views_.end())
      return nullptr;

    return it->get();
  }
  void ConvertNotificationViewToGroupedNotificationView(
      const std::string& ungrouped_notification_id,
      const std::string& new_grouped_notification_id) override {}

  void ConvertGroupedNotificationViewToNotificationView(
      const std::string& ungrouped_notification_id,
      const std::string& new_grouped_notification_id) override {}

  void OnNotificationAdded(const std::string& notification_id) override {
    NotificationViewController::OnNotificationAdded(notification_id);
    auto* notification =
        MessageCenter::Get()->FindNotificationById(notification_id);
    fake_message_views_.push_back(
        std::make_unique<MockMessageView>(*notification));

    if (!notification->group_child())
      return;

    std::string parent_id = GetParentIdForChildForTest(notification_id);
    auto it = std::find_if(
        fake_message_views_.begin(), fake_message_views_.end(),
        [&](const auto& view) { return view->notification_id() == parent_id; });
    if (it == fake_message_views_.end()) {
      fake_message_views_.push_back(std::make_unique<MockMessageView>(
          *MessageCenter::Get()->FindNotificationById(parent_id)));
    }
  }

  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override {
    RemoveNotificationViewById(notification_id);
    NotificationViewController::OnNotificationRemoved(notification_id, by_user);
  }

  void RemoveNotificationViewById(const std::string& id) {
    auto it = std::find_if(
        fake_message_views_.begin(), fake_message_views_.end(),
        [&](const auto& view) { return view->notification_id() == id; });
    if (it != fake_message_views_.end())
      fake_message_views_.erase(it);
  }

 private:
  std::vector<std::unique_ptr<MockMessageView>> fake_message_views_;
};

class NotificationViewControllerTest : public testing::Test {
 public:
  NotificationViewControllerTest() = default;
  NotificationViewControllerTest(const NotificationViewController& other) =
      delete;
  NotificationViewControllerTest& operator=(
      const NotificationViewController& other) = delete;
  ~NotificationViewControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kNotificationsRefresh);

    MessageCenter::Initialize();
    // We do not want to wait for notification timeouts
    // in unit tests.
    MessageCenter::Get()->DisableTimersForTest();

    notification_view_controller_ =
        std::make_unique<MockNotificationViewController>();
  }

 protected:
  std::string AddNotificationWithNotifierId(std::string notifier_id) {
    std::string id;
    MessageCenter::Get()->AddNotification(MakeNotification(id, notifier_id));
    return id;
  }

  Notification* GetPopupNotification(const std::string& id) {
    auto popups = MessageCenter::Get()->GetPopupNotifications();
    auto it =
        std::find_if(popups.begin(), popups.end(),
                     [&](const auto& popup) { return popup->id() == id; });
    if (it == popups.end())
      return nullptr;

    return *it;
  }

  void RemoveAndMarkPopupAsShown(std::string& id) {
    notification_view_controller_->RemoveNotificationViewById(id);
    MessageCenter::Get()->MarkSinglePopupAsShown(id, false);
  }

  // Construct a new notification for testing.
  std::unique_ptr<Notification> MakeNotification(std::string& id_out,
                                                 std::string notifier_id) {
    id_out = base::StringPrintf(kIdFormat, notifications_counter_);
    auto notification = std::make_unique<Notification>(
        NOTIFICATION_TYPE_SIMPLE, id_out,
        u"id" + base::NumberToString16(notifications_counter_),
        u"message" + base::NumberToString16(notifications_counter_),
        gfx::Image(), u"src", GURL(),
        NotifierId(NotifierType::APPLICATION, notifier_id),
        RichNotificationData(), nullptr);
    notifications_counter_++;
    return notification;
  }

  static const char kIdFormat[];

  std::unique_ptr<MockNotificationViewController> notification_view_controller_;

  base::test::ScopedFeatureList scoped_feature_list_;

  size_t notifications_counter_ = 0;
};

const char NotificationViewControllerTest::kIdFormat[] = "id%ld";

TEST_F(NotificationViewControllerTest, BasicGrouping) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);
  id1 = AddNotificationWithNotifierId(group_id);
  id2 = AddNotificationWithNotifierId(group_id);

  EXPECT_TRUE(message_center->FindNotificationById(id0)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id1)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id2)->group_child());

  std::string id_parent = id0 + kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(message_center->FindNotificationById(id_parent)->group_parent());
}

TEST_F(NotificationViewControllerTest, BasicRemoval) {
  std::string id0, id1, id2;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);
  id1 = AddNotificationWithNotifierId(group_id);
  id2 = AddNotificationWithNotifierId(group_id);

  std::string id_parent = id0 + kIdSuffixForGroupContainerNotification;
  // Group notification should stay intact if a single group notification is
  // removed.
  MessageCenter::Get()->RemoveNotification(id1, true);
  EXPECT_TRUE(
      MessageCenter::Get()->FindNotificationById(id_parent)->group_parent());

  // Adding and removing a non group notification should have no impact.
  std::string tmp = AddNotificationWithNotifierId("tmp");
  MessageCenter::Get()->RemoveNotification(tmp, true);

  EXPECT_TRUE(MessageCenter::Get()->FindNotificationById(id0)->group_child());
  EXPECT_TRUE(MessageCenter::Get()->FindNotificationById(id2)->group_child());
  EXPECT_TRUE(
      MessageCenter::Get()->FindNotificationById(id_parent)->group_parent());
}

TEST_F(NotificationViewControllerTest, ParentNotificationReshownWithNewChild) {
  std::string id0;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);

  std::string tmp;
  tmp = AddNotificationWithNotifierId(group_id);

  std::string parent_id = id0 + kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(GetPopupNotification(parent_id));
  RemoveAndMarkPopupAsShown(parent_id);
  EXPECT_FALSE(GetPopupNotification(parent_id));

  // Adding notification with a different notifier id should have no effect.
  AddNotificationWithNotifierId("tmp");
  EXPECT_FALSE(GetPopupNotification(parent_id));

  AddNotificationWithNotifierId(group_id);
  EXPECT_TRUE(GetPopupNotification(parent_id));
}

TEST_F(NotificationViewControllerTest,
       RemovingParentRemovesChildGroupNotifications) {
  std::string id0;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);

  std::string tmp;
  AddNotificationWithNotifierId(group_id);
  AddNotificationWithNotifierId(group_id);

  MessageCenter::Get()->RemoveNotification(
      id0 + kIdSuffixForGroupContainerNotification, true);

  ASSERT_FALSE(MessageCenter::Get()->HasPopupNotifications());
}

TEST_F(NotificationViewControllerTest,
       ConvertingGroupedNotificationToSingleNotificationAndBack) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);
  id1 = AddNotificationWithNotifierId(group_id);
  id2 = AddNotificationWithNotifierId(group_id);

  std::string parent_id = id0 + kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(
      MessageCenter::Get()->FindNotificationById(parent_id)->group_parent());

  // Removing all but 1 notification should convert it back to a single
  // notification and result in the removal of the parent notification.
  message_center->RemoveNotification(id0, true);
  message_center->RemoveNotification(id1, true);

  auto* single_notification = message_center->FindNotificationById(id2);
  EXPECT_FALSE(single_notification->group_child() ||
               single_notification->group_parent());
  EXPECT_FALSE(message_center->FindNotificationById(parent_id));

  // Adding further notifications should create a new group with the parent id
  // being derived from `id2`.
  id0 = AddNotificationWithNotifierId(group_id);
  id1 = AddNotificationWithNotifierId(group_id);

  parent_id = id2 + kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(message_center->FindNotificationById(parent_id));
}

TEST_F(NotificationViewControllerTest,
       ConvertingRepopulatedParentToSingleNotification) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2, id3, id4;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);
  id1 = AddNotificationWithNotifierId(group_id);
  id2 = AddNotificationWithNotifierId(group_id);
  id3 = AddNotificationWithNotifierId(group_id);

  std::string parent_id = id0 + kIdSuffixForGroupContainerNotification;
  RemoveAndMarkPopupAsShown(parent_id);

  id4 = AddNotificationWithNotifierId(group_id);

  message_center->RemoveNotification(id0, true);
  message_center->RemoveNotification(id1, true);
  message_center->RemoveNotification(id2, true);
  message_center->RemoveNotification(id3, true);

  auto* single_notification = MessageCenter::Get()->FindNotificationById(id4);
  EXPECT_FALSE(single_notification->group_child() ||
               single_notification->group_parent());
  EXPECT_FALSE(MessageCenter::Get()->FindNotificationById(parent_id));
}

}  // namespace message_center
