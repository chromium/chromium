// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/relative_time_formatter.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

using base::TimeDelta;

namespace message_center {

namespace {

// In Android, DateUtils.YEAR_IN_MILLIS is 364 days (52 weeks * 7 days).
constexpr TimeDelta kYearTimeDelta = TimeDelta::FromDays(364);

base::string16 GetRelativeTime(TimeDelta delta) {
  base::string16 relative_time;
  TimeDelta next_update;
  GetRelativeTimeStringAndNextUpdateTime(delta, &relative_time, &next_update);
  return relative_time;
}

TimeDelta GetNextUpdate(TimeDelta delta) {
  base::string16 relative_time;
  TimeDelta next_update;
  GetRelativeTimeStringAndNextUpdateTime(delta, &relative_time, &next_update);
  return next_update;
}

}  // namespace

TEST(RelativeTimeFormatterTest, Format_Future_30Sec) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromSeconds(30));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_30Sec) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromSeconds(-30));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_60Sec) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromMinutes(1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_60Sec) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromMinutes(-1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_5Min) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromMinutes(5));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 5),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_5Min) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromMinutes(-5));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST, 5),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_60Min) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromHours(1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_60Min) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromHours(-1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_10Hrs) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromHours(10));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 10),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_10Hrs) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromHours(-10));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST, 10),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_24Hrs) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromDays(1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST_FUTURE, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_24Hrs) {
  base::string16 relative_time = GetRelativeTime(TimeDelta::FromDays(-1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_1Year) {
  base::string16 relative_time = GetRelativeTime(kYearTimeDelta);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST_FUTURE, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_1Year) {
  base::string16 relative_time = GetRelativeTime(-kYearTimeDelta);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_10Years) {
  base::string16 relative_time = GetRelativeTime(kYearTimeDelta * 10);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST_FUTURE, 10),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_10Years) {
  base::string16 relative_time = GetRelativeTime(-kYearTimeDelta * 10);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST, 10),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Update_Future_Year) {
  TimeDelta next_update =
      GetNextUpdate(kYearTimeDelta * 2 - TimeDelta::FromMilliseconds(1));
  EXPECT_EQ(kYearTimeDelta, next_update);
}

TEST(RelativeTimeFormatterTest, Update_Future_Day) {
  TimeDelta next_update =
      GetNextUpdate(TimeDelta::FromDays(2) - TimeDelta::FromMilliseconds(1));
  EXPECT_EQ(TimeDelta::FromDays(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Future_Hour) {
  TimeDelta next_update =
      GetNextUpdate(TimeDelta::FromHours(2) - TimeDelta::FromMilliseconds(1));
  EXPECT_EQ(TimeDelta::FromHours(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Future_Minute) {
  TimeDelta next_update =
      GetNextUpdate(TimeDelta::FromMinutes(2) - TimeDelta::FromMilliseconds(1));
  EXPECT_EQ(TimeDelta::FromMinutes(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Future_Now) {
  TimeDelta next_update = GetNextUpdate(TimeDelta::FromSeconds(30));
  EXPECT_EQ(TimeDelta::FromSeconds(90), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Past_Now) {
  TimeDelta next_update = GetNextUpdate(TimeDelta::FromSeconds(-30));
  EXPECT_EQ(TimeDelta::FromSeconds(30), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Past_Minute) {
  TimeDelta next_update = GetNextUpdate(TimeDelta::FromMinutes(-1));
  EXPECT_EQ(TimeDelta::FromMinutes(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Past_Hour) {
  TimeDelta next_update = GetNextUpdate(TimeDelta::FromHours(-1));
  EXPECT_EQ(TimeDelta::FromHours(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Past_Day) {
  TimeDelta next_update = GetNextUpdate(TimeDelta::FromDays(-1));
  EXPECT_EQ(TimeDelta::FromDays(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Past_Year) {
  TimeDelta next_update = GetNextUpdate(-kYearTimeDelta);
  EXPECT_EQ(kYearTimeDelta, next_update);
}

}  // namespace message_center
