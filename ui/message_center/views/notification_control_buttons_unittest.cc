// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_control_buttons_view.h"

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

namespace message_center {

namespace {

class TestMessageView : public MessageView {
 public:
  explicit TestMessageView(const Notification& notification)
      : MessageView(notification),
        buttons_view_(std::make_unique<NotificationControlButtonsView>(this)) {}

  NotificationControlButtonsView* GetControlButtonsView() const override {
    return buttons_view_.get();
  }

 private:
  std::unique_ptr<NotificationControlButtonsView> buttons_view_;
};

}  // namespace

class NotificationControlButtonsTest : public testing::Test {
 public:
  NotificationControlButtonsTest() = default;
  ~NotificationControlButtonsTest() override = default;

  // testing::Test
  void SetUp() override {
    Test::SetUp();
    Notification notification(
        NOTIFICATION_TYPE_SIMPLE, "id", base::UTF8ToUTF16("title"),
        base::UTF8ToUTF16("id"), gfx::Image(), base::string16(), GURL(),
        NotifierId(NotifierType::APPLICATION, "notifier_id"),
        RichNotificationData(), nullptr);
    message_view_ = std::make_unique<TestMessageView>(notification);
  }

  NotificationControlButtonsView* buttons_view() {
    return message_view_->GetControlButtonsView();
  }

  bool MatchesIcon(PaddedButton* button,
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
  std::unique_ptr<TestMessageView> message_view_;

  DISALLOW_COPY_AND_ASSIGN(NotificationControlButtonsTest);
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
  ExpectIconColor(gfx::kChromeIconGrey);

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
