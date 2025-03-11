// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::names::RawDateTimeNamesBorrowed;
use super::pattern::DateTimePatternBorrowed;
use crate::format::datetime::try_write_pattern_items;
use crate::format::ExtractedInput;
use crate::scaffold::*;
use crate::scaffold::{
    AllInputMarkers, DateInputMarkers, DateTimeMarkers, InFixedCalendar, TimeMarkers,
    TypedDateDataMarkers, ZoneMarkers,
};
use crate::DateTimeWriteError;
use core::fmt;
use core::marker::PhantomData;
use writeable::TryWriteable;

/// A formatter for a specific [`DateTimePattern`].
///
/// ❗ This type forgoes most internationalization functionality of the datetime crate.
/// It assumes that the pattern is already localized for the customer's locale. Most clients
/// should use [`DateTimeFormatter`] instead of directly formatting with patterns.
///
/// Create one of these via factory methods on [`FixedCalendarDateTimeNames`].
///
/// [`DateTimePattern`]: super::DateTimePattern
/// [`FixedCalendarDateTimeNames`]: super::FixedCalendarDateTimeNames
/// [`DateTimeFormatter`]: crate::DateTimeFormatter
#[derive(Debug, Copy, Clone)]
pub struct DateTimePatternFormatter<'a, C: CldrCalendar, FSet> {
    inner: RawDateTimePatternFormatter<'a>,
    _calendar: PhantomData<C>,
    _marker: PhantomData<FSet>,
}

#[derive(Debug, Copy, Clone)]
pub(crate) struct RawDateTimePatternFormatter<'a> {
    pattern: DateTimePatternBorrowed<'a>,
    names: RawDateTimeNamesBorrowed<'a>,
}

impl<'a, C: CldrCalendar, FSet> DateTimePatternFormatter<'a, C, FSet> {
    pub(crate) fn new(
        pattern: DateTimePatternBorrowed<'a>,
        names: RawDateTimeNamesBorrowed<'a>,
    ) -> Self {
        Self {
            inner: RawDateTimePatternFormatter { pattern, names },
            _calendar: PhantomData,
            _marker: PhantomData,
        }
    }
}

impl<'a, C: CldrCalendar, FSet: DateTimeMarkers> DateTimePatternFormatter<'a, C, FSet>
where
    FSet::D: TypedDateDataMarkers<C> + DateInputMarkers,
    FSet::T: TimeMarkers,
    FSet::Z: ZoneMarkers,
{
    /// Formats a date and time of day with a custom date/time pattern.
    ///
    /// # Examples
    ///
    /// Format a date:
    ///
    /// ```
    /// use icu::calendar::Gregorian;
    /// use icu::datetime::fieldsets::enums::DateFieldSet;
    /// use icu::datetime::input::Date;
    /// use icu::datetime::pattern::DateTimePattern;
    /// use icu::datetime::pattern::FixedCalendarDateTimeNames;
    /// use icu::datetime::pattern::MonthNameLength;
    /// use icu::datetime::pattern::YearNameLength;
    /// use icu::locale::locale;
    /// use writeable::assert_try_writeable_eq;
    ///
    /// // Create an instance that can format wide month and era names:
    /// let mut names: FixedCalendarDateTimeNames<Gregorian, DateFieldSet> =
    ///     FixedCalendarDateTimeNames::try_new(locale!("en-GB").into()).unwrap();
    /// names
    ///     .include_month_names(MonthNameLength::Wide)
    ///     .unwrap()
    ///     .include_year_names(YearNameLength::Wide)
    ///     .unwrap();
    ///
    /// // Create a pattern from a pattern string:
    /// let pattern_str = "'The date is:' MMMM d, y GGGG";
    /// let pattern: DateTimePattern = pattern_str.parse().unwrap();
    ///
    /// // Test it with some different dates:
    /// // Note: extended year -50 is year 51 BCE
    /// let date_bce = Date::try_new_gregorian(-50, 3, 15).unwrap();
    /// let date_ce = Date::try_new_gregorian(1700, 11, 20).unwrap();
    /// assert_try_writeable_eq!(
    ///     names.with_pattern_unchecked(&pattern).format(&date_bce),
    ///     "The date is: March 15, 51 Before Christ"
    /// );
    /// assert_try_writeable_eq!(
    ///     names.with_pattern_unchecked(&pattern).format(&date_ce),
    ///     "The date is: November 20, 1700 Anno Domini"
    /// );
    /// ```
    ///
    /// Format a time:
    ///
    /// ```
    /// use icu::calendar::Gregorian;
    /// use icu::datetime::fieldsets::enums::TimeFieldSet;
    /// use icu::datetime::input::Time;
    /// use icu::datetime::pattern::DateTimePattern;
    /// use icu::datetime::pattern::DayPeriodNameLength;
    /// use icu::datetime::pattern::FixedCalendarDateTimeNames;
    /// use icu::locale::locale;
    /// use writeable::assert_try_writeable_eq;
    ///
    /// // Create an instance that can format abbreviated day periods:
    /// let mut names: FixedCalendarDateTimeNames<Gregorian, TimeFieldSet> =
    ///     FixedCalendarDateTimeNames::try_new(locale!("en-US").into()).unwrap();
    /// names
    ///     .include_day_period_names(DayPeriodNameLength::Abbreviated)
    ///     .unwrap();
    ///
    /// // Create a pattern from a pattern string:
    /// let pattern_str = "'The time is:' h:mm b";
    /// let pattern: DateTimePattern = pattern_str.parse().unwrap();
    ///
    /// // Test it with different times of day:
    /// let time_am = Time::try_new(11, 4, 14, 0).unwrap();
    /// let time_pm = Time::try_new(13, 41, 28, 0).unwrap();
    /// let time_noon = Time::try_new(12, 0, 0, 0).unwrap();
    /// let time_midnight = Time::try_new(0, 0, 0, 0).unwrap();
    /// assert_try_writeable_eq!(
    ///     names.with_pattern_unchecked(&pattern).format(&time_am),
    ///     "The time is: 11:04 AM"
    /// );
    /// assert_try_writeable_eq!(
    ///     names.with_pattern_unchecked(&pattern).format(&time_pm),
    ///     "The time is: 1:41 PM"
    /// );
    /// assert_try_writeable_eq!(
    ///     names.with_pattern_unchecked(&pattern).format(&time_noon),
    ///     "The time is: 12:00 noon"
    /// );
    /// assert_try_writeable_eq!(
    ///     names
    ///         .with_pattern_unchecked(&pattern)
    ///         .format(&time_midnight),
    ///     "The time is: 12:00 midnight"
    /// );
    /// ```
    ///
    /// Format a time zone:
    ///
    /// ```
    /// use icu::calendar::Gregorian;
    /// use icu::datetime::fieldsets::enums::ZoneFieldSet;
    /// use icu::datetime::input::ZonedDateTime;
    /// use icu::datetime::pattern::DateTimePattern;
    /// use icu::datetime::pattern::FixedCalendarDateTimeNames;
    /// use icu::locale::locale;
    /// use icu::time::zone::{IanaParser, UtcOffsetCalculator};
    /// use writeable::assert_try_writeable_eq;
    ///
    /// let mut london_winter = ZonedDateTime::try_from_str(
    ///     "2024-01-01T00:00:00+00:00[Europe/London]",
    ///     Gregorian,
    ///     IanaParser::new(),
    ///     &UtcOffsetCalculator::new(),
    /// )
    /// .unwrap();
    /// let mut london_summer = ZonedDateTime::try_from_str(
    ///     "2024-07-01T00:00:00+01:00[Europe/London]",
    ///     Gregorian,
    ///     IanaParser::new(),
    ///     &UtcOffsetCalculator::new(),
    /// )
    /// .unwrap();
    ///
    /// let mut names =
    ///     FixedCalendarDateTimeNames::<Gregorian, ZoneFieldSet>::try_new(
    ///         locale!("en-GB").into(),
    ///     )
    ///     .unwrap();
    ///
    /// names.include_time_zone_essentials().unwrap();
    /// names.include_time_zone_specific_short_names().unwrap();
    ///
    /// // Create a pattern with symbol `z`:
    /// let pattern_str = "'Your time zone is:' z";
    /// let pattern: DateTimePattern = pattern_str.parse().unwrap();
    ///
    /// assert_try_writeable_eq!(
    ///     names
    ///         .with_pattern_unchecked(&pattern)
    ///         .format(&london_winter),
    ///     "Your time zone is: GMT",
    /// );
    /// assert_try_writeable_eq!(
    ///     names
    ///         .with_pattern_unchecked(&pattern)
    ///         .format(&london_summer),
    ///     "Your time zone is: BST",
    /// );
    /// ```
    pub fn format<I>(&self, datetime: &I) -> FormattedDateTimePattern<'a>
    where
        I: ?Sized + InFixedCalendar<C> + AllInputMarkers<FSet>,
    {
        FormattedDateTimePattern {
            pattern: self.inner.pattern,
            input: ExtractedInput::extract_from_neo_input::<FSet::D, FSet::T, FSet::Z, I>(datetime),
            names: self.inner.names,
        }
    }
}

/// A pattern that has been interpolated and implements [`TryWriteable`].
#[derive(Debug)]
pub struct FormattedDateTimePattern<'a> {
    pattern: DateTimePatternBorrowed<'a>,
    input: ExtractedInput,
    names: RawDateTimeNamesBorrowed<'a>,
}

impl TryWriteable for FormattedDateTimePattern<'_> {
    type Error = DateTimeWriteError;
    fn try_write_to_parts<S: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut S,
    ) -> Result<Result<(), Self::Error>, fmt::Error> {
        try_write_pattern_items(
            self.pattern.0.as_borrowed().metadata,
            self.pattern.0.as_borrowed().items.iter(),
            &self.input,
            &self.names,
            self.names.decimal_formatter,
            sink,
        )
    }

    // TODO(#489): Implement writeable_length_hint
}

#[cfg(test)]
#[cfg(feature = "compiled_data")]
mod tests {
    use super::super::*;
    use icu_calendar::{Date, Gregorian};
    use icu_locale_core::locale;
    use icu_time::{DateTime, Time};
    use writeable::assert_try_writeable_eq;

    #[test]
    fn test_basic_pattern_formatting() {
        let locale = locale!("en").into();
        let mut names: FixedCalendarDateTimeNames<Gregorian> =
            FixedCalendarDateTimeNames::try_new(locale).unwrap();
        names
            .load_month_names(&crate::provider::Baked, MonthNameLength::Wide)
            .unwrap()
            .load_weekday_names(&crate::provider::Baked, WeekdayNameLength::Abbreviated)
            .unwrap()
            .load_year_names(&crate::provider::Baked, YearNameLength::Narrow)
            .unwrap()
            .load_day_period_names(&crate::provider::Baked, DayPeriodNameLength::Abbreviated)
            .unwrap();
        let pattern: DateTimePattern = "'It is' E, MMMM d, y GGGGG 'at' hh:mm a'!'"
            .parse()
            .unwrap();
        let datetime = DateTime {
            date: Date::try_new_gregorian(2023, 10, 25).unwrap(),
            time: Time::try_new(15, 0, 55, 0).unwrap(),
        };
        let formatted_pattern = names.with_pattern_unchecked(&pattern).format(&datetime);

        assert_try_writeable_eq!(
            formatted_pattern,
            "It is Wed, October 25, 2023 A at 03:00 PM!",
            Ok(()),
        );
    }

    #[test]
    fn test_era_coverage() {
        let locale = locale!("uk").into();
        #[derive(Debug)]
        struct TestCase {
            pattern: &'static str,
            length: YearNameLength,
            expected: &'static str,
        }
        let cases = [
            TestCase {
                pattern: "<G>",
                length: YearNameLength::Abbreviated,
                expected: "<н. е.>",
            },
            TestCase {
                pattern: "<GG>",
                length: YearNameLength::Abbreviated,
                expected: "<н. е.>",
            },
            TestCase {
                pattern: "<GGG>",
                length: YearNameLength::Abbreviated,
                expected: "<н. е.>",
            },
            TestCase {
                pattern: "<GGGG>",
                length: YearNameLength::Wide,
                expected: "<нашої ери>",
            },
            TestCase {
                pattern: "<GGGGG>",
                length: YearNameLength::Narrow,
                expected: "<н.е.>",
            },
        ];
        for cas in cases {
            let TestCase {
                pattern,
                length,
                expected,
            } = cas;
            let mut names: FixedCalendarDateTimeNames<Gregorian> =
                FixedCalendarDateTimeNames::try_new(locale).unwrap();
            names
                .load_year_names(&crate::provider::Baked, length)
                .unwrap();
            let pattern: DateTimePattern = pattern.parse().unwrap();
            let datetime = DateTime {
                date: Date::try_new_gregorian(2023, 11, 17).unwrap(),
                time: Time::try_new(13, 41, 28, 0).unwrap(),
            };
            let formatted_pattern = names.with_pattern_unchecked(&pattern).format(&datetime);

            assert_try_writeable_eq!(formatted_pattern, expected, Ok(()), "{cas:?}");
        }
    }

    #[test]
    fn test_month_coverage() {
        // Ukrainian has different values for format and standalone
        let locale = locale!("uk").into();
        #[derive(Debug)]
        struct TestCase {
            pattern: &'static str,
            length: MonthNameLength,
            expected: &'static str,
        }
        let cases = [
            // 'M' and 'MM' are numeric
            TestCase {
                pattern: "<MMM>",
                length: MonthNameLength::Abbreviated,
                expected: "<лист.>",
            },
            TestCase {
                pattern: "<MMMM>",
                length: MonthNameLength::Wide,
                expected: "<листопада>",
            },
            TestCase {
                pattern: "<MMMMM>",
                length: MonthNameLength::Narrow,
                expected: "<л>",
            },
            // 'L' and 'LL' are numeric
            TestCase {
                pattern: "<LLL>",
                length: MonthNameLength::StandaloneAbbreviated,
                expected: "<лист.>",
            },
            TestCase {
                pattern: "<LLLL>",
                length: MonthNameLength::StandaloneWide,
                expected: "<листопад>",
            },
            TestCase {
                pattern: "<LLLLL>",
                length: MonthNameLength::StandaloneNarrow,
                expected: "<Л>",
            },
        ];
        for cas in cases {
            let TestCase {
                pattern,
                length,
                expected,
            } = cas;
            let mut names: FixedCalendarDateTimeNames<Gregorian> =
                FixedCalendarDateTimeNames::try_new(locale).unwrap();
            names
                .load_month_names(&crate::provider::Baked, length)
                .unwrap();
            let pattern: DateTimePattern = pattern.parse().unwrap();
            let datetime = DateTime {
                date: Date::try_new_gregorian(2023, 11, 17).unwrap(),
                time: Time::try_new(13, 41, 28, 0).unwrap(),
            };
            let formatted_pattern = names.with_pattern_unchecked(&pattern).format(&datetime);

            assert_try_writeable_eq!(formatted_pattern, expected, Ok(()), "{cas:?}");
        }
    }

    #[test]
    fn test_weekday_coverage() {
        let locale = locale!("uk").into();
        #[derive(Debug)]
        struct TestCase {
            pattern: &'static str,
            length: WeekdayNameLength,
            expected: &'static str,
        }
        let cases = [
            TestCase {
                pattern: "<E>",
                length: WeekdayNameLength::Abbreviated,
                expected: "<пт>",
            },
            TestCase {
                pattern: "<EE>",
                length: WeekdayNameLength::Abbreviated,
                expected: "<пт>",
            },
            TestCase {
                pattern: "<EEE>",
                length: WeekdayNameLength::Abbreviated,
                expected: "<пт>",
            },
            TestCase {
                pattern: "<EEEE>",
                length: WeekdayNameLength::Wide,
                expected: "<пʼятниця>",
            },
            TestCase {
                pattern: "<EEEEE>",
                length: WeekdayNameLength::Narrow,
                expected: "<П>",
            },
            TestCase {
                pattern: "<EEEEEE>",
                length: WeekdayNameLength::Short,
                expected: "<пт>",
            },
            // 'e' and 'ee' are numeric
            TestCase {
                pattern: "<eee>",
                length: WeekdayNameLength::Abbreviated,
                expected: "<пт>",
            },
            TestCase {
                pattern: "<eeee>",
                length: WeekdayNameLength::Wide,
                expected: "<пʼятниця>",
            },
            TestCase {
                pattern: "<eeeee>",
                length: WeekdayNameLength::Narrow,
                expected: "<П>",
            },
            TestCase {
                pattern: "<eeeeee>",
                length: WeekdayNameLength::Short,
                expected: "<пт>",
            },
            // 'c' and 'cc' are numeric
            TestCase {
                pattern: "<ccc>",
                length: WeekdayNameLength::StandaloneAbbreviated,
                expected: "<пт>",
            },
            TestCase {
                pattern: "<cccc>",
                length: WeekdayNameLength::StandaloneWide,
                expected: "<пʼятниця>",
            },
            TestCase {
                pattern: "<ccccc>",
                length: WeekdayNameLength::StandaloneNarrow,
                expected: "<П>",
            },
            TestCase {
                pattern: "<cccccc>",
                length: WeekdayNameLength::StandaloneShort,
                expected: "<пт>",
            },
        ];
        for cas in cases {
            let TestCase {
                pattern,
                length,
                expected,
            } = cas;
            let mut names: FixedCalendarDateTimeNames<Gregorian> =
                FixedCalendarDateTimeNames::try_new(locale).unwrap();
            names
                .load_weekday_names(&crate::provider::Baked, length)
                .unwrap();
            let pattern: DateTimePattern = pattern.parse().unwrap();
            let datetime = DateTime {
                date: Date::try_new_gregorian(2023, 11, 17).unwrap(),
                time: Time::try_new(13, 41, 28, 0).unwrap(),
            };
            let formatted_pattern = names.with_pattern_unchecked(&pattern).format(&datetime);

            assert_try_writeable_eq!(formatted_pattern, expected, Ok(()), "{cas:?}");
        }
    }

    #[test]
    fn test_dayperiod_coverage() {
        // Thai has different values for different lengths of day periods
        // TODO(#487): Support flexible day periods, too
        let locale = locale!("th").into();
        #[derive(Debug)]
        struct TestCase {
            pattern: &'static str,
            length: DayPeriodNameLength,
            expected: &'static str,
        }
        let cases = [
            TestCase {
                pattern: "<a>",
                length: DayPeriodNameLength::Abbreviated,
                expected: "<PM>",
            },
            TestCase {
                pattern: "<aa>",
                length: DayPeriodNameLength::Abbreviated,
                expected: "<PM>",
            },
            TestCase {
                pattern: "<aaa>",
                length: DayPeriodNameLength::Abbreviated,
                expected: "<PM>",
            },
            TestCase {
                pattern: "<aaaa>",
                length: DayPeriodNameLength::Wide,
                expected: "<หลังเที่ยง>",
            },
            TestCase {
                pattern: "<aaaaa>",
                length: DayPeriodNameLength::Narrow,
                expected: "<p>",
            },
            TestCase {
                pattern: "<b>",
                length: DayPeriodNameLength::Abbreviated,
                expected: "<PM>",
            },
            TestCase {
                pattern: "<bb>",
                length: DayPeriodNameLength::Abbreviated,
                expected: "<PM>",
            },
            TestCase {
                pattern: "<bbb>",
                length: DayPeriodNameLength::Abbreviated,
                expected: "<PM>",
            },
            TestCase {
                pattern: "<bbbb>",
                length: DayPeriodNameLength::Wide,
                expected: "<หลังเที่ยง>",
            },
            TestCase {
                pattern: "<bbbbb>",
                length: DayPeriodNameLength::Narrow,
                expected: "<p>",
            },
        ];
        for cas in cases {
            let TestCase {
                pattern,
                length,
                expected,
            } = cas;
            let mut names: FixedCalendarDateTimeNames<Gregorian> =
                FixedCalendarDateTimeNames::try_new(locale).unwrap();
            names
                .load_day_period_names(&crate::provider::Baked, length)
                .unwrap();
            let pattern: DateTimePattern = pattern.parse().unwrap();
            let datetime = DateTime {
                date: Date::try_new_gregorian(2023, 11, 17).unwrap(),
                time: Time::try_new(13, 41, 28, 0).unwrap(),
            };
            let formatted_pattern = names.with_pattern_unchecked(&pattern).format(&datetime);

            assert_try_writeable_eq!(formatted_pattern, expected, Ok(()), "{cas:?}");
        }
    }
}
