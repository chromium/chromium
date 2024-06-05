// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "ui/message_center/views/message_view.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_utils.h"

namespace message_center {

class NotificationControlButtonsView;

class TestMessageView : public MessageView {
 public:
  explicit TestMessageView(const Notification& notification)
      : MessageView(notification) {
    SetLayoutManager(std::make_unique<views::BoxLayout>());
    AddChildView(std::make_unique<views::Label>())
        ->SetText(notification.title());
    AddChildView((std::make_unique<views::Label>()))
        ->SetText(notification.message());
  }
  TestMessageView(const TestMessageView&) = delete;
  TestMessageView& operator=(const TestMessageView&) = delete;
  ~TestMessageView() override = default;

  // MessageView:
  MOCK_METHOD(void, UpdateControlButtonsVisibility, ());
  NotificationControlButtonsView* GetControlButtonsView() const override {
    return nullptr;
  }
};

class MessageViewTest : public views::ViewsTestBase {
 public:
  MessageViewTest() = default;
  MessageViewTest(const MessageViewTest&) = delete;
  MessageViewTest& operator=(const MessageViewTest&) = delete;
  ~MessageViewTest() override = default;

  // Overridden from ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    MessageCenter::Initialize();
    notification_ = std::make_unique<Notification>(
        NOTIFICATION_TYPE_SIMPLE, "id", u"title", u"test message",
        ui::ImageModel(), /*display_source=*/std::u16string(), GURL(),
        NotifierId(), RichNotificationData(), /*delegate=*/nullptr);

    // `widget_` owns `message_view_`.
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    message_view_ = widget_->SetContentsView(
        std::make_unique<TestMessageView>(*notification_.get()));
    widget_->Show();
  }

  void TearDown() override {
    message_view_ = nullptr;
    widget_.reset();
    MessageCenter::Shutdown();
    views::ViewsTestBase::TearDown();
  }

 protected:
  TestMessageView* message_view() { return message_view_.get(); }

 private:
  raw_ptr<TestMessageView> message_view_;
  std::unique_ptr<Notification> notification_;
  std::unique_ptr<views::Widget> widget_;
};

// Updating control buttons visibility is a ChromeOS only feature.
#if BUILDFLAG(IS_CHROMEOS_ASH)
// Make sure UpdateControlButtonsVisibility is called when their is a mouse
// enter or exit on a MessageView.
TEST_F(MessageViewTest, UpdateControlButtonsVisibilityCalled) {
  ui::test::EventGenerator event_generator(
      GetRootWindow(message_view()->GetWidget()));

  // Expect a call on the mouse entering the message view.
  EXPECT_CALL(*message_view(), UpdateControlButtonsVisibility);
  event_generator.MoveMouseTo(message_view()->GetBoundsInScreen().CenterPoint(),
                              10);

  // Expect a call on the mouse exiting the message view.
  EXPECT_CALL(*message_view(), UpdateControlButtonsVisibility);
  event_generator.MoveMouseTo(
      message_view()->GetBoundsInScreen().origin() - gfx::Vector2d(10, 10), 10);
}
#endif  // IS_CHROMEOS_ASH

}  // namespace message_center
