// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/relative_time_formatter.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"


namespace message_center {

namespace {

// In Android, DateUtils.YEAR_IN_MILLIS is 364 days (52 weeks * 7 days).
constexpr base::TimeDelta kYearTimeDelta = base::Days(364);

std::u16string GetRelativeTime(base::TimeDelta delta) {
  std::u16string relative_time;
  base::TimeDelta next_update;
  GetRelativeTimeStringAndNextUpdateTime(delta, &relative_time, &next_update);
  return relative_time;
}

base::TimeDelta GetNextUpdate(base::TimeDelta delta) {
  std::u16string relative_time;
  base::TimeDelta next_update;
  GetRelativeTimeStringAndNextUpdateTime(delta, &relative_time, &next_update);
  return next_update;
}

}  // namespace

TEST(RelativeTimeFormatterTest, Format_Future_30Sec) {
  std::u16string relative_time = GetRelativeTime(base::Seconds(30));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_30Sec) {
  std::u16string relative_time = GetRelativeTime(base::Seconds(-30));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_60Sec) {
  std::u16string relative_time = GetRelativeTime(base::Minutes(1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_60Sec) {
  std::u16string relative_time = GetRelativeTime(base::Minutes(-1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_5Min) {
  std::u16string relative_time = GetRelativeTime(base::Minutes(5));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 5),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_5Min) {
  std::u16string relative_time = GetRelativeTime(base::Minutes(-5));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST, 5),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_60Min) {
  std::u16string relative_time = GetRelativeTime(base::Hours(1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_60Min) {
  std::u16string relative_time = GetRelativeTime(base::Hours(-1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_10Hrs) {
  std::u16string relative_time = GetRelativeTime(base::Hours(10));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 10),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_10Hrs) {
  std::u16string relative_time = GetRelativeTime(base::Hours(-10));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST, 10),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_24Hrs) {
  std::u16string relative_time = GetRelativeTime(base::Days(1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST_FUTURE, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_24Hrs) {
  std::u16string relative_time = GetRelativeTime(base::Days(-1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_1Year) {
  std::u16string relative_time = GetRelativeTime(kYearTimeDelta);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST_FUTURE, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_1Year) {
  std::u16string relative_time = GetRelativeTime(-kYearTimeDelta);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST, 1),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Future_10Years) {
  std::u16string relative_time = GetRelativeTime(kYearTimeDelta * 10);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST_FUTURE, 10),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Format_Past_10Years) {
  std::u16string relative_time = GetRelativeTime(-kYearTimeDelta * 10);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST, 10),
            relative_time);
}

TEST(RelativeTimeFormatterTest, Update_Future_Year) {
  base::TimeDelta next_update =
      GetNextUpdate(kYearTimeDelta * 2 - base::Milliseconds(1));
  EXPECT_EQ(kYearTimeDelta, next_update);
}

TEST(RelativeTimeFormatterTest, Update_Future_Day) {
  base::TimeDelta next_update =
      GetNextUpdate(base::Days(2) - base::Milliseconds(1));
  EXPECT_EQ(base::Days(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Future_Hour) {
  base::TimeDelta next_update =
      GetNextUpdate(base::Hours(2) - base::Milliseconds(1));
  EXPECT_EQ(base::Hours(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Future_Minute) {
  base::TimeDelta next_update =
      GetNextUpdate(base::Minutes(2) - base::Milliseconds(1));
  EXPECT_EQ(base::Minutes(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Future_Now) {
  base::TimeDelta next_update = GetNextUpdate(base::Seconds(30));
  EXPECT_EQ(base::Seconds(90), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Past_Now) {
  base::TimeDelta next_update = GetNextUpdate(base::Seconds(-30));
  EXPECT_EQ(base::Seconds(30), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Past_Minute) {
  base::TimeDelta next_update = GetNextUpdate(base::Minutes(-1));
  EXPECT_EQ(base::Minutes(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Past_Hour) {
  base::TimeDelta next_update = GetNextUpdate(base::Hours(-1));
  EXPECT_EQ(base::Hours(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Past_Day) {
  base::TimeDelta next_update = GetNextUpdate(base::Days(-1));
  EXPECT_EQ(base::Days(1), next_update);
}

TEST(RelativeTimeFormatterTest, Update_Past_Year) {
  base::TimeDelta next_update = GetNextUpdate(-kYearTimeDelta);
  EXPECT_EQ(kYearTimeDelta, next_update);
}

}  // namespace message_center
