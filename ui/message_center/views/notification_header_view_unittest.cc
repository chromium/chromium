// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_header_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/vector_icons.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace message_center {

class NotificationHeaderViewTest : public views::ViewsTestBase {
 public:
  NotificationHeaderViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~NotificationHeaderViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(200, 200);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(std::move(params));
    views::View* container =
        widget_.SetContentsView(std::make_unique<views::View>());

    notification_header_view_ = new NotificationHeaderView(nullptr);
    container->AddChildView(notification_header_view_);

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

  bool MatchesAppIconColor(SkColor color) {
    SkBitmap expected =
        *gfx::CreateVectorIcon(kProductIcon, kSmallImageSizeMD, color).bitmap();
    SkBitmap actual =
        *notification_header_view_->app_icon_for_testing().bitmap();
    return gfx::test::AreBitmapsEqual(expected, actual);
  }

  bool MatchesExpandIconColor(SkColor color) {
    constexpr int kExpandIconSize = 8;
    SkBitmap expected = *gfx::CreateVectorIcon(kNotificationExpandMoreIcon,
                                               kExpandIconSize, color)
                             .bitmap();
    SkBitmap actual =
        *notification_header_view_->expand_button()->GetImage().bitmap();
    return gfx::test::AreBitmapsEqual(expected, actual);
  }

 protected:
  NotificationHeaderView* notification_header_view_ = nullptr;

 private:
  views::Widget widget_;

  DISALLOW_COPY_AND_ASSIGN(NotificationHeaderViewTest);
};

TEST_F(NotificationHeaderViewTest, UpdatesTimestampOverTime) {
  auto* timestamp_view =
      notification_header_view_->timestamp_view_for_testing();

  notification_header_view_->SetTimestamp(base::Time::Now() +
                                          base::TimeDelta::FromHours(3) +
                                          base::TimeDelta::FromMinutes(30));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 3),
            timestamp_view->GetText());

  task_environment()->FastForwardBy(base::TimeDelta::FromHours(3));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 30),
            timestamp_view->GetText());

  task_environment()->FastForwardBy(base::TimeDelta::FromMinutes(30));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      timestamp_view->GetText());

  task_environment()->FastForwardBy(base::TimeDelta::FromDays(2));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, 2),
            timestamp_view->GetText());
}

TEST_F(NotificationHeaderViewTest, AllowsHidingOfAppIcon) {
  // The icon should be shown by default...
  EXPECT_TRUE(
      notification_header_view_->app_icon_view_for_testing()->IsDrawn());

  // ... though it can be explicitly hidden...
  notification_header_view_->SetAppIconVisible(false);
  EXPECT_FALSE(
      notification_header_view_->app_icon_view_for_testing()->IsDrawn());

  // ... and shown again.
  notification_header_view_->SetAppIconVisible(true);
  EXPECT_TRUE(
      notification_header_view_->app_icon_view_for_testing()->IsDrawn());
}

TEST_F(NotificationHeaderViewTest, SetProgress) {
  int progress = 50;
  base::string16 expected_summary_text = l10n_util::GetStringFUTF16Int(
      IDS_MESSAGE_CENTER_NOTIFICATION_PROGRESS_PERCENTAGE, progress);

  notification_header_view_->SetProgress(progress);

  auto* summary_text = notification_header_view_->summary_text_for_testing();
  EXPECT_TRUE(summary_text->GetVisible());
  EXPECT_EQ(expected_summary_text, summary_text->GetText());
}

TEST_F(NotificationHeaderViewTest, SetOverflowIndicator) {
  int count = 10;
  base::string16 expected_summary_text = l10n_util::GetStringFUTF16Int(
      IDS_MESSAGE_CENTER_LIST_NOTIFICATION_HEADER_OVERFLOW_INDICATOR, count);

  notification_header_view_->SetOverflowIndicator(count);

  auto* summary_text = notification_header_view_->summary_text_for_testing();
  EXPECT_TRUE(summary_text->GetVisible());
  EXPECT_EQ(expected_summary_text, summary_text->GetText());
}

TEST_F(NotificationHeaderViewTest, SetSummaryText) {
  base::string16 expected_summary_text = base::ASCIIToUTF16("summary");

  notification_header_view_->SetSummaryText(expected_summary_text);

  auto* summary_text = notification_header_view_->summary_text_for_testing();
  EXPECT_TRUE(summary_text->GetVisible());
  EXPECT_EQ(expected_summary_text, summary_text->GetText());
}

TEST_F(NotificationHeaderViewTest, TimestampHiddenWithProgress) {
  auto* timestamp_view =
      notification_header_view_->timestamp_view_for_testing();
  notification_header_view_->SetTimestamp(base::Time::Now());

  // We do not show the timestamp viev if there is a progress view.
  notification_header_view_->SetProgress(/*progress=*/50);
  EXPECT_FALSE(timestamp_view->GetVisible());

  // Make sure we show the timestamp view with overflow indicators.
  notification_header_view_->SetOverflowIndicator(/*count=*/10);
  EXPECT_TRUE(timestamp_view->GetVisible());

  // Make sure we show the timestamp view with summary text.
  notification_header_view_->SetSummaryText(base::ASCIIToUTF16("summary"));
  EXPECT_TRUE(timestamp_view->GetVisible());
}

TEST_F(NotificationHeaderViewTest, ColorContrastEnforcement) {
  notification_header_view_->SetSummaryText(base::ASCIIToUTF16("summary"));
  auto* summary_text = notification_header_view_->summary_text_for_testing();
  notification_header_view_->ClearAppIcon();
  notification_header_view_->SetExpandButtonEnabled(true);
  notification_header_view_->SetExpanded(false);

  // A bright background should enforce dark enough icons.
  notification_header_view_->SetBackgroundColor(SK_ColorWHITE);
  notification_header_view_->SetAccentColor(SK_ColorWHITE);
  SkColor expected_color =
      color_utils::BlendForMinContrast(SK_ColorWHITE, SK_ColorWHITE).color;
  EXPECT_EQ(expected_color, summary_text->GetEnabledColor());
  EXPECT_TRUE(MatchesAppIconColor(expected_color));
  EXPECT_TRUE(MatchesExpandIconColor(expected_color));

  // A dark background should enforce bright enough icons.
  notification_header_view_->SetBackgroundColor(SK_ColorBLACK);
  notification_header_view_->SetAccentColor(SK_ColorBLACK);
  expected_color =
      color_utils::BlendForMinContrast(SK_ColorBLACK, SK_ColorBLACK).color;
  EXPECT_EQ(expected_color, summary_text->GetEnabledColor());
  EXPECT_TRUE(MatchesAppIconColor(expected_color));
  EXPECT_TRUE(MatchesExpandIconColor(expected_color));
}
}  // namespace message_center
