use crate::builtins::TZ_PROVIDER;
use crate::provider::TransitionDirection;
use crate::ZonedDateTime;
use crate::{
    options::{
        ArithmeticOverflow, DifferenceSettings, Disambiguation, DisplayCalendar, DisplayOffset,
        DisplayTimeZone, OffsetDisambiguation, RoundingOptions, ToStringRoundingOptions,
    },
    Duration, MonthCode, PlainDate, PlainDateTime, PlainTime, TemporalError, TemporalResult,
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
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.year_with_provider(&*provider)
    }

    /// Returns the `ZonedDateTime`'s calendar month.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn month(&self) -> TemporalResult<u8> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.month_with_provider(&*provider)
    }

    /// Returns the `ZonedDateTime`'s calendar month code.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn month_code(&self) -> TemporalResult<MonthCode> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.month_code_with_provider(&*provider)
    }

    /// Returns the `ZonedDateTime`'s calendar day.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn day(&self) -> TemporalResult<u8> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.day_with_provider(&*provider)
    }

    /// Returns the `ZonedDateTime`'s hour.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn hour(&self) -> TemporalResult<u8> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.hour_with_provider(&*provider)
    }

    /// Enable with the `compiled_data` feature flag.
    pub fn minute(&self) -> TemporalResult<u8> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.minute_with_provider(&*provider)
    }

    /// Enable with the `compiled_data` feature flag.
    pub fn second(&self) -> TemporalResult<u8> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.second_with_provider(&*provider)
    }

    /// Enable with the `compiled_data` feature flag.
    pub fn millisecond(&self) -> TemporalResult<u16> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.millisecond_with_provider(&*provider)
    }

    /// Enable with the `compiled_data` feature flag.
    pub fn microsecond(&self) -> TemporalResult<u16> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.millisecond_with_provider(&*provider)
    }

    /// Enable with the `compiled_data` feature flag.
    pub fn nanosecond(&self) -> TemporalResult<u16> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;

        self.millisecond_with_provider(&*provider)
    }

    /// Returns the current offset as a formatted offset string.
    pub fn offset(&self) -> TemporalResult<String> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.offset_with_provider(&*provider)
    }

    /// Returns the current offset in nanoseconds
    pub fn offset_nanoseconds(&self) -> TemporalResult<i64> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.offset_nanoseconds_with_provider(&*provider)
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
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.era_with_provider(&*provider)
    }

    /// Return the era year for the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    ///
    /// # Experimental
    ///
    /// Please note that era year support is still experimental. Use with caution.
    pub fn era_year(&self) -> TemporalResult<Option<i32>> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.era_year_with_provider(&*provider)
    }

    /// Returns the calendar day of week value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn day_of_week(&self) -> TemporalResult<u16> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.day_of_week_with_provider(&*provider)
    }

    /// Returns the calendar day of year value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn day_of_year(&self) -> TemporalResult<u16> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.day_of_year_with_provider(&*provider)
    }

    /// Returns the calendar week of year value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn week_of_year(&self) -> TemporalResult<Option<u8>> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.week_of_year_with_provider(&*provider)
    }

    /// Returns the calendar year of week value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn year_of_week(&self) -> TemporalResult<Option<i32>> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.year_of_week_with_provider(&*provider)
    }

    /// Returns the calendar days in week value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn days_in_week(&self) -> TemporalResult<u16> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.days_in_week_with_provider(&*provider)
    }

    /// Returns the calendar days in month value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn days_in_month(&self) -> TemporalResult<u16> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.days_in_month_with_provider(&*provider)
    }

    /// Returns the calendar days in year value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn days_in_year(&self) -> TemporalResult<u16> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.days_in_year_with_provider(&*provider)
    }

    /// Returns the calendar months in year value.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn months_in_year(&self) -> TemporalResult<u16> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.months_in_year_with_provider(&*provider)
    }

    /// Returns returns whether the date in a leap year for the given calendar.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn in_leap_year(&self) -> TemporalResult<bool> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.in_leap_year_with_provider(&*provider)
    }

    // TODO: Update direction to correct option
    pub fn get_time_zone_transition(
        &self,
        direction: TransitionDirection,
    ) -> TemporalResult<Option<Self>> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.get_time_zone_transition_with_provider(direction, &*provider)
    }

    /// Returns the hours in the day.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn hours_in_day(&self) -> TemporalResult<u8> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.hours_in_day_with_provider(&*provider)
    }
}

// ==== Experimental TZ_PROVIDER method implementations ====

/// The primary `ZonedDateTime` method implementations.
///
/// The following [`ZonedDateTime`] methods are feature gated behind the
/// `compiled_data` feature flag.
impl ZonedDateTime {
    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime` with the provided `PlainTime`.
    ///
    /// combined with the provided `TimeZone`.
    pub fn with_plain_time(&self, time: PlainTime) -> TemporalResult<Self> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.with_plain_time_and_provider(time, &*provider)
    }

    /// Adds a [`Duration`] to the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn add(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.add_with_provider(duration, overflow, &*provider)
    }

    /// Subtracts a [`Duration`] to the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn subtract(
        &self,
        duration: &Duration,
        overflow: Option<ArithmeticOverflow>,
    ) -> TemporalResult<Self> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.subtract_with_provider(duration, overflow, &*provider)
    }

    /// Returns a [`Duration`] representing the period of time from this `ZonedDateTime` since the other `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn since(&self, other: &Self, options: DifferenceSettings) -> TemporalResult<Duration> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.since_with_provider(other, options, &*provider)
    }

    /// Returns a [`Duration`] representing the period of time from this `ZonedDateTime` since the other `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn until(&self, other: &Self, options: DifferenceSettings) -> TemporalResult<Duration> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.until_with_provider(other, options, &*provider)
    }

    /// Returns the start of day for the current `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn start_of_day(&self) -> TemporalResult<Self> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.start_of_day_with_provider(&*provider)
    }

    /// Creates a new [`PlainDate`] from this `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn to_plain_date(&self) -> TemporalResult<PlainDate> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.to_plain_date_with_provider(&*provider)
    }

    /// Creates a new [`PlainTime`] from this `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn to_plain_time(&self) -> TemporalResult<PlainTime> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.to_plain_time_with_provider(&*provider)
    }

    /// Creates a new [`PlainDateTime`] from this `ZonedDateTime`.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn to_plain_datetime(&self) -> TemporalResult<PlainDateTime> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.to_plain_datetime_with_provider(&*provider)
    }

    /// Rounds this [`ZonedDateTime`] to the nearest value according to the given rounding options.
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn round(&self, options: RoundingOptions) -> TemporalResult<Self> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.round_with_provider(options, &*provider)
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
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        self.to_ixdtf_string_with_provider(
            display_offset,
            display_timezone,
            display_calendar,
            options,
            &*provider,
        )
    }

    /// Attempts to parse and create a `ZonedDateTime` from an IXDTF formatted [`&str`].
    ///
    /// Enable with the `compiled_data` feature flag.
    pub fn from_str(
        source: &str,
        disambiguation: Disambiguation,
        offset_option: OffsetDisambiguation,
    ) -> TemporalResult<Self> {
        let provider = TZ_PROVIDER
            .lock()
            .map_err(|_| TemporalError::general("Unable to acquire lock"))?;
        ZonedDateTime::from_str_with_provider(source, disambiguation, offset_option, &*provider)
    }
}

mod tests {
    #[cfg(not(target_os = "windows"))]
    #[test]
    fn static_tzdb_zdt_test() {
        use super::ZonedDateTime;
        use crate::{Calendar, TimeZone};
        use core::str::FromStr;

        let nov_30_2023_utc = 1_701_308_952_000_000_000i128;

        let zdt = ZonedDateTime::try_new(
            nov_30_2023_utc,
            Calendar::from_str("iso8601").unwrap(),
            TimeZone::try_from_str("Z").unwrap(),
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
        use super::ZonedDateTime;
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
        assert_eq!(result, expected);
    }
}
