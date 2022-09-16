// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_control_buttons_view.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {

class TestMessageView : public MessageView {
 public:
  explicit TestMessageView(const Notification& notification)
      : MessageView(notification) {}

  NotificationControlButtonsView* GetControlButtonsView() const override {
    return buttons_view_;
  }

  void set_control_buttons_view(NotificationControlButtonsView* buttons_view) {
    buttons_view_ = buttons_view;
  }

 private:
  raw_ptr<NotificationControlButtonsView> buttons_view_ = nullptr;
};

}  // namespace

class NotificationControlButtonsTest : public views::ViewsTestBase {
 public:
  NotificationControlButtonsTest() = default;

  NotificationControlButtonsTest(const NotificationControlButtonsTest&) =
      delete;
  NotificationControlButtonsTest& operator=(
      const NotificationControlButtonsTest&) = delete;

  ~NotificationControlButtonsTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    Notification notification(
        NOTIFICATION_TYPE_SIMPLE, "id", u"title", u"id", ui::ImageModel(),
        std::u16string(), GURL(),
        NotifierId(NotifierType::APPLICATION, "notifier_id"),
        RichNotificationData(), nullptr);
    message_view_ = std::make_unique<TestMessageView>(notification);
    message_view_->set_control_buttons_view(widget_->SetContentsView(
        std::make_unique<NotificationControlButtonsView>(message_view_.get())));
  }

  void TearDown() override {
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  NotificationControlButtonsView* buttons_view() {
    return message_view_->GetControlButtonsView();
  }

  bool MatchesIcon(views::ImageButton* button,
                   const gfx::VectorIcon& icon,
                   SkColor color) {
    SkBitmap expected = *gfx::CreateVectorIcon(icon, color).bitmap();
    SkBitmap actual = *button->GetImage(views::Button::STATE_NORMAL).bitmap();
    return gfx::test::AreBitmapsEqual(expected, actual);
  }

  void ExpectIconColor(SkColor color) {
    EXPECT_TRUE(MatchesIcon(buttons_view()->close_button(),
                            kNotificationCloseButtonIcon, color));
    EXPECT_TRUE(MatchesIcon(buttons_view()->settings_button(),
                            kNotificationSettingsButtonIcon, color));
    EXPECT_TRUE(MatchesIcon(buttons_view()->snooze_button(),
                            kNotificationSnoozeButtonIcon, color));
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<TestMessageView> message_view_;
};

TEST_F(NotificationControlButtonsTest, TestShowAndHideButtons) {
  EXPECT_EQ(nullptr, buttons_view()->close_button());
  EXPECT_EQ(nullptr, buttons_view()->settings_button());
  EXPECT_EQ(nullptr, buttons_view()->snooze_button());

  buttons_view()->ShowCloseButton(true);
  buttons_view()->ShowSettingsButton(true);
  buttons_view()->ShowSnoozeButton(true);

  EXPECT_NE(nullptr, buttons_view()->close_button());
  EXPECT_NE(nullptr, buttons_view()->settings_button());
  EXPECT_NE(nullptr, buttons_view()->snooze_button());

  buttons_view()->ShowCloseButton(false);
  buttons_view()->ShowSettingsButton(false);
  buttons_view()->ShowSnoozeButton(false);

  EXPECT_EQ(nullptr, buttons_view()->close_button());
  EXPECT_EQ(nullptr, buttons_view()->settings_button());
  EXPECT_EQ(nullptr, buttons_view()->snooze_button());
}

TEST_F(NotificationControlButtonsTest, IconColor_NoContrastEnforcement) {
  buttons_view()->ShowCloseButton(true);
  buttons_view()->ShowSettingsButton(true);
  buttons_view()->ShowSnoozeButton(true);

  // Default icon color.
  ExpectIconColor(buttons_view()->GetColorProvider()->GetColor(ui::kColorIcon));

  // Without setting a background color we won't enforce contrast ratios.
  buttons_view()->SetButtonIconColors(SK_ColorWHITE);
  ExpectIconColor(SK_ColorWHITE);
  buttons_view()->SetButtonIconColors(SK_ColorBLACK);
  ExpectIconColor(SK_ColorBLACK);
}

TEST_F(NotificationControlButtonsTest, IconColor_ContrastEnforcement) {
  buttons_view()->ShowCloseButton(true);
  buttons_view()->ShowSettingsButton(true);
  buttons_view()->ShowSnoozeButton(true);

  // A bright background should enforce dark enough icons.
  buttons_view()->SetBackgroundColor(SK_ColorWHITE);
  buttons_view()->SetButtonIconColors(SK_ColorWHITE);
  ExpectIconColor(
      color_utils::BlendForMinContrast(SK_ColorWHITE, SK_ColorWHITE).color);

  // A dark background should enforce bright enough icons.
  buttons_view()->SetBackgroundColor(SK_ColorBLACK);
  buttons_view()->SetButtonIconColors(SK_ColorBLACK);
  ExpectIconColor(
      color_utils::BlendForMinContrast(SK_ColorBLACK, SK_ColorBLACK).color);
}

}  // namespace message_center
