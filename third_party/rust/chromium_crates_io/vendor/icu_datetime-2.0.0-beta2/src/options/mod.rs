// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Options types for date/time formatting.

use icu_time::scaffold::IntoOption;

/// The length of a formatted date/time string.
///
/// Length settings are always a hint, not a guarantee. For example, certain locales and
/// calendar systems do not define numeric names, so spelled-out names could occur even if a
/// short length was requested, and likewise with numeric names with a medium or long length.
///
/// # Examples
///
/// ```
/// use icu::calendar::Gregorian;
/// use icu::datetime::fieldsets::YMD;
/// use icu::datetime::input::Date;
/// use icu::datetime::FixedCalendarDateTimeFormatter;
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let short_formatter = FixedCalendarDateTimeFormatter::try_new(
///     locale!("en-US").into(),
///     YMD::short(),
/// )
/// .unwrap();
///
/// let medium_formatter = FixedCalendarDateTimeFormatter::try_new(
///     locale!("en-US").into(),
///     YMD::medium(),
/// )
/// .unwrap();
///
/// let long_formatter = FixedCalendarDateTimeFormatter::try_new(
///     locale!("en-US").into(),
///     YMD::long(),
/// )
/// .unwrap();
///
/// assert_writeable_eq!(
///     short_formatter.format(&Date::try_new_gregorian(2000, 1, 1).unwrap()),
///     "1/1/00"
/// );
///
/// assert_writeable_eq!(
///     medium_formatter.format(&Date::try_new_gregorian(2000, 1, 1).unwrap()),
///     "Jan 1, 2000"
/// );
///
/// assert_writeable_eq!(
///     long_formatter.format(&Date::try_new_gregorian(2000, 1, 1).unwrap()),
///     "January 1, 2000"
/// );
/// ```
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, Default)]
#[cfg_attr(
    all(feature = "serde", feature = "experimental"),
    derive(serde::Serialize, serde::Deserialize)
)]
#[cfg_attr(
    all(feature = "serde", feature = "experimental"),
    serde(rename_all = "lowercase")
)]
#[repr(u8)] // discriminants come from symbol count in UTS 35
#[non_exhaustive]
pub enum Length {
    /// A long date; typically spelled-out, as in “January 1, 2000”.
    Long = 4,
    /// A medium-sized date; typically abbreviated, as in “Jan. 1, 2000”.
    ///
    /// This is the default.
    #[default]
    Medium = 3,
    /// A short date; typically numeric, as in “1/1/2000”.
    Short = 1,
}

impl IntoOption<Length> for Length {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

/// The alignment context of the formatted string.
///
/// By default, datetimes are formatted for a variable-width context. You can
/// give a hint that the strings will be displayed in a column-like context,
/// which will coerce numerics to be padded with zeros.
///
/// # Examples
///
/// ```
/// use icu::calendar::Gregorian;
/// use icu::datetime::fieldsets::YMD;
/// use icu::datetime::input::Date;
/// use icu::datetime::options::Alignment;
/// use icu::datetime::FixedCalendarDateTimeFormatter;
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let plain_formatter =
///     FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
///         locale!("en-US").into(),
///         YMD::short(),
///     )
///     .unwrap();
///
/// let column_formatter =
///     FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
///         locale!("en-US").into(),
///         YMD::short().with_alignment(Alignment::Column),
///     )
///     .unwrap();
///
/// // By default, en-US does not pad the month and day with zeros.
/// assert_writeable_eq!(
///     plain_formatter.format(&Date::try_new_gregorian(2025, 1, 1).unwrap()),
///     "1/1/25"
/// );
///
/// // The column alignment option hints that they should be padded.
/// assert_writeable_eq!(
///     column_formatter.format(&Date::try_new_gregorian(2025, 1, 1).unwrap()),
///     "01/01/25"
/// );
/// ```
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, Default)]
#[cfg_attr(
    all(feature = "serde", feature = "experimental"),
    derive(serde::Serialize, serde::Deserialize)
)]
#[cfg_attr(
    all(feature = "serde", feature = "experimental"),
    serde(rename_all = "lowercase")
)]
#[non_exhaustive]
pub enum Alignment {
    /// Align fields as the locale specifies them to be aligned.
    ///
    /// This is the default option.
    #[default]
    Auto,
    /// Align fields as appropriate for a column layout. For example:
    ///
    /// | US Holiday   | Date       |
    /// |--------------|------------|
    /// | Memorial Day | 05/26/2025 |
    /// | Labor Day    | 09/01/2025 |
    /// | Veterans Day | 11/11/2025 |
    ///
    /// This option causes numeric fields to be padded when necessary. It does
    /// not impact whether a numeric or spelled-out field is chosen.
    Column,
}

impl IntoOption<Alignment> for Alignment {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

/// A specification of how to render the year and the era.
///
/// The choices may grow over time; to follow along and offer feedback, see
/// <https://github.com/unicode-org/icu4x/issues/6010>.
///
/// # Examples
///
/// ```
/// use icu::calendar::Gregorian;
/// use icu::datetime::fieldsets::YMD;
/// use icu::datetime::input::Date;
/// use icu::datetime::options::YearStyle;
/// use icu::datetime::FixedCalendarDateTimeFormatter;
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let formatter = FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
///     locale!("en-US").into(),
///     YMD::short().with_year_style(YearStyle::Auto),
/// )
/// .unwrap();
///
/// // Era displayed when needed for disambiguation,
/// // such as years before year 0 and small year numbers:
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(-1000, 1, 1).unwrap()),
///     "1/1/1001 BC"
/// );
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(77, 1, 1).unwrap()),
///     "1/1/77 AD"
/// );
/// // Era elided for modern years:
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(1900, 1, 1).unwrap()),
///     "1/1/1900"
/// );
/// // Era and century both elided for nearby years:
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(2025, 1, 1).unwrap()),
///     "1/1/25"
/// );
///
/// let formatter = FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
///     locale!("en-US").into(),
///     YMD::short().with_year_style(YearStyle::Full),
/// )
/// .unwrap();
///
/// // Era still displayed in cases with ambiguity:
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(-1000, 1, 1).unwrap()),
///     "1/1/1001 BC"
/// );
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(77, 1, 1).unwrap()),
///     "1/1/77 AD"
/// );
/// // Era elided for modern years:
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(1900, 1, 1).unwrap()),
///     "1/1/1900"
/// );
/// // But now we always get a full-precision year:
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(2025, 1, 1).unwrap()),
///     "1/1/2025"
/// );
///
/// let formatter = FixedCalendarDateTimeFormatter::<Gregorian, _>::try_new(
///     locale!("en-US").into(),
///     YMD::short().with_year_style(YearStyle::WithEra),
/// )
/// .unwrap();
///
/// // Era still displayed in cases with ambiguity:
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(-1000, 1, 1).unwrap()),
///     "1/1/1001 BC"
/// );
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(77, 1, 1).unwrap()),
///     "1/1/77 AD"
/// );
/// // But now it is shown even on modern years:
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(1900, 1, 1).unwrap()),
///     "1/1/1900 AD"
/// );
/// assert_writeable_eq!(
///     formatter.format(&Date::try_new_gregorian(2025, 1, 1).unwrap()),
///     "1/1/2025 AD"
/// );
/// ```
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, Default)]
#[cfg_attr(
    all(feature = "serde", feature = "experimental"),
    derive(serde::Serialize, serde::Deserialize)
)]
#[cfg_attr(
    all(feature = "serde", feature = "experimental"),
    serde(rename_all = "camelCase")
)]
#[non_exhaustive]
pub enum YearStyle {
    /// Display the century and/or era when needed to disambiguate the year,
    /// based on locale preferences.
    ///
    /// This is the default option.
    ///
    /// Examples:
    ///
    /// - `1000 BC`
    /// - `77 AD`
    /// - `1900`
    /// - `'24`
    #[default]
    Auto,
    /// Always display the century, and display the era when needed to
    /// disambiguate the year, based on locale preferences.
    ///
    /// Examples:
    ///
    /// - `1000 BC`
    /// - `77 AD`
    /// - `1900`
    /// - `2024`
    Full,
    /// Always display the century and era.
    ///
    /// Examples:
    ///
    /// - `1000 BC`
    /// - `77 AD`
    /// - `1900 AD`
    /// - `2024 AD`
    WithEra,
}

impl IntoOption<YearStyle> for YearStyle {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

/// A specification for how precisely to display the time of day.
///
/// The time can be displayed with hour, minute, or second precision, and
/// zero-valued fields can be automatically hidden.
///
/// The examples in the discriminants are based on the following inputs and hour cycles:
///
/// 1. 11 o'clock with 12-hour time
/// 2. 16:20 (4:20 pm) with 24-hour time
/// 3. 7:15:01.85 with 24-hour time
///
/// Fractional second digits can be displayed with a fixed precision. If you would like
/// additional options for fractional second digit display, please leave a comment in
/// <https://github.com/unicode-org/icu4x/issues/6008>.
///
/// # Examples
///
/// ```
/// use icu::datetime::input::Time;
/// use icu::datetime::fieldsets::T;
/// use icu::datetime::options::SubsecondDigits;
/// use icu::datetime::options::TimePrecision;
/// use icu::datetime::FixedCalendarDateTimeFormatter;
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let formatters = [
///     TimePrecision::Hour,
///     TimePrecision::Minute,
///     TimePrecision::Second,
///     TimePrecision::Subsecond(SubsecondDigits::S2),
///     TimePrecision::MinuteOptional,
/// ]
/// .map(|time_precision| {
///     FixedCalendarDateTimeFormatter::<(), _>::try_new(
///         locale!("en-US").into(),
///         T::short().with_time_precision(time_precision),
///     )
///     .unwrap()
/// });
///
/// let times = [
///     Time::try_new(7, 0, 0, 0).unwrap(),
///     Time::try_new(7, 0, 10, 0).unwrap(),
///     Time::try_new(7, 12, 20, 543_200_000).unwrap(),
/// ];
///
/// let expected_value_table = [
///     // 7:00:00, 7:00:10, 7:12:20.5432
///     ["7 AM", "7 AM", "7 AM"],                            // Hour
///     ["7:00 AM", "7:00 AM", "7:12 AM"],                   // Minute
///     ["7:00:00 AM", "7:00:10 AM", "7:12:20 AM"],          // Second
///     ["7:00:00.00 AM", "7:00:10.00 AM", "7:12:20.54 AM"], // Subsecond(F2)
///     ["7 AM", "7 AM", "7:12 AM"],                         // MinuteOptional
/// ];
///
/// for (expected_value_row, formatter) in
///     expected_value_table.iter().zip(formatters.iter())
/// {
///     for (expected_value, time) in
///         expected_value_row.iter().zip(times.iter())
///     {
///         assert_writeable_eq!(
///             formatter.format(time),
///             *expected_value,
///             "{formatter:?} @ {time:?}"
///         );
///     }
/// }
/// ```
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, Default)]
#[cfg_attr(
    all(feature = "serde", feature = "experimental"),
    derive(serde::Serialize, serde::Deserialize)
)]
#[cfg_attr(
    all(feature = "serde", feature = "experimental"),
    serde(from = "TimePrecisionSerde", into = "TimePrecisionSerde")
)]
#[non_exhaustive]
pub enum TimePrecision {
    /// Display the hour. Hide all other time fields.
    ///
    /// Examples:
    ///
    /// 1. `11 am`
    /// 2. `16h`
    /// 3. `07h`
    Hour,
    /// Display the hour and minute. Hide the second.
    ///
    /// Examples:
    ///
    /// 1. `11:00 am`
    /// 2. `16:20`
    /// 3. `07:15`
    Minute,
    /// Display the hour, minute, and second. Hide fractional seconds.
    ///
    /// This is currently the default, but the default is subject to change.
    ///
    /// Examples:
    ///
    /// 1. `11:00:00 am`
    /// 2. `16:20:00`
    /// 3. `07:15:01`
    #[default]
    Second,
    /// Display the hour, minute, and second with the given number of
    /// fractional second digits.
    ///
    /// Examples with [`SubsecondDigits::S1`]:
    ///
    /// 1. `11:00:00.0 am`
    /// 2. `16:20:00.0`
    /// 3. `07:15:01.8`
    Subsecond(SubsecondDigits),
    /// Display the hour; display the minute if nonzero. Hide the second.
    ///
    /// Examples:
    ///
    /// 1. `11 am`
    /// 2. `16:20`
    /// 3. `07:15`
    MinuteOptional,
}

impl IntoOption<TimePrecision> for TimePrecision {
    #[inline]
    fn into_option(self) -> Option<Self> {
        Some(self)
    }
}

#[cfg(all(feature = "serde", feature = "experimental"))]
#[derive(Copy, Clone, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "camelCase")]
enum TimePrecisionSerde {
    Hour,
    Minute,
    Second,
    Subsecond1,
    Subsecond2,
    Subsecond3,
    Subsecond4,
    Subsecond5,
    Subsecond6,
    Subsecond7,
    Subsecond8,
    Subsecond9,
    MinuteOptional,
}

#[cfg(all(feature = "serde", feature = "experimental"))]
impl From<TimePrecision> for TimePrecisionSerde {
    fn from(value: TimePrecision) -> Self {
        match value {
            TimePrecision::Hour => TimePrecisionSerde::Hour,
            TimePrecision::Minute => TimePrecisionSerde::Minute,
            TimePrecision::Second => TimePrecisionSerde::Second,
            TimePrecision::Subsecond(SubsecondDigits::S1) => TimePrecisionSerde::Subsecond1,
            TimePrecision::Subsecond(SubsecondDigits::S2) => TimePrecisionSerde::Subsecond2,
            TimePrecision::Subsecond(SubsecondDigits::S3) => TimePrecisionSerde::Subsecond3,
            TimePrecision::Subsecond(SubsecondDigits::S4) => TimePrecisionSerde::Subsecond4,
            TimePrecision::Subsecond(SubsecondDigits::S5) => TimePrecisionSerde::Subsecond5,
            TimePrecision::Subsecond(SubsecondDigits::S6) => TimePrecisionSerde::Subsecond6,
            TimePrecision::Subsecond(SubsecondDigits::S7) => TimePrecisionSerde::Subsecond7,
            TimePrecision::Subsecond(SubsecondDigits::S8) => TimePrecisionSerde::Subsecond8,
            TimePrecision::Subsecond(SubsecondDigits::S9) => TimePrecisionSerde::Subsecond9,
            TimePrecision::MinuteOptional => TimePrecisionSerde::MinuteOptional,
        }
    }
}

#[cfg(all(feature = "serde", feature = "experimental"))]
impl From<TimePrecisionSerde> for TimePrecision {
    fn from(value: TimePrecisionSerde) -> Self {
        match value {
            TimePrecisionSerde::Hour => TimePrecision::Hour,
            TimePrecisionSerde::Minute => TimePrecision::Minute,
            TimePrecisionSerde::Second => TimePrecision::Second,
            TimePrecisionSerde::Subsecond1 => TimePrecision::Subsecond(SubsecondDigits::S1),
            TimePrecisionSerde::Subsecond2 => TimePrecision::Subsecond(SubsecondDigits::S2),
            TimePrecisionSerde::Subsecond3 => TimePrecision::Subsecond(SubsecondDigits::S3),
            TimePrecisionSerde::Subsecond4 => TimePrecision::Subsecond(SubsecondDigits::S4),
            TimePrecisionSerde::Subsecond5 => TimePrecision::Subsecond(SubsecondDigits::S5),
            TimePrecisionSerde::Subsecond6 => TimePrecision::Subsecond(SubsecondDigits::S6),
            TimePrecisionSerde::Subsecond7 => TimePrecision::Subsecond(SubsecondDigits::S7),
            TimePrecisionSerde::Subsecond8 => TimePrecision::Subsecond(SubsecondDigits::S8),
            TimePrecisionSerde::Subsecond9 => TimePrecision::Subsecond(SubsecondDigits::S9),
            TimePrecisionSerde::MinuteOptional => TimePrecision::MinuteOptional,
        }
    }
}

/// A specification for how many fractional second digits to display.
///
/// For example, to display the time with millisecond precision, use
/// [`SubsecondDigits::S3`].
///
/// Lower-precision digits will be truncated.
///
/// # Examples
///
/// Times can be displayed with a custom number of fractional digits from 0-9:
///
/// ```
/// use icu::calendar::Gregorian;
/// use icu::datetime::fieldsets::T;
/// use icu::datetime::input::Time;
/// use icu::datetime::options::SubsecondDigits;
/// use icu::datetime::options::TimePrecision;
/// use icu::datetime::FixedCalendarDateTimeFormatter;
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let formatter = FixedCalendarDateTimeFormatter::<(), _>::try_new(
///     locale!("en-US").into(),
///     T::short()
///         .with_time_precision(TimePrecision::Subsecond(SubsecondDigits::S2)),
/// )
/// .unwrap();
///
/// assert_writeable_eq!(
///     formatter.format(&Time::try_new(16, 12, 20, 543200000).unwrap()),
///     "4:12:20.54 PM"
/// );
/// ```
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum SubsecondDigits {
    /// One fractional digit (tenths of a second).
    S1 = 1,
    /// Two fractional digits (hundredths of a second).
    S2 = 2,
    /// Three fractional digits (milliseconds).
    S3 = 3,
    /// Four fractional digits.
    S4 = 4,
    /// Five fractional digits.
    S5 = 5,
    /// Six fractional digits (microseconds).
    S6 = 6,
    /// Seven fractional digits.
    S7 = 7,
    /// Eight fractional digits.
    S8 = 8,
    /// Nine fractional digits (nanoseconds)
    S9 = 9,
}

/// An error from constructing [`SubsecondDigits`].
#[derive(Debug, Copy, Clone, PartialEq, Eq, displaydoc::Display)]
#[non_exhaustive]
pub enum SubsecondError {
    /// The provided value is out of range (0-9).
    OutOfRange,
}

impl core::error::Error for SubsecondError {}

impl From<SubsecondDigits> for u8 {
    fn from(value: SubsecondDigits) -> u8 {
        use SubsecondDigits::*;
        match value {
            S1 => 1,
            S2 => 2,
            S3 => 3,
            S4 => 4,
            S5 => 5,
            S6 => 6,
            S7 => 7,
            S8 => 8,
            S9 => 9,
        }
    }
}

impl TryFrom<u8> for SubsecondDigits {
    type Error = SubsecondError;
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        use SubsecondDigits::*;
        match value {
            1 => Ok(S1),
            2 => Ok(S2),
            3 => Ok(S3),
            4 => Ok(S4),
            5 => Ok(S5),
            6 => Ok(S6),
            7 => Ok(S7),
            8 => Ok(S8),
            9 => Ok(S9),
            _ => Err(SubsecondError::OutOfRange),
        }
    }
}
