// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/time_format.h"

#include <limits>

#include "base/check_op.h"
#include "base/component_export.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "ui/base/l10n/formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

using ui::TimeFormat;

namespace ui {

COMPONENT_EXPORT(UI_BASE)
base::LazyInstance<FormatterContainer>::Leaky g_container =
    LAZY_INSTANCE_INITIALIZER;

// static
std::u16string TimeFormat::Simple(TimeFormat::Format format,
                                  TimeFormat::Length length,
                                  const base::TimeDelta& delta) {
  return Detailed(format, length, 0, delta);
}

std::u16string TimeFormat::SimpleWithMonthAndYear(TimeFormat::Format format,
                                                  TimeFormat::Length length,
                                                  const base::TimeDelta& delta,
                                                  bool with_month_and_year) {
  return DetailedWithMonthAndYear(format, length, 0, delta,
                                  with_month_and_year);
}

// static
std::u16string TimeFormat::Detailed(TimeFormat::Format format,
                                    TimeFormat::Length length,
                                    int cutoff,
                                    const base::TimeDelta& delta) {
  return DetailedWithMonthAndYear(format, length, cutoff, delta, false);
}

std::u16string TimeFormat::DetailedWithMonthAndYear(
    TimeFormat::Format format,
    TimeFormat::Length length,
    int cutoff,
    const base::TimeDelta& delta,
    bool with_month_and_year) {
  DCHECK_GE(delta, base::TimeDelta());

  // Negative cutoff: always use two-value format.
  if (cutoff < 0)
    cutoff = std::numeric_limits<int>::max();

  constexpr base::TimeDelta kMinute = base::Minutes(1);
  constexpr base::TimeDelta kHour = base::Hours(1);
  constexpr base::TimeDelta kDay = base::Days(1);

  // Simplify one year to be 365 days.
  constexpr base::TimeDelta kYear = 365 * kDay;

  // An average month is a twelfth of a year.
  constexpr base::TimeDelta kMonth = kYear / 12;

  constexpr base::TimeDelta kHalfSecond = base::Seconds(1) / 2;
  constexpr base::TimeDelta kHalfMinute = kMinute / 2;
  constexpr base::TimeDelta kHalfHour = kHour / 2;
  constexpr base::TimeDelta kHalfDay = kDay / 2;

  // Rationale: Start by determining major (first) unit, then add minor (second)
  // unit if mandated by |cutoff|.
  icu::UnicodeString time_string;
  const Formatter* formatter = g_container.Get().Get(format, length);
  if (delta < kMinute - kHalfSecond) {
    // Anything up to 59.500 seconds is formatted as seconds.
    const int seconds = base::ClampRound(delta.InSecondsF());
    formatter->Format(Formatter::UNIT_SEC, seconds, &time_string);
  } else if (delta < kHour - (cutoff < base::Time::kMinutesPerHour
                                  ? kHalfMinute
                                  : kHalfSecond)) {
    // Anything up to 59.5 minutes (respectively 59:59.500 when |cutoff| permits
    // two-value output) is formatted as minutes (respectively minutes and
    // seconds).
    if (delta >= cutoff * kMinute - kHalfSecond) {
      const int minutes = (delta + kHalfMinute).InMinutes();
      formatter->Format(Formatter::UNIT_MIN, minutes, &time_string);
    } else {
      const int minutes = (delta + kHalfSecond).InMinutes();
      const int seconds =
          base::ClampRound(delta.InSecondsF()) % base::Time::kSecondsPerMinute;
      formatter->Format(Formatter::TWO_UNITS_MIN_SEC,
                        minutes, seconds, &time_string);
    }
  } else if (delta < kDay - (cutoff < base::Time::kHoursPerDay ? kHalfHour
                                                               : kHalfMinute)) {
    // Anything up to 23.5 hours (respectively 23:59:30.000 when |cutoff|
    // permits two-value output) is formatted as hours (respectively hours and
    // minutes).
    if (delta >= cutoff * kHour - kHalfMinute) {
      const int hours = (delta + kHalfHour).InHours();
      formatter->Format(Formatter::UNIT_HOUR, hours, &time_string);
    } else {
      const int hours = (delta + kHalfMinute).InHours();
      const int minutes =
          (delta + kHalfMinute).InMinutes() % base::Time::kMinutesPerHour;
      formatter->Format(Formatter::TWO_UNITS_HOUR_MIN,
                        hours, minutes, &time_string);
    }
  } else if (!with_month_and_year || delta < kMonth) {
    // Anything bigger is formatted as days (respectively days and hours).
    if (delta >= cutoff * kDay - kHalfHour) {
      const int days = (delta + kHalfDay).InDays();
      formatter->Format(Formatter::UNIT_DAY, days, &time_string);
    } else {
      const int days = (delta + kHalfHour).InDays();
      const int hours =
          (delta + kHalfHour).InHours() % base::Time::kHoursPerDay;
      formatter->Format(Formatter::TWO_UNITS_DAY_HOUR, days, hours,
                        &time_string);
    }
  } else if (delta < kYear) {
    DCHECK(with_month_and_year);
    const int month = base::ClampFloor(delta / kMonth);
    DCHECK_GE(month, 1);
    DCHECK_LE(month, 12);
    formatter->Format(Formatter::UNIT_MONTH, month, &time_string);
  } else {
    DCHECK(with_month_and_year);
    const int year = base::ClampFloor(delta / kYear);
    formatter->Format(Formatter::UNIT_YEAR, year, &time_string);
  }

  const int32_t capacity = time_string.length() + 1;
  DCHECK_GT(capacity, 1);
  std::u16string result;
  UErrorCode error = U_ZERO_ERROR;
  time_string.extract(base::WriteInto(&result, static_cast<size_t>(capacity)),
                      capacity, error);
  DCHECK(U_SUCCESS(error));
  return result;
}

// static
std::u16string TimeFormat::RelativeDate(
    const base::Time& time,
    const base::Time* optional_midnight_today) {
  const base::Time midnight_today = optional_midnight_today
                                        ? *optional_midnight_today
                                        : base::Time::Now().LocalMidnight();
  constexpr base::TimeDelta kDay = base::Days(1);
  const base::Time tomorrow = midnight_today + kDay;
  const base::Time yesterday = midnight_today - kDay;
  if (time >= tomorrow)
    return std::u16string();
  if (time >= midnight_today)
    return l10n_util::GetStringUTF16(IDS_PAST_TIME_TODAY);
  return (time >= yesterday)
             ? l10n_util::GetStringUTF16(IDS_PAST_TIME_YESTERDAY)
             : std::u16string();
}

}  // namespace ui
