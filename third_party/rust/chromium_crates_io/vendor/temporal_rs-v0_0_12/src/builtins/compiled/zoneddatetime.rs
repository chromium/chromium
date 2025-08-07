use crate::builtins::zoneddatetime::ZonedDateTimeFields;
use crate::builtins::TZ_PROVIDER;
use crate::partial::PartialZonedDateTime;
use crate::provider::TransitionDirection;
use crate::ZonedDateTime;
use crate::{
    options::{
        ArithmeticOverflow, DifferenceSettings, Disambiguation, DisplayCalendar, DisplayOffset,
        DisplayTimeZone, OffsetDisambiguation, RoundingOptions, ToStringRoundingOptions,
    },
    Duration, MonthCode, PlainDate, PlainDateTime, PlainTime, TemporalResult,
};
use alloc::string::String;
use tinystr::TinyAsciiStr;

impl core::fmt::Display for ZonedDateTime {
    /// The [`core::fmt::Display`] implementation for `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str(
            &self
                .to_ixdtf_string(
                    DisplayOffset::Auto,
                    DisplayTimeZone::Auto,
                    DisplayCalendar::Auto,
                    ToStringRoundingOptions::default(),
                )
                .expect("A valid ZonedDateTime string with default options."),
        )
    }
}

// ===== Experimental TZ_PROVIDER accessor implementations =====

/// `ZonedDateTime` methods for accessing primary date/time unit fields.
///
/// The following [`ZonedDateTime`] methods are feature gated behind the
/// `compiled_data` feature flag.
impl ZonedDateTime {
    /// Returns the `ZonedDateTime`'s calendar year.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn year(&self) -> TemporalResult<i32> {
        self.year_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the `ZonedDateTime`'s calendar month.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn month(&self) -> TemporalResult<u8> {
        self.month_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the `ZonedDateTime`'s calendar month code.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn month_code(&self) -> TemporalResult<MonthCode> {
        self.month_code_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the `ZonedDateTime`'s calendar day.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn day(&self) -> TemporalResult<u8> {
        self.day_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the `ZonedDateTime`'s hour.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn hour(&self) -> TemporalResult<u8> {
        self.hour_with_provider(&*TZ_PROVIDER)
    }

    /// Enable with the `compiled_data` feature flag.
    pub fn minute(&self) -> TemporalResult<u8> {
        self.minute_with_provider(&*TZ_PROVIDER)
    }

    /// Enable with the `compiled_data` feature flag.
    pub fn second(&self) -> TemporalResult<u8> {
        self.second_with_provider(&*TZ_PROVIDER)
    }

    /// Enable with the `compiled_data` feature flag.
    pub fn millisecond(&self) -> TemporalResult<u16> {
        self.millisecond_with_provider(&*TZ_PROVIDER)
    }

    /// Enable with the `compiled_data` feature flag.
    pub fn microsecond(&self) -> TemporalResult<u16> {
        self.microsecond_with_provider(&*TZ_PROVIDER)
    }

    /// Enable with the `compiled_data` feature flag.
    pub fn nanosecond(&self) -> TemporalResult<u16> {
        self.nanosecond_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the current offset as a formatted offset string.
    pub fn offset(&self) -> TemporalResult<String> {
        self.offset_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the current offset in nanoseconds
    pub fn offset_nanoseconds(&self) -> TemporalResult<i64> {
        self.offset_nanoseconds_with_provider(&*TZ_PROVIDER)
    }
}

// ==== Experimental TZ_PROVIDER calendar method implementations ====

/// Calendar method implementations for `ZonedDateTime`.
///
/// The following [`ZonedDateTime`] methods are feature gated behind the
/// `compiled_data` feature flag.
impl ZonedDateTime {
    /// Returns the era for the current `ZonedDateTime`
    ///
    /// Enable with the `compiled_data` feature flag.
    ///
    /// # Experimental
    ///
    /// Please note that era support is still experimental. Use with caution.
    pub fn era(&self) -> TemporalResult<Option<TinyAsciiStr<16>>> {
        self.era_with_provider(&*TZ_PROVIDER)
    }

    /// Return the era year for the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    ///
    /// # Experimental
    ///
    /// Please note that era year support is still experimental. Use with caution.
    pub fn era_year(&self) -> TemporalResult<Option<i32>> {
        self.era_year_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the calendar day of week value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn day_of_week(&self) -> TemporalResult<u16> {
        self.day_of_week_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the calendar day of year value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn day_of_year(&self) -> TemporalResult<u16> {
        self.day_of_year_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the calendar week of year value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn week_of_year(&self) -> TemporalResult<Option<u8>> {
        self.week_of_year_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the calendar year of week value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn year_of_week(&self) -> TemporalResult<Option<i32>> {
        self.year_of_week_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the calendar days in week value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn days_in_week(&self) -> TemporalResult<u16> {
        self.days_in_week_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the calendar days in month value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn days_in_month(&self) -> TemporalResult<u16> {
        self.days_in_month_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the calendar days in year value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn days_in_year(&self) -> TemporalResult<u16> {
        self.days_in_year_with_provider(&*TZ_PROVIDER)
    }

    /// Returns the calendar months in year value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn months_in_year(&self) -> TemporalResult<u16> {
        self.months_in_year_with_provider(&*TZ_PROVIDER)
    }

    /// Returns returns whether the date in a leap year for the given calendar.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn in_leap_year(&self) -> TemporalResult<bool> {
        self.in_leap_year_with_provider(&*TZ_PROVIDER)
    }

    // TODO: Update direction to correct option
    pub fn get_time_zone_transition(
        &self,
        direction: TransitionDirection,
    ) -> TemporalResult<Option<Self>> {
        self.get_time_zone_transition_with_provider(direction, &*TZ_PROVIDER)
    }

    /// Returns the hours in the day.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn hours_in_day(&self) -> TemporalResult<u8> {
        self.hours_in_day_with_provider(&*TZ_PROVIDER)
    }
}

// ==== Experimental TZ_PROVIDER method implementations ====

/// The primary `ZonedDateTime` method implementations.
///
/// The following [`ZonedDateTime`] methods are feature gated behind the
/// `compiled_data` feature flag.
impl ZonedDateTime {
    #[inline]
    pub fn from_partial(
        partial: PartialZonedDateTime,
        overflow: Option<ArithmeticOverflow>,
        disambiguation: Option<Disambiguation>,
        offset_option: Option<OffsetDisambiguation>,
    ) -> TemporalResult<Self> {
        Self::from_partial_with_provider(
            partial,
            overflow,
            disambiguation,
            offset_option,
            &*crate::builtins::TZ_PROVIDER,
        )
    }

    #[inline]
    pub fn with(
        &self,
        fields: ZonedDateTimeFields,
        disambiguation: Option<Disambiguation>,
        offset_option: Option<OffsetDisambiguation>,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        self.with_with_provider(
            fields,
            disambiguation,
            offset_option,
            overflow,
            &*TZ_PROVIDER,
        )
    }

    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime` with the provided `PlainTime`.
    ///
    /// combined with the provided `TimeZone`.
    pub fn with_plain_time(&self, time: Option<PlainTime>) -> TemporalResult<Self> {
        self.with_plain_time_and_provider(time, &*TZ_PROVIDER)
    }

    /// Adds a [`Duration`] to the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn add(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        self.add_with_provider(duration, overflow, &*TZ_PROVIDER)
    }

    /// Subtracts a [`Duration`] to the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn subtract(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        self.subtract_with_provider(duration, overflow, &*TZ_PROVIDER)
    }

    pub fn equals(&self, other: &Self) -> TemporalResult<bool> {
        self.equals_with_provider(other, &*TZ_PROVIDER)
    }

    /// Returns a [`Duration`] representing the period of time from this `ZonedDateTime` since the other `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn since(&self, other: &Self, options: DifferenceSettings) -> TemporalResult<Duration> {
        self.since_with_provider(other, options, &*TZ_PROVIDER)
    }

    /// Returns a [`Duration`] representing the period of time from this `ZonedDateTime` since the other `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn until(&self, other: &Self, options: DifferenceSettings) -> TemporalResult<Duration> {
        self.until_with_provider(other, options, &*TZ_PROVIDER)
    }

    /// Returns the start of day for the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn start_of_day(&self) -> TemporalResult<Self> {
        self.start_of_day_with_provider(&*TZ_PROVIDER)
    }

    /// Creates a new [`PlainDate`] from this `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn to_plain_date(&self) -> TemporalResult<PlainDate> {
        self.to_plain_date_with_provider(&*TZ_PROVIDER)
    }

    /// Creates a new [`PlainTime`] from this `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn to_plain_time(&self) -> TemporalResult<PlainTime> {
        self.to_plain_time_with_provider(&*TZ_PROVIDER)
    }

    /// Creates a new [`PlainDateTime`] from this `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn to_plain_datetime(&self) -> TemporalResult<PlainDateTime> {
        self.to_plain_datetime_with_provider(&*TZ_PROVIDER)
    }

    /// Rounds this [`ZonedDateTime`] to the nearest value according to the given rounding options.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn round(&self, options: RoundingOptions) -> TemporalResult<Self> {
        self.round_with_provider(options, &*TZ_PROVIDER)
    }

    /// Returns a RFC9557 (IXDTF) string with the provided options.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn to_ixdtf_string(
        &self,
        display_offset: DisplayOffset,
        display_timezone: DisplayTimeZone,
        display_calendar: DisplayCalendar,
        options: ToStringRoundingOptions,
    ) -> TemporalResult<String> {
        self.to_ixdtf_string_with_provider(
            display_offset,
            display_timezone,
            display_calendar,
            options,
            &*TZ_PROVIDER,
        )
    }

    /// Attempts to parse and create a `ZonedDateTime` from an IXDTF formatted [`&str`].
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn from_utf8(
        source: &[u8],
        disambiguation: Disambiguation,
        offset_option: OffsetDisambiguation,
    ) -> TemporalResult<Self> {
        ZonedDateTime::from_utf8_with_provider(source, disambiguation, offset_option, &*TZ_PROVIDER)
    }
    /// Attempts to parse and create a `ZonedDateTime` from an IXDTF formatted [`&str`].
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn from_parsed(
        parsed: crate::parsed_intermediates::ParsedZonedDateTime,
        disambiguation: Disambiguation,
        offset_option: OffsetDisambiguation,
    ) -> TemporalResult<Self> {
        ZonedDateTime::from_parsed_with_provider(
            parsed,
            disambiguation,
            offset_option,
            &*TZ_PROVIDER,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::ZonedDateTime;
    use crate::options::{
        Disambiguation, DisplayCalendar, DisplayOffset, DisplayTimeZone, OffsetDisambiguation, Unit,
    };
    use crate::provider::TransitionDirection;
    use crate::Duration;
    use crate::TemporalResult;
    use alloc::string::ToString;

    #[cfg(not(target_os = "windows"))]
    #[test]
    fn static_tzdb_zdt_test() {
        use crate::{Calendar, TimeZone};
        use core::str::FromStr;

        let nov_30_2023_utc = 1_701_308_952_000_000_000i128;

        let zdt = ZonedDateTime::try_new(
            nov_30_2023_utc,
            Calendar::from_str("iso8601").unwrap(),
            TimeZone::try_from_str("UTC").unwrap(),
        )
        .unwrap();

        assert_eq!(zdt.year().unwrap(), 2023);
        assert_eq!(zdt.month().unwrap(), 11);
        assert_eq!(zdt.day().unwrap(), 30);
        assert_eq!(zdt.hour().unwrap(), 1);
        assert_eq!(zdt.minute().unwrap(), 49);
        assert_eq!(zdt.second().unwrap(), 12);

        let zdt_minus_five = ZonedDateTime::try_new(
            nov_30_2023_utc,
            Calendar::from_str("iso8601").unwrap(),
            TimeZone::try_from_str("America/New_York").unwrap(),
        )
        .unwrap();

        assert_eq!(zdt_minus_five.year().unwrap(), 2023);
        assert_eq!(zdt_minus_five.month().unwrap(), 11);
        assert_eq!(zdt_minus_five.day().unwrap(), 29);
        assert_eq!(zdt_minus_five.hour().unwrap(), 20);
        assert_eq!(zdt_minus_five.minute().unwrap(), 49);
        assert_eq!(zdt_minus_five.second().unwrap(), 12);

        let zdt_plus_eleven = ZonedDateTime::try_new(
            nov_30_2023_utc,
            Calendar::from_str("iso8601").unwrap(),
            TimeZone::try_from_str("Australia/Sydney").unwrap(),
        )
        .unwrap();

        assert_eq!(zdt_plus_eleven.year().unwrap(), 2023);
        assert_eq!(zdt_plus_eleven.month().unwrap(), 11);
        assert_eq!(zdt_plus_eleven.day().unwrap(), 30);
        assert_eq!(zdt_plus_eleven.hour().unwrap(), 12);
        assert_eq!(zdt_plus_eleven.minute().unwrap(), 49);
        assert_eq!(zdt_plus_eleven.second().unwrap(), 12);
    }

    #[cfg(not(target_os = "windows"))]
    #[test]
    fn basic_zdt_add() {
        use crate::{Calendar, Duration, TimeZone};

        let zdt =
            ZonedDateTime::try_new(-560174321098766, Calendar::default(), TimeZone::default())
                .unwrap();
        let d = Duration::new(
            0.into(),
            0.into(),
            0.into(),
            0.into(),
            240.into(),
            0.into(),
            0.into(),
            0.into(),
            0.into(),
            800.into(),
        )
        .unwrap();
        // "1970-01-04T12:23:45.678902034+00:00[UTC]"
        let expected =
            ZonedDateTime::try_new(303825678902034, Calendar::default(), TimeZone::default())
                .unwrap();

        let result = zdt.add(&d, None).unwrap();
        assert!(result.equals(&expected).unwrap());
    }

    fn parse_zdt_with_reject(s: &str) -> TemporalResult<ZonedDateTime> {
        ZonedDateTime::from_utf8(
            s.as_bytes(),
            Disambiguation::Reject,
            OffsetDisambiguation::Reject,
        )
    }

    #[test]
    fn test_pacific_niue() {
        // test/intl402/Temporal/ZonedDateTime/compare/sub-minute-offset.js
        // Pacific/Niue on October 15, 1952, where
        // the offset shifted by 20 seconds to a whole-minute boundary.
        //
        // The precise transition is from
        // 1952-10-15T23:59:59-11:19:40[-11:19:40] to 1952-10-15T23:59:40-11:19:00[-11:19:00]
        let ms_pre = -543_069_621_000;
        let zdt = parse_zdt_with_reject("1952-10-15T23:59:59-11:19:40[Pacific/Niue]").unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_pre,
            "-11:19:40 is accepted as -11:19:40 in Pacific/Niue edge case"
        );

        let zdt = parse_zdt_with_reject("1952-10-15T23:59:59-11:20[Pacific/Niue]").unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_pre,
            "-11:20 matches the first candidate -11:19:40 in the Pacific/Niue edge case"
        );

        let ms_post = -543_069_601_000;

        let zdt = parse_zdt_with_reject("1952-10-15T23:59:59-11:20:00[Pacific/Niue]").unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_post,
            "-11:19:40 is accepted as -11:19:40 in Pacific/Niue edge case"
        );

        // Additional tests ensuring that boundary cases are handled

        let zdt = parse_zdt_with_reject("1952-10-15T23:59:40-11:20:00[Pacific/Niue]").unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_post - 19_000,
            "Post-transition Niue time allows up to `1952-10-15T23:59:40`"
        );
        let zdt = parse_zdt_with_reject("1952-10-15T23:59:39-11:20:00[Pacific/Niue]");
        assert!(
            zdt.is_err(),
            "Post-transition Niue time does not allow times before `1952-10-15T23:59:40`"
        );

        let zdt = parse_zdt_with_reject("1952-10-15T23:59:40-11:19:40[Pacific/Niue]").unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_pre - 19_000,
            "Pre-transition Niue time also allows `1952-10-15T23:59:40`"
        );

        let zdt = parse_zdt_with_reject("1952-10-15T23:59:39-11:19:40[Pacific/Niue]").unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_pre - 20_000,
            "Pre-transition Niue time also allows `1952-10-15T23:59:39`"
        );

        // Tests without explicit offset
        let zdt = parse_zdt_with_reject("1952-10-15T23:59:39[Pacific/Niue]").unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_pre - 20_000,
            "Unambiguous before 1952-10-15T23:59:39"
        );

        let zdt = parse_zdt_with_reject("1952-10-16T00:00:00[Pacific/Niue]").unwrap();
        assert_eq!(
            zdt.epoch_milliseconds(),
            ms_post + 1_000,
            "Unambiguous after 1952-10-16T00:00:00"
        );

        let zdt = parse_zdt_with_reject("1952-10-15T23:59:40[Pacific/Niue]");
        assert!(zdt.is_err(), "Ambiguity starts at 1952-10-15T23:59:40");
        let zdt = parse_zdt_with_reject("1952-10-15T23:59:59[Pacific/Niue]");
        assert!(zdt.is_err(), "Ambiguity ends at 1952-10-15T23:59:59");
    }

    fn total_seconds_for_one_day(s: &str) -> TemporalResult<f64> {
        Ok(Duration::new(0, 0, 0, 1, 0, 0, 0, 0, 0, 0)
            .unwrap()
            .total(Unit::Second, Some(parse_zdt_with_reject(s).unwrap().into()))?
            .as_inner())
    }

    #[test]
    fn test_pacific_niue_duration() {
        // Also tests add_to_instant codepaths
        // From intl402/Temporal/Duration/prototype/total/relativeto-sub-minute-offset
        let total =
            total_seconds_for_one_day("1952-10-15T23:59:59-11:19:40[Pacific/Niue]").unwrap();
        assert_eq!(
            total, 86420.,
            "-11:19:40 is accepted as -11:19:40 in Pacific/Niue edge case"
        );

        let total = total_seconds_for_one_day("1952-10-15T23:59:59-11:20[Pacific/Niue]").unwrap();
        assert_eq!(
            total, 86420.,
            "-11:20 matches the first candidate -11:19:40 in the Pacific/Niue edge case"
        );

        let total =
            total_seconds_for_one_day("1952-10-15T23:59:59-11:20:00[Pacific/Niue]").unwrap();
        assert_eq!(
            total, 86400.,
            "-11:20:00 is accepted as -11:20:00 in the Pacific/Niue edge case"
        );
    }

    #[track_caller]
    fn assert_tr(zdt: &ZonedDateTime, direction: TransitionDirection, s: &str) {
        assert_eq!(
            zdt.get_time_zone_transition(direction)
                .unwrap()
                .unwrap()
                .to_string(),
            s
        );
    }

    // Modern dates

    // Transitions
    const DST_2025_03_09: &str = "2025-03-09T03:00:00-07:00[America/Los_Angeles]";
    const DST_2026_03_08: &str = "2026-03-08T03:00:00-07:00[America/Los_Angeles]";
    const STD_2025_11_02: &str = "2025-11-02T01:00:00-08:00[America/Los_Angeles]";
    const STD_2024_11_03: &str = "2024-11-03T01:00:00-08:00[America/Los_Angeles]";

    // Non transitions
    const IN_DST_2025_07_31: &str = "2025-07-31T00:00:00-07:00[America/Los_Angeles]";
    const AFTER_DST_2025_12_31: &str = "2025-12-31T00:00:00-08:00[America/Los_Angeles]";
    const BEFORE_DST_2025_01_31: &str = "2025-01-31T00:00:00-08:00[America/Los_Angeles]";

    // Transition dates ± 1
    const DST_2025_03_09_PLUS_ONE: &str =
        "2025-03-09T03:00:00.000000001-07:00[America/Los_Angeles]";
    const DST_2025_03_09_MINUS_ONE: &str =
        "2025-03-09T01:59:59.999999999-08:00[America/Los_Angeles]";
    const STD_2025_11_02_PLUS_ONE: &str =
        "2025-11-02T01:00:00.000000001-08:00[America/Los_Angeles]";
    const STD_2025_11_02_MINUS_ONE: &str =
        "2025-11-02T01:59:59.999999999-07:00[America/Los_Angeles]";

    // Dates from the tzif data block
    // Transitions
    const DST_1999_04_04: &str = "1999-04-04T03:00:00-07:00[America/Los_Angeles]";
    const DST_2000_04_02: &str = "2000-04-02T03:00:00-07:00[America/Los_Angeles]";
    const STD_1999_10_31: &str = "1999-10-31T01:00:00-08:00[America/Los_Angeles]";
    const STD_1998_01_31: &str = "1998-10-25T01:00:00-08:00[America/Los_Angeles]";

    // Non transitions
    const IN_DST_1999_07_31: &str = "1999-07-31T00:00:00-07:00[America/Los_Angeles]";
    const AFTER_DST_1999_12_31: &str = "1999-12-31T00:00:00-08:00[America/Los_Angeles]";
    const BEFORE_DST_1999_01_31: &str = "1999-01-31T00:00:00-08:00[America/Los_Angeles]";

    const LONDON_TRANSITION_1968_02_18: &str = "1968-02-18T03:00:00+01:00[Europe/London]";
    const LONDON_TRANSITION_1968_02_18_MINUS_ONE: &str =
        "1968-02-18T01:59:59.999999999+00:00[Europe/London]";

    const SAMOA_IDL_CHANGE: &str = "2011-12-31T00:00:00+14:00[Pacific/Apia]";
    const SAMOA_IDL_CHANGE_MINUS_ONE: &str = "2011-12-29T23:59:59.999999999-10:00[Pacific/Apia]";

    // MUST only contain full strings
    // The second boolean is whether these are unambiguous when the offset is removed
    // As a rule of thumb, anything around an STD->DST transition
    // will be unambiguous, but DST->STD will not
    const TO_STRING_TESTCASES: &[(&str, bool)] = &[
        (DST_2025_03_09, true),
        (DST_2026_03_08, true),
        (STD_2025_11_02, false),
        (STD_2024_11_03, false),
        (IN_DST_2025_07_31, true),
        (AFTER_DST_2025_12_31, true),
        (BEFORE_DST_2025_01_31, true),
        (DST_2025_03_09_PLUS_ONE, true),
        (DST_2025_03_09_MINUS_ONE, true),
        (STD_2025_11_02_PLUS_ONE, false),
        (STD_2025_11_02_MINUS_ONE, false),
        (DST_1999_04_04, true),
        (DST_2000_04_02, true),
        (STD_1999_10_31, false),
        (STD_1998_01_31, false),
        (IN_DST_1999_07_31, true),
        (AFTER_DST_1999_12_31, true),
        (BEFORE_DST_1999_01_31, true),
        (LONDON_TRANSITION_1968_02_18, true),
        (LONDON_TRANSITION_1968_02_18_MINUS_ONE, true),
        (SAMOA_IDL_CHANGE, true),
        (SAMOA_IDL_CHANGE_MINUS_ONE, true),
    ];

    #[test]
    fn get_time_zone_transition() {
        // This stops it from wrapping
        use TransitionDirection::*;

        // Modern dates that utilize the posix string

        // During DST
        let zdt = parse_zdt_with_reject(IN_DST_2025_07_31).unwrap();
        assert_tr(&zdt, Previous, DST_2025_03_09);
        assert_tr(&zdt, Next, STD_2025_11_02);

        // After DST
        let zdt = parse_zdt_with_reject(AFTER_DST_2025_12_31).unwrap();
        assert_tr(&zdt, Previous, STD_2025_11_02);
        assert_tr(&zdt, Next, DST_2026_03_08);

        // Before DST
        let zdt = parse_zdt_with_reject(BEFORE_DST_2025_01_31).unwrap();
        assert_tr(&zdt, Previous, STD_2024_11_03);
        assert_tr(&zdt, Next, DST_2025_03_09);

        // Boundary test
        // Modern date (On start of DST)
        let zdt = parse_zdt_with_reject(DST_2025_03_09).unwrap();
        assert_tr(&zdt, Previous, STD_2024_11_03);
        assert_tr(&zdt, Next, STD_2025_11_02);
        // Modern date (one ns after DST)
        let zdt = parse_zdt_with_reject(DST_2025_03_09_PLUS_ONE).unwrap();
        assert_tr(&zdt, Previous, DST_2025_03_09);
        assert_tr(&zdt, Next, STD_2025_11_02);
        // Modern date (one ns before DST)
        let zdt = parse_zdt_with_reject(DST_2025_03_09_MINUS_ONE).unwrap();
        assert_tr(&zdt, Previous, STD_2024_11_03);
        assert_tr(&zdt, Next, DST_2025_03_09);

        // Modern date (On start of STD)
        let zdt = parse_zdt_with_reject(STD_2025_11_02).unwrap();
        assert_tr(&zdt, Previous, DST_2025_03_09);
        assert_tr(&zdt, Next, DST_2026_03_08);
        // Modern date (one ns after STD)
        let zdt = parse_zdt_with_reject(STD_2025_11_02_PLUS_ONE).unwrap();
        assert_tr(&zdt, Previous, STD_2025_11_02);
        assert_tr(&zdt, Next, DST_2026_03_08);
        // Modern date (one ns before STD)
        let zdt = parse_zdt_with_reject(STD_2025_11_02_MINUS_ONE).unwrap();
        assert_tr(&zdt, Previous, DST_2025_03_09);
        assert_tr(&zdt, Next, STD_2025_11_02);

        // Old dates using the Tzif data

        // During DST
        let zdt = parse_zdt_with_reject(IN_DST_1999_07_31).unwrap();
        assert_tr(&zdt, Previous, DST_1999_04_04);
        assert_tr(&zdt, Next, STD_1999_10_31);

        // After DST
        let zdt = parse_zdt_with_reject(AFTER_DST_1999_12_31).unwrap();
        assert_tr(&zdt, Previous, STD_1999_10_31);
        assert_tr(&zdt, Next, DST_2000_04_02);

        // Before DST
        let zdt = parse_zdt_with_reject(BEFORE_DST_1999_01_31).unwrap();
        assert_tr(&zdt, Previous, STD_1998_01_31);
        assert_tr(&zdt, Next, DST_1999_04_04);

        // Test case from intl402/Temporal/ZonedDateTime/prototype/getTimeZoneTransition/rule-change-without-offset-transition
        // This ensures we skip "fake" transition entries that do not actually change the offset

        let zdt = parse_zdt_with_reject("1970-01-01T01:00:00+01:00[Europe/London]").unwrap();
        assert_tr(&zdt, Previous, LONDON_TRANSITION_1968_02_18);
        let zdt = parse_zdt_with_reject("1968-10-01T00:00:00+01:00[Europe/London]").unwrap();
        assert_tr(&zdt, Next, "1971-10-31T02:00:00+00:00[Europe/London]");
        let zdt = parse_zdt_with_reject("1967-05-01T00:00:00-10:00[America/Anchorage]").unwrap();
        assert_tr(
            &zdt,
            Previous,
            "1945-09-30T01:00:00-10:00[America/Anchorage]",
        );
        let zdt = parse_zdt_with_reject("1967-01-01T00:00:00-10:00[America/Anchorage]").unwrap();
        assert_tr(&zdt, Next, "1969-04-27T03:00:00-09:00[America/Anchorage]");
        // These dates are one second after a "fake" transition at the end of the tzif data
        // Ensure that they find a real transition, not the fake one
        let zdt = parse_zdt_with_reject("2020-11-01T00:00:01-07:00[America/Whitehorse]").unwrap();
        assert_tr(
            &zdt,
            Previous,
            "2020-03-08T03:00:00-07:00[America/Whitehorse]",
        );
        let zdt = parse_zdt_with_reject("1996-05-13T00:00:01+03:00[Europe/Kyiv]").unwrap();
        assert_tr(&zdt, Previous, "1996-03-31T03:00:00+03:00[Europe/Kyiv]");

        // This ensures that nanosecond-to-second casting works correctly
        let zdt = parse_zdt_with_reject(LONDON_TRANSITION_1968_02_18_MINUS_ONE).unwrap();
        assert_tr(&zdt, Next, LONDON_TRANSITION_1968_02_18);
        assert_tr(&zdt, Previous, "1967-10-29T02:00:00+00:00[Europe/London]");
    }

    #[test]
    fn test_to_string_roundtrip() {
        for (test, is_unambiguous) in TO_STRING_TESTCASES {
            let zdt = parse_zdt_with_reject(test).expect(test);
            let string = zdt.to_string();

            assert_eq!(
                *test, &*string,
                "ZonedDateTime {test} round trips on ToString"
            );
            let without_offset = zdt
                .to_ixdtf_string(
                    DisplayOffset::Never,
                    DisplayTimeZone::Auto,
                    DisplayCalendar::Never,
                    Default::default(),
                )
                .unwrap();
            assert_eq!(
                without_offset[0..19],
                test[0..19],
                "Stringified object should have same date part"
            );

            // These testcases should all also parse unambiguously when the offset is removed.
            if *is_unambiguous {
                let zdt = parse_zdt_with_reject(&without_offset).expect(test);
                let string = zdt.to_string();
                assert_eq!(
                    *test, &*string,
                    "ZonedDateTime {without_offset} round trips to {test} on ToString"
                );
            }
        }
    }

    #[test]
    fn test_apia() {
        // This transition skips an entire day
        // From: 2011-12-29T23:59:59.999999999-10:00[Pacific/Apia]
        // To: 2011-12-31T00:00:00+14:00[Pacific/Apia]
        let zdt = parse_zdt_with_reject(SAMOA_IDL_CHANGE).unwrap();
        let _ = zdt
            .add(&Duration::new(0, 0, 0, 1, 1, 0, 0, 0, 0, 0).unwrap(), None)
            .unwrap();

        assert_eq!(zdt.hours_in_day().unwrap(), 24);

        let samoa_before = parse_zdt_with_reject(SAMOA_IDL_CHANGE_MINUS_ONE).unwrap();
        assert_eq!(samoa_before.hours_in_day().unwrap(), 24);
    }
}
