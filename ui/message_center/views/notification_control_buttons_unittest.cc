// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_control_buttons_view.h"

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
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
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
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
    message_view_->set_control_buttons_view(nullptr);
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  NotificationControlButtonsView* buttons_view() {
    return message_view_->GetControlButtonsView();
  }

  bool MatchesIcon(views::ImageButton* button,
                   const gfx::VectorIcon& icon,
                   SkColor color,
                   int size = 0) {
    SkBitmap expected = *gfx::CreateVectorIcon(icon, size, color).bitmap();
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

// Tests default and custom control button icons.
TEST_F(NotificationControlButtonsTest, SetIcons) {
  // Show the default control buttons.
  buttons_view()->ShowCloseButton(true);
  buttons_view()->ShowSettingsButton(true);
  buttons_view()->ShowSnoozeButton(true);

  // Verify that the default control buttons are using their default icons.
  const SkColor default_icon_color =
      buttons_view()->GetColorProvider()->GetColor(ui::kColorIcon);
  EXPECT_TRUE(MatchesIcon(buttons_view()->close_button(),
                          NotificationControlButtonsView::kDefaultCloseIcon,
                          default_icon_color));
  EXPECT_TRUE(MatchesIcon(buttons_view()->settings_button(),
                          NotificationControlButtonsView::kDefaultSettingsIcon,
                          default_icon_color));
  EXPECT_TRUE(MatchesIcon(buttons_view()->snooze_button(),
                          NotificationControlButtonsView::kDefaultSnoozeIcon,
                          default_icon_color));

  // Set the control buttons to have custom, non-default icons.
  buttons_view()->ShowCloseButton(false);
  buttons_view()->ShowSettingsButton(false);
  buttons_view()->ShowSnoozeButton(false);
  const gfx::VectorIcon& test_icon = kProductIcon;
  buttons_view()->SetCloseButtonIcon(test_icon);
  buttons_view()->SetSettingsButtonIcon(test_icon);
  buttons_view()->SetSnoozeButtonIcon(test_icon);
  buttons_view()->ShowCloseButton(true);
  buttons_view()->ShowSettingsButton(true);
  buttons_view()->ShowSnoozeButton(true);

  // Verify that the control buttons are now using the custom icons.
  EXPECT_TRUE(MatchesIcon(buttons_view()->close_button(), test_icon,
                          default_icon_color));
  EXPECT_TRUE(MatchesIcon(buttons_view()->settings_button(), test_icon,
                          default_icon_color));
  EXPECT_TRUE(MatchesIcon(buttons_view()->snooze_button(), test_icon,
                          default_icon_color));
}

// Tests that the icon size can be specified.
TEST_F(NotificationControlButtonsTest, IconSize) {
  // Set the control buttons to have a custom size.
  int custom_size = 8;
  buttons_view()->SetButtonIconSize(custom_size);

  // Show the control buttons and verify that they are using the custom size.
  buttons_view()->ShowCloseButton(true);
  buttons_view()->ShowSettingsButton(true);
  buttons_view()->ShowSnoozeButton(true);
  const SkColor default_icon_color =
      buttons_view()->GetColorProvider()->GetColor(ui::kColorIcon);
  EXPECT_TRUE(MatchesIcon(buttons_view()->close_button(),
                          NotificationControlButtonsView::kDefaultCloseIcon,
                          default_icon_color, custom_size));
  EXPECT_TRUE(MatchesIcon(buttons_view()->settings_button(),
                          NotificationControlButtonsView::kDefaultSettingsIcon,
                          default_icon_color, custom_size));
  EXPECT_TRUE(MatchesIcon(buttons_view()->snooze_button(),
                          NotificationControlButtonsView::kDefaultSnoozeIcon,
                          default_icon_color, custom_size));

  // Show the same control buttons with a different custom size.
  buttons_view()->ShowCloseButton(false);
  buttons_view()->ShowSettingsButton(false);
  buttons_view()->ShowSnoozeButton(false);
  custom_size = 12;
  buttons_view()->SetButtonIconSize(custom_size);
  buttons_view()->ShowCloseButton(true);
  buttons_view()->ShowSettingsButton(true);
  buttons_view()->ShowSnoozeButton(true);

  // Verify that the control buttons are using the new custom size.
  EXPECT_TRUE(MatchesIcon(buttons_view()->close_button(),
                          NotificationControlButtonsView::kDefaultCloseIcon,
                          default_icon_color, custom_size));
  EXPECT_TRUE(MatchesIcon(buttons_view()->settings_button(),
                          NotificationControlButtonsView::kDefaultSettingsIcon,
                          default_icon_color, custom_size));
  EXPECT_TRUE(MatchesIcon(buttons_view()->snooze_button(),
                          NotificationControlButtonsView::kDefaultSnoozeIcon,
                          default_icon_color, custom_size));
}

// Tests spacing between control buttons.
TEST_F(NotificationControlButtonsTest, BetweenButtonsSpacing) {
  // Show all the control buttons with default horizontal spacing.
  buttons_view()->ShowCloseButton(true);
  buttons_view()->ShowSettingsButton(true);
  buttons_view()->ShowSnoozeButton(true);

  // Verify that the control buttons use the default horizontal spacing.
  EXPECT_EQ(static_cast<views::BoxLayout*>(buttons_view()->GetLayoutManager())
                ->between_child_spacing(),
            NotificationControlButtonsView::kDefaultBetweenButtonSpacing);

  // Use a non-default amount of horizontal spacing.
  const int new_spacing =
      NotificationControlButtonsView::kDefaultBetweenButtonSpacing + 1;
  buttons_view()->SetBetweenButtonSpacing(new_spacing);

  // Verify that the control buttons use the new horizontal spacing, and that
  // the layout has been invalidated.
  EXPECT_EQ(static_cast<views::BoxLayout*>(buttons_view()->GetLayoutManager())
                ->between_child_spacing(),
            new_spacing);
  EXPECT_TRUE(buttons_view()->needs_layout());
}

}  // namespace message_center
