// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_TIME_FORMAT_H_
#define UI_BASE_L10N_TIME_FORMAT_H_

#include <string>

#include "base/component_export.h"
#include "build/build_config.h"

namespace base {
class Time;
class TimeDelta;
}

namespace ui {

// Methods to format time values as strings.
class COMPONENT_EXPORT(UI_BASE) TimeFormat {
 public:
  enum Format {
    FORMAT_DURATION,   // Plain duration, e.g. in English: "2 minutes".
    FORMAT_REMAINING,  // Remaining time, e.g. in English: "2 minutes left".
    FORMAT_ELAPSED,    // Elapsed time, e.g. in English: "2 minutes ago".
#if BUILDFLAG(IS_IOS)
    FORMAT_TITLE_CASE_ELAPSED,  // Elapsed time in title case, e.g. in English:
                                // "2 Minutes Ago".
#endif
    FORMAT_COUNT  // Enum size counter, not a format.  Must be last.
  };

  enum Length {
    LENGTH_SHORT,  // Short format, e.g. in English: sec/min/hour/day.
    LENGTH_LONG,   // Long format, e.g. in English: second/minute/hour/day.
    LENGTH_COUNT   // Enum size counter, not a length.  Must be last.
  };

  TimeFormat() = delete;
  TimeFormat(const TimeFormat&) = delete;
  TimeFormat& operator=(const TimeFormat&) = delete;

  // Equivalent to SimpleWithMonthAndYear(format, length, delta, false);
  static std::u16string Simple(Format format,
                               Length length,
                               const base::TimeDelta& delta);
  // Return a localized string of approximate time duration, formatted as a
  // single number, e.g. in English "2 hours ago".  Currently, all combinations
  // of format and length are implemented.
  // If |use_month_and_year| is false. biggest unit is the day. If it is true,
  // "month" and "year" are also used.
  static std::u16string SimpleWithMonthAndYear(Format format,
                                               Length length,
                                               const base::TimeDelta& delta,
                                               bool use_month_and_year);

  // Equivalent to
  // DetailedWithMonthAndYear(format, length, cutoff, delta, false);
  static std::u16string Detailed(Format format,
                                 Length length,
                                 int cutoff,
                                 const base::TimeDelta& delta);

  // Return a localized string of more precise time duration, either formatted
  // as a single value or as two values: for a time delta of e.g. 2h19m either
  // "2 hours" or "2 hours and 19 minutes" could be returned in English.
  // Two-value output can be forced by setting |cutoff| to -1.  Single-value
  // output can be forced by using Simple() or setting |cutoff| to 0.
  // Otherwise, choice of format happens automatically and the value of |cutoff|
  // determines the largest numeric value that may appear in a single-value
  // format -- for lower numeric values, a second unit is added to increase
  // precision.  (Applied to the examples above, a |cutoff| of 2 or smaller
  // would yield the first string and a |cutoff| of 3 or larger would return the
  // second string.)
  //
  // The aim of this logic is to reduce rounding errors (that in single-value
  // representation can amount up to 33% of the true time duration) while at the
  // same time avoiding over-precise time outputs such as e.g. "56 minutes 29
  // seconds".  The relative rounding error is guaranteed to be less than 0.5 /
  // |cutoff| (e.g. 5% for a |cutoff| of 10) and a second unit is only used when
  // necessary to achieve the precision guarantee.
  //
  // If |use_month_and_year| is true, also display longer time in number of
  // month and year. A year is defined as 365 days. A month is defined as a
  // twelfth of the year. Due to the length of these time, and the approximate
  // definition, no cutoff is allowed with these values and showing two values
  // is not supported (1 month 2 days is ambiguous due to the definition of a
  // month).
  //
  // Currently, the only combination of format and length that is implemented is
  // (FORMAT_DURATION, LENGTH_LONG), but it's easy to add others if required.
  //
  // Note: To allow pre-, post- and infixes which can be inflected depending on
  // either the first or the second value, two separate translation strings
  // (IDS_TIME_*_1ST and IDS_TIME_*_2ND) are used per [plurality] times [pair of
  // units] and are concatenated after having been formatted individually.  The
  // separator between first unit and second unit (a blank in English) is
  // included in IDS_TIME_*_1ST.
  static std::u16string DetailedWithMonthAndYear(Format format,
                                                 Length length,
                                                 int cutoff,
                                                 const base::TimeDelta& delta,
                                                 bool use_month_and_year);

  // For displaying a relative time in the past.  This method returns either
  // "Today", "Yesterday", or an empty string if it's older than that.  Returns
  // the empty string for days in the future.
  //
  // TODO(brettw): This should be able to handle days in the future like
  //    "Tomorrow".
  // TODO(tc): This should be able to do things like "Last week".  This
  //    requires handling singluar/plural for all languages.
  //
  // The second parameter is optional, it is midnight of "Now" for relative day
  // computations: Time::Now().LocalMidnight()
  // If NULL, the current day's midnight will be retrieved, which can be
  // slow. If many items are being processed, it is best to get the current
  // time once at the beginning and pass it for each computation.
  static std::u16string RelativeDate(const base::Time& time,
                                     const base::Time* optional_midnight_today);
};

}  // namespace ui

#endif  // UI_BASE_L10N_TIME_FORMAT_H_
