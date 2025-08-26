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
    Calendar, Duration, PlainTime, TemporalResult, TimeZone,
};
use alloc::string::String;

impl core::fmt::Display for ZonedDateTime {
    /// The [`core::fmt::Display`] implementation for `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        let string = self.to_ixdtf_string(
            DisplayOffset::Auto,
            DisplayTimeZone::Auto,
            DisplayCalendar::Auto,
            ToStringRoundingOptions::default(),
        );
        debug_assert!(
            string.is_ok(),
            "A valid ZonedDateTime string with default options."
        );
        f.write_str(&string.map_err(|_| Default::default())?)
    }
}

// ==== Experimental TZ_PROVIDER calendar method implementations ====

/// Calendar method implementations for `ZonedDateTime`.
///
/// The following [`ZonedDateTime`] methods are feature gated behind the
/// `compiled_data` feature flag.
impl ZonedDateTime {
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
    pub fn hours_in_day(&self) -> TemporalResult<f64> {
        self.hours_in_day_with_provider(&*TZ_PROVIDER)
    }
}

// ==== Experimental TZ_PROVIDER method implementations ====

/// The primary `ZonedDateTime` method implementations.
///
/// The following [`ZonedDateTime`] methods are feature gated behind the
/// `compiled_data` feature flag.
impl ZonedDateTime {
    /// Creates a new valid `ZonedDateTime`.
    #[inline]
    pub fn try_new(nanos: i128, calendar: Calendar, time_zone: TimeZone) -> TemporalResult<Self> {
        Self::try_new_with_provider(nanos, calendar, time_zone, &*TZ_PROVIDER)
    }
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

    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime`
    /// combined with the provided `TimeZone`.
    pub fn with_timezone(&self, timezone: TimeZone) -> TemporalResult<Self> {
        self.with_timezone_with_provider(timezone, &*TZ_PROVIDER)
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
        DifferenceSettings, Disambiguation, DisplayCalendar, DisplayOffset, DisplayTimeZone,
        OffsetDisambiguation, RoundingMode, RoundingOptions, Unit,
    };
    use crate::provider::TransitionDirection;
    use crate::Calendar;
    use crate::Duration;
    use crate::TemporalResult;
    use crate::TimeZone;
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

    // This date is a fifth Sunday
    const LONDON_POSIX_TRANSITION_2019_03_31_MINUS_ONE: &str =
        "2019-03-31T00:59:59.999999999+00:00[Europe/London]";
    const LONDON_POSIX_TRANSITION_2019_03_31: &str = "2019-03-31T02:00:00+01:00[Europe/London]";

    // This date is a fourth (but last) Sunday
    const LONDON_POSIX_TRANSITION_2017_03_26_MINUS_ONE: &str =
        "2017-03-26T00:59:59.999999999+00:00[Europe/London]";
    const LONDON_POSIX_TRANSITION_2017_03_26: &str = "2017-03-26T02:00:00+01:00[Europe/London]";

    const TROLL_FIRST_TRANSITION: &str = "2005-03-27T03:00:00+02:00[Antarctica/Troll]";

    /// Vancouver transitions on the first Sunday in November, which may or may not be
    /// before the first Friday in November
    const VANCOUVER_FIRST_FRIDAY_IN_NOVEMBER_BEFORE_SUNDAY: &str =
        "2019-11-01T00:00:00-07:00[America/Vancouver]";
    const VANCOUVER_FIRST_FRIDAY_IN_NOVEMBER_AFTER_SUNDAY: &str =
        "2019-11-06T00:00:00-08:00[America/Vancouver]";

    /// Chile tzdb has a transition on the "first saturday in april", except the transition occurs
    /// at 24:00:00, which is, of course, the day after. This is not the same thing as the first Sunday in April
    const SANTIAGO_DST_2024: &str = "2024-09-08T01:00:00-03:00[America/Santiago]";
    const SANTIAGO_STD_2025_APRIL: &str = "2025-04-05T23:00:00-04:00[America/Santiago]";
    const SANTIAGO_STD_2025_APRIL_PLUS_ONE: &str =
        "2025-04-05T23:00:00.000000001-04:00[America/Santiago]";
    const SANTIAGO_STD_2025_APRIL_MINUS_ONE: &str =
        "2025-04-05T23:59:59.999999999-03:00[America/Santiago]";
    const SANTIAGO_DST_2025_SEPT: &str = "2025-09-07T01:00:00-03:00[America/Santiago]";
    const SANTIAGO_DST_2025_SEPT_PLUS_ONE: &str =
        "2025-09-07T01:00:00.000000001-03:00[America/Santiago]";
    const SANTIAGO_DST_2025_SEPT_MINUS_ONE: &str =
        "2025-09-06T23:59:59.999999999-04:00[America/Santiago]";
    const SANTIAGO_STD_2026: &str = "2026-04-04T23:00:00-04:00[America/Santiago]";

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
        (LONDON_POSIX_TRANSITION_2019_03_31_MINUS_ONE, true),
        (LONDON_POSIX_TRANSITION_2019_03_31, true),
        (LONDON_POSIX_TRANSITION_2017_03_26_MINUS_ONE, true),
        (LONDON_POSIX_TRANSITION_2017_03_26_MINUS_ONE, true),
        (TROLL_FIRST_TRANSITION, true),
        (VANCOUVER_FIRST_FRIDAY_IN_NOVEMBER_BEFORE_SUNDAY, true),
        (VANCOUVER_FIRST_FRIDAY_IN_NOVEMBER_AFTER_SUNDAY, true),
        (SANTIAGO_DST_2024, true),
        (SANTIAGO_STD_2025_APRIL, false),
        (SANTIAGO_STD_2025_APRIL_PLUS_ONE, false),
        (SANTIAGO_STD_2025_APRIL_MINUS_ONE, false),
        (SANTIAGO_DST_2025_SEPT, true),
        (SANTIAGO_DST_2025_SEPT_PLUS_ONE, true),
        (SANTIAGO_DST_2025_SEPT_MINUS_ONE, true),
        (SANTIAGO_STD_2026, false),
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

        // Santiago tests, testing that transitions with the offset = 24:00:00
        // still work
        let zdt = parse_zdt_with_reject(SANTIAGO_DST_2025_SEPT_MINUS_ONE).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_STD_2025_APRIL);
        assert_tr(&zdt, Next, SANTIAGO_DST_2025_SEPT);
        let zdt = parse_zdt_with_reject(SANTIAGO_DST_2025_SEPT).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_STD_2025_APRIL);
        assert_tr(&zdt, Next, SANTIAGO_STD_2026);
        let zdt = parse_zdt_with_reject(SANTIAGO_DST_2025_SEPT_PLUS_ONE).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_DST_2025_SEPT);
        assert_tr(&zdt, Next, SANTIAGO_STD_2026);

        let zdt = parse_zdt_with_reject(SANTIAGO_STD_2025_APRIL_MINUS_ONE).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_DST_2024);
        assert_tr(&zdt, Next, SANTIAGO_STD_2025_APRIL);
        let zdt = parse_zdt_with_reject(SANTIAGO_STD_2025_APRIL).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_DST_2024);
        assert_tr(&zdt, Next, SANTIAGO_DST_2025_SEPT);
        let zdt = parse_zdt_with_reject(SANTIAGO_STD_2025_APRIL_PLUS_ONE).unwrap();
        assert_tr(&zdt, Previous, SANTIAGO_STD_2025_APRIL);
        assert_tr(&zdt, Next, SANTIAGO_DST_2025_SEPT);

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

        assert_eq!(zdt.hours_in_day().unwrap(), 24.);

        let samoa_before = parse_zdt_with_reject(SAMOA_IDL_CHANGE_MINUS_ONE).unwrap();
        assert_eq!(samoa_before.hours_in_day().unwrap(), 24.);
    }

    #[test]
    fn test_london() {
        // Europe/London has an MWD transition where the transitions are on the
        // last sundays of March and October.

        // Test that they correctly compute from nanoseconds
        let zdt = ZonedDateTime::try_new(
            1_553_993_999_999_999_999,
            Calendar::ISO,
            TimeZone::try_from_str("Europe/London").unwrap(),
        )
        .unwrap();
        assert_eq!(
            zdt.to_string(),
            LONDON_POSIX_TRANSITION_2019_03_31_MINUS_ONE,
        );
        let zdt = ZonedDateTime::try_new(
            1_553_994_000_000_000_000,
            Calendar::ISO,
            TimeZone::try_from_str("Europe/London").unwrap(),
        )
        .unwrap();
        assert_eq!(zdt.to_string(), LONDON_POSIX_TRANSITION_2019_03_31,);

        // Test that they correctly compute from ZDT strings without explicit offset
        let zdt = parse_zdt_with_reject("2019-03-31T00:59:59.999999999[Europe/London]").unwrap();
        assert_eq!(
            zdt.to_string(),
            LONDON_POSIX_TRANSITION_2019_03_31_MINUS_ONE
        );

        let zdt = parse_zdt_with_reject("2019-03-31T02:00:00+01:00[Europe/London]").unwrap();
        assert_eq!(zdt.to_string(), LONDON_POSIX_TRANSITION_2019_03_31);

        let zdt = parse_zdt_with_reject("2017-03-26T00:59:59.999999999[Europe/London]").unwrap();
        assert_eq!(
            zdt.to_string(),
            LONDON_POSIX_TRANSITION_2017_03_26_MINUS_ONE
        );

        let zdt = parse_zdt_with_reject("2017-03-26T02:00:00+01:00[Europe/London]").unwrap();
        assert_eq!(zdt.to_string(), LONDON_POSIX_TRANSITION_2017_03_26);
    }

    #[test]
    fn test_berlin() {
        // Need to ensure that when the transition is the last day of the month it still works
        let zdt = parse_zdt_with_reject("2021-03-28T01:00:00Z[Europe/Berlin]").unwrap();
        std::println!("GET");
        let prev = zdt
            .get_time_zone_transition(TransitionDirection::Previous)
            .unwrap()
            .unwrap();

        assert_eq!(prev.to_string(), "2020-10-25T02:00:00+01:00[Europe/Berlin]");
    }

    #[test]
    fn test_troll() {
        // Antarctica/Troll started DST in 2005, but had no other transitions before that
        let zdt = ZonedDateTime::try_new(
            0,
            Calendar::ISO,
            TimeZone::try_from_str("Antarctica/Troll").unwrap(),
        )
        .unwrap();

        let next = zdt
            .get_time_zone_transition(TransitionDirection::Next)
            .unwrap()
            .unwrap();
        assert_eq!(next.to_string(), TROLL_FIRST_TRANSITION);
    }

    #[test]
    fn test_zdt_until_rounding() {
        // Regression test for beyondDaySpan rounding behavior
        let start = parse_zdt_with_reject("2020-01-01T00:00-08:00[-08:00]").unwrap();
        let end = parse_zdt_with_reject("2020-01-03T23:59-08:00[-08:00]").unwrap();
        let difference = start
            .until(
                &end,
                DifferenceSettings {
                    largest_unit: Some(Unit::Day),
                    smallest_unit: Some(Unit::Hour),
                    rounding_mode: Some(RoundingMode::HalfExpand),
                    ..Default::default()
                },
            )
            .unwrap();
        assert_eq!(difference.to_string(), "P3D");
    }

    #[test]
    fn test_toronto_half_hour() {
        let zdt = parse_zdt_with_reject("1919-03-30T12:00:00-05:00[America/Toronto]").unwrap();
        assert_eq!(zdt.hours_in_day().unwrap(), 23.5);
    }

    #[test]
    fn test_round_to_start_of_day() {
        // Round up to DST
        let zdt = parse_zdt_with_reject("1919-03-30T11:45-05:00[America/Toronto]").unwrap();
        let rounded = zdt
            .round(RoundingOptions {
                smallest_unit: Some(Unit::Day),
                ..Default::default()
            })
            .unwrap();
        let known_rounded =
            parse_zdt_with_reject("1919-03-31T00:30:00-04:00[America/Toronto]").unwrap();

        assert!(
            rounded.equals(&known_rounded).unwrap(),
            "Expected {known_rounded}, found {rounded}"
        );
        assert_eq!(rounded.get_iso_datetime(), known_rounded.get_iso_datetime());

        // Round down (ensure the offset picked is the correct one)
        // See https://github.com/boa-dev/temporal/pull/520
        let zdt = parse_zdt_with_reject("1919-03-30T01:45-05:00[America/Toronto]").unwrap();
        let rounded = zdt
            .round(RoundingOptions {
                smallest_unit: Some(Unit::Day),
                ..Default::default()
            })
            .unwrap();
        let known_rounded =
            parse_zdt_with_reject("1919-03-30T00:00:00-05:00[America/Toronto]").unwrap();

        assert!(
            rounded.equals(&known_rounded).unwrap(),
            "Expected {known_rounded}, found {rounded}"
        );
        assert_eq!(rounded.get_iso_datetime(), known_rounded.get_iso_datetime());
    }
}
