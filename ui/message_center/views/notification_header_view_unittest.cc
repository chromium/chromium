// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_header_view.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/image_view.h"
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
    views::View* container = new views::View();
    widget_.SetContentsView(container);

    notification_header_view_ = new NotificationHeaderView(nullptr);
    container->AddChildView(notification_header_view_);

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

 protected:
  NotificationHeaderView* notification_header_view_ = nullptr;

 private:
  views::Widget widget_;

  DISALLOW_COPY_AND_ASSIGN(NotificationHeaderViewTest);
};

TEST_F(NotificationHeaderViewTest, UpdatesTimestampOverTime) {
  notification_header_view_->SetTimestamp(base::Time::Now() +
                                          base::TimeDelta::FromHours(3) +
                                          base::TimeDelta::FromMinutes(30));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 3),
            notification_header_view_->timestamp_for_testing());

  task_environment_->FastForwardBy(base::TimeDelta::FromHours(3));
  task_environment_->RunUntilIdle();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 30),
            notification_header_view_->timestamp_for_testing());

  task_environment_->FastForwardBy(base::TimeDelta::FromMinutes(30));
  task_environment_->RunUntilIdle();

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      notification_header_view_->timestamp_for_testing());

  task_environment_->FastForwardBy(base::TimeDelta::FromDays(2));
  task_environment_->RunUntilIdle();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, 2),
            notification_header_view_->timestamp_for_testing());
}

TEST_F(NotificationHeaderViewTest, AllowsHidingOfAppIcon) {
  // The icon should be shown by default.
  EXPECT_TRUE(
      notification_header_view_->app_icon_view_for_testing()->IsDrawn());

  // Though it can be explicitly hidden.
  notification_header_view_->HideAppIcon();
  EXPECT_FALSE(
      notification_header_view_->app_icon_view_for_testing()->IsDrawn());
}

}  // namespace message_center
