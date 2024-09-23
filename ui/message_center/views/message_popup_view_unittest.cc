// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_view.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/desktop_message_popup_collection.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace message_center {

namespace {

class MockMessagePopupView;

class TestMessageView : public MessageView {
 public:
  explicit TestMessageView(const Notification& notification)
      : MessageView(notification) {}
  TestMessageView(const TestMessageView&) = delete;
  TestMessageView& operator=(const TestMessageView&) = delete;
  ~TestMessageView() override = default;

  NotificationControlButtonsView* GetControlButtonsView() const override {
    return nullptr;
  }
};

class MockMessagePopupCollection : public DesktopMessagePopupCollection {
 public:
  MockMessagePopupCollection() = default;

  MockMessagePopupCollection(const MockMessagePopupCollection&) = delete;
  MockMessagePopupCollection& operator=(const MockMessagePopupCollection&) =
      delete;

  ~MockMessagePopupCollection() override = default;

  MessagePopupView* CreatePopup(const Notification& notification) override;
};

class MockMessagePopupView : public MessagePopupView {
 public:
  MockMessagePopupView(MockMessagePopupCollection* popup_collection,
                       MessageView* message_view)
      : MessagePopupView(message_view, popup_collection, false) {}

  ~MockMessagePopupView() override = default;

  void Close() override { delete this; }
};

MessagePopupView* MockMessagePopupCollection::CreatePopup(
    const Notification& notification) {
  TestMessageView* message_view = new TestMessageView(notification);
  auto* popup = new MockMessagePopupView(this, message_view);
  return popup;
}

}  // namespace

class MessagePopupViewTest : public views::ViewsTestBase {
 public:
  MessagePopupViewTest() = default;

  MessagePopupViewTest(const MessagePopupViewTest&) = delete;
  MessagePopupViewTest& operator=(const MessagePopupViewTest&) = delete;

  ~MessagePopupViewTest() override = default;

  // Overridden from ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    MessageCenter::Initialize();
    MessageCenter::Get()->DisableTimersForTest();

    notification_ = std::make_unique<Notification>(
        NOTIFICATION_TYPE_SIMPLE, "id", u"title", u"test message",
        ui::ImageModel(), /*display_source=*/std::u16string(), GURL(),
        NotifierId(), RichNotificationData(), /*delegate=*/nullptr);
  }

  Notification& GetNotification() { return *notification_.get(); }

  void TearDown() override {
    notification_.reset();

    MessageCenter::Get()->RemoveAllNotifications(
        false /* by_user */, MessageCenter::RemoveType::ALL);
    MessageCenter::Shutdown();
    views::ViewsTestBase::TearDown();
  }

 private:
  std::unique_ptr<Notification> notification_ = nullptr;
};

TEST_F(MessagePopupViewTest, AccessibleAttributes) {
  MockMessagePopupCollection popup_collection;
  MessagePopupView* popup = popup_collection.CreatePopup(GetNotification());

  ui::AXNodeData data;
  popup->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kAlertDialog);
  popup->Close();
}

}  // namespace message_center
