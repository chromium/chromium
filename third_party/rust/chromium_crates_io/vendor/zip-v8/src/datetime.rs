//! Code related to `DateTime` in zip files

use crate::result::DateTimeRangeError;
use core::fmt;

/// Representation of a moment in time.
///
/// Zip files use an old format from DOS to store timestamps,
/// with its own set of peculiarities.
/// For example, it has a resolution of 2 seconds!
///
/// A [`DateTime`] can be stored directly in a zipfile with
/// [`FileOptions::last_modified_time`](crate::read::ZipFile::last_modified), or
/// read from one with [`ZipFile::last_modified`](crate::read::ZipFile::last_modified).
///
/// # Warning
///
/// Because there is no timezone associated with the [`DateTime`], they should ideally only
/// be used for user-facing descriptions.
///
/// Modern zip files store more precise timestamps; see [`crate::extra_fields::ExtendedTimestamp`]
/// for details.
#[derive(Clone, Copy, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct DateTime {
    datepart: u16,
    timepart: u16,
}

impl fmt::Debug for DateTime {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if *self == Self::default() {
            return f.write_str("DateTime::default()");
        }
        f.write_fmt(format_args!(
            "DateTime::from_date_and_time({}, {}, {}, {}, {}, {})?",
            self.year(),
            self.month(),
            self.day(),
            self.hour(),
            self.minute(),
            self.second()
        ))
    }
}

#[cfg(feature = "_arbitrary")]
impl arbitrary::Arbitrary<'_> for DateTime {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        // DOS time format stores seconds divided by 2 in a 5-bit field (0..=29),
        // so the maximum representable second value is 58.
        const MAX_DOS_SECONDS: u16 = 58;

        let year: u16 = u.int_in_range(1980..=2107)?;
        let month: u16 = u.int_in_range(1..=12)?;
        let day: u16 = u.int_in_range(1..=31)?;
        let datepart = day | (month << 5) | ((year - 1980) << 9);
        let hour: u16 = u.int_in_range(0..=23)?;
        let minute: u16 = u.int_in_range(0..=59)?;
        let second: u16 = u.int_in_range(0..=MAX_DOS_SECONDS)?;
        let timepart = (second >> 1) | (minute << 5) | (hour << 11);
        Ok(DateTime { datepart, timepart })
    }
}

#[cfg(feature = "chrono")]
impl TryFrom<chrono::NaiveDateTime> for DateTime {
    type Error = DateTimeRangeError;

    fn try_from(value: chrono::NaiveDateTime) -> Result<Self, Self::Error> {
        use chrono::{Datelike, Timelike};

        DateTime::from_date_and_time(
            value.year().try_into()?,
            value.month().try_into()?,
            value.day().try_into()?,
            value.hour().try_into()?,
            value.minute().try_into()?,
            value.second().try_into()?,
        )
    }
}

#[cfg(feature = "chrono")]
impl TryFrom<DateTime> for chrono::NaiveDateTime {
    type Error = DateTimeRangeError;

    fn try_from(value: DateTime) -> Result<Self, Self::Error> {
        let date = chrono::NaiveDate::from_ymd_opt(
            value.year().into(),
            value.month().into(),
            value.day().into(),
        )
        .ok_or(DateTimeRangeError)?;
        let time = chrono::NaiveTime::from_hms_opt(
            value.hour().into(),
            value.minute().into(),
            value.second().into(),
        )
        .ok_or(DateTimeRangeError)?;
        Ok(chrono::NaiveDateTime::new(date, time))
    }
}

#[cfg(feature = "jiff-02")]
impl TryFrom<jiff::civil::DateTime> for DateTime {
    type Error = DateTimeRangeError;

    fn try_from(value: jiff::civil::DateTime) -> Result<Self, Self::Error> {
        Self::from_date_and_time(
            value.year().try_into()?,
            value.month() as u8,
            value.day() as u8,
            value.hour() as u8,
            value.minute() as u8,
            value.second() as u8,
        )
    }
}

#[cfg(feature = "jiff-02")]
impl TryFrom<DateTime> for jiff::civil::DateTime {
    type Error = jiff::Error;

    fn try_from(value: DateTime) -> Result<Self, Self::Error> {
        Self::new(
            value.year() as i16,
            value.month() as i8,
            value.day() as i8,
            value.hour() as i8,
            value.minute() as i8,
            value.second() as i8,
            0,
        )
    }
}

impl TryFrom<(u16, u16)> for DateTime {
    type Error = DateTimeRangeError;

    #[inline]
    fn try_from(values: (u16, u16)) -> Result<Self, Self::Error> {
        Self::try_from_msdos(values.0, values.1)
    }
}

impl From<DateTime> for (u16, u16) {
    #[inline]
    fn from(dt: DateTime) -> Self {
        (dt.datepart(), dt.timepart())
    }
}

impl Default for DateTime {
    /// Constructs an 'default' datetime of 1980-01-01 00:00:00
    fn default() -> DateTime {
        DateTime::DEFAULT
    }
}

impl fmt::Display for DateTime {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
            self.year(),
            self.month(),
            self.day(),
            self.hour(),
            self.minute(),
            self.second()
        )
    }
}

impl DateTime {
    /// Constructs a default datetime of 1980-01-01 00:00:00.
    pub const DEFAULT: Self = DateTime {
        datepart: 0b0000_0000_0010_0001,
        timepart: 0,
    };

    /// Returns the current time if possible, otherwise the default of 1980-01-01.
    #[cfg(feature = "time")]
    #[must_use]
    pub fn default_for_write() -> Self {
        let now = time::OffsetDateTime::now_utc();
        time::PrimitiveDateTime::new(now.date(), now.time())
            .try_into()
            .unwrap_or_else(|_| DateTime::default())
    }

    /// Returns the current time if possible, otherwise the default of 1980-01-01.
    #[cfg(not(feature = "time"))]
    #[must_use]
    pub fn default_for_write() -> Self {
        DateTime::default()
    }

    #[cfg(feature = "chrono")]
    /// Generate a `SystemTime` from a `DateTime`.
    pub(crate) fn datetime_to_systemtime(&self) -> Option<std::time::SystemTime> {
        if let Some(chrono_datetime) = self.generate_chrono_datetime() {
            let time = chrono::DateTime::<chrono::Utc>::from_naive_utc_and_offset(
                chrono_datetime,
                chrono::Utc,
            );
            return Some(time.into());
        }
        None
    }

    #[cfg(feature = "chrono")]
    /// Generate a `NaiveDateTime` from a `DateTime`.
    fn generate_chrono_datetime(&self) -> Option<chrono::NaiveDateTime> {
        if let Some(chrono_date) = chrono::NaiveDate::from_ymd_opt(
            self.year().into(),
            self.month().into(),
            self.day().into(),
        ) && let Some(chrono_datetime) = chrono_date.and_hms_opt(
            self.hour().into(),
            self.minute().into(),
            self.second().into(),
        ) {
            return Some(chrono_datetime);
        }
        None
    }

    /// Converts an msdos (u16, u16) pair to a `DateTime` object
    ///
    /// # Safety
    /// The caller must ensure the date and time are valid.
    #[must_use]
    pub const unsafe fn from_msdos_unchecked(datepart: u16, timepart: u16) -> DateTime {
        DateTime { datepart, timepart }
    }

    pub(crate) fn is_leap_year(year: u16) -> bool {
        year.is_multiple_of(4) && (!year.is_multiple_of(100) || year.is_multiple_of(400))
    }

    /// Converts an msdos (u16, u16) pair to a `DateTime` object if it represents a valid date and
    /// time.
    pub fn try_from_msdos(datepart: u16, timepart: u16) -> Result<DateTime, DateTimeRangeError> {
        let seconds = (timepart & 0b0000_0000_0001_1111) << 1;
        let minutes = (timepart & 0b0000_0111_1110_0000) >> 5;
        let hours = (timepart & 0b1111_1000_0000_0000) >> 11;
        let days = datepart & 0b0000_0000_0001_1111;
        let months = (datepart & 0b0000_0001_1110_0000) >> 5;
        let years = (datepart & 0b1111_1110_0000_0000) >> 9;
        Self::from_date_and_time(
            years.checked_add(1980).ok_or(DateTimeRangeError)?,
            months.try_into()?,
            days.try_into()?,
            hours.try_into()?,
            minutes.try_into()?,
            seconds.try_into()?,
        )
    }

    /// Constructs a `DateTime` from a specific date and time
    ///
    /// The bounds are:
    /// * year: [1980, 2107]
    /// * month: [1, 12]
    /// * day: [1, 28..=31]
    /// * hour: [0, 23]
    /// * minute: [0, 59]
    /// * second: [0, 60] (rounded down to even and to [0, 58] due to ZIP format limitation)
    pub fn from_date_and_time(
        year: u16,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
    ) -> Result<DateTime, DateTimeRangeError> {
        if (1980..=2107).contains(&year)
            && (1..=12).contains(&month)
            && (1..=31).contains(&day)
            && hour <= 23
            && minute <= 59
            && second <= 60
        {
            // DOS/ZIP timestamp stores seconds/2 in 5 bits and cannot represent 59 or 60 seconds (incl. leap seconds)
            let second = second.min(58);
            let max_day = match month {
                1 | 3 | 5 | 7 | 8 | 10 | 12 => 31,
                4 | 6 | 9 | 11 => 30,
                2 if Self::is_leap_year(year) => 29,
                2 => 28,
                _ => unreachable!(),
            };
            if day > max_day {
                return Err(DateTimeRangeError);
            }
            let datepart = u16::from(day) | (u16::from(month) << 5) | ((year - 1980) << 9);
            let timepart =
                (u16::from(second) >> 1) | (u16::from(minute) << 5) | (u16::from(hour) << 11);
            Ok(DateTime { datepart, timepart })
        } else {
            Err(DateTimeRangeError)
        }
    }

    /// Indicates whether this date and time can be written to a zip archive.
    #[must_use]
    pub fn is_valid(&self) -> bool {
        Self::try_from_msdos(self.datepart, self.timepart).is_ok()
    }

    /// Gets the time portion of this datetime in the msdos representation
    #[must_use]
    pub const fn timepart(&self) -> u16 {
        self.timepart
    }

    /// Gets the date portion of this datetime in the msdos representation
    #[must_use]
    pub const fn datepart(&self) -> u16 {
        self.datepart
    }

    /// Get the year. There is no epoch, i.e. 2018 will be returned as 2018.
    #[must_use]
    pub const fn year(&self) -> u16 {
        (self.datepart >> 9) + 1980
    }

    /// Get the month, where 1 = january and 12 = december
    ///
    /// # Warning
    ///
    /// When read from a zip file, this may not be a reasonable value
    #[must_use]
    pub const fn month(&self) -> u8 {
        ((self.datepart & 0b0000_0001_1110_0000) >> 5) as u8
    }

    /// Get the day
    ///
    /// # Warning
    ///
    /// When read from a zip file, this may not be a reasonable value
    #[must_use]
    pub const fn day(&self) -> u8 {
        (self.datepart & 0b0000_0000_0001_1111) as u8
    }

    /// Get the hour
    ///
    /// # Warning
    ///
    /// When read from a zip file, this may not be a reasonable value
    #[must_use]
    pub const fn hour(&self) -> u8 {
        (self.timepart >> 11) as u8
    }

    /// Get the minute
    ///
    /// # Warning
    ///
    /// When read from a zip file, this may not be a reasonable value
    #[must_use]
    pub const fn minute(&self) -> u8 {
        ((self.timepart & 0b0000_0111_1110_0000) >> 5) as u8
    }

    /// Get the second
    ///
    /// # Warning
    ///
    /// When read from a zip file, this may not be a reasonable value
    #[must_use]
    pub const fn second(&self) -> u8 {
        ((self.timepart & 0b0000_0000_0001_1111) << 1) as u8
    }
}

#[cfg(all(feature = "time", feature = "deprecated-time"))]
impl TryFrom<time::OffsetDateTime> for DateTime {
    type Error = DateTimeRangeError;

    fn try_from(dt: time::OffsetDateTime) -> Result<Self, Self::Error> {
        Self::try_from(time::PrimitiveDateTime::new(dt.date(), dt.time()))
    }
}

#[cfg(feature = "time")]
impl TryFrom<time::PrimitiveDateTime> for DateTime {
    type Error = DateTimeRangeError;

    fn try_from(dt: time::PrimitiveDateTime) -> Result<Self, Self::Error> {
        Self::from_date_and_time(
            dt.year().try_into()?,
            dt.month().into(),
            dt.day(),
            dt.hour(),
            dt.minute(),
            dt.second(),
        )
    }
}

#[cfg(all(feature = "time", feature = "deprecated-time"))]
impl TryFrom<DateTime> for time::OffsetDateTime {
    type Error = time::error::ComponentRange;

    fn try_from(dt: DateTime) -> Result<Self, Self::Error> {
        time::PrimitiveDateTime::try_from(dt).map(time::PrimitiveDateTime::assume_utc)
    }
}

#[cfg(feature = "time")]
impl TryFrom<DateTime> for time::PrimitiveDateTime {
    type Error = time::error::ComponentRange;

    fn try_from(dt: DateTime) -> Result<Self, Self::Error> {
        use time::{Date, Month, Time};
        let date =
            Date::from_calendar_date(i32::from(dt.year()), Month::try_from(dt.month())?, dt.day())?;
        let time = Time::from_hms(dt.hour(), dt.minute(), dt.second())?;
        Ok(time::PrimitiveDateTime::new(date, time))
    }
}

#[cfg(test)]
mod tests {
    #[test]
    #[allow(clippy::unusual_byte_groupings)]
    fn datetime_default() {
        use super::DateTime;
        let dt = DateTime::default();
        assert_eq!(dt.timepart(), 0);
        assert_eq!(dt.datepart(), 0b0000000_0001_00001);
    }

    #[test]
    #[allow(clippy::unusual_byte_groupings)]
    fn datetime_max() {
        use super::DateTime;
        let dt = DateTime::from_date_and_time(2107, 12, 31, 23, 59, 58).unwrap();
        assert_eq!(dt.timepart(), 0b10111_111011_11101);
        assert_eq!(dt.datepart(), 0b1111111_1100_11111);
    }

    #[test]
    fn datetime_equality() {
        use super::DateTime;

        let dt = DateTime::from_date_and_time(2018, 11, 17, 10, 38, 30).unwrap();
        assert_eq!(
            dt,
            DateTime::from_date_and_time(2018, 11, 17, 10, 38, 30).unwrap()
        );
        assert_ne!(dt, DateTime::default());
    }

    #[test]
    fn datetime_order() {
        use std::cmp::Ordering;

        use super::DateTime;

        let dt = DateTime::from_date_and_time(2018, 11, 17, 10, 38, 30).unwrap();
        assert_eq!(
            dt.cmp(&DateTime::from_date_and_time(2018, 11, 17, 10, 38, 30).unwrap()),
            Ordering::Equal
        );
        // year
        assert!(dt < DateTime::from_date_and_time(2019, 11, 17, 10, 38, 30).unwrap());
        assert!(dt > DateTime::from_date_and_time(2017, 11, 17, 10, 38, 30).unwrap());
        // month
        assert!(dt < DateTime::from_date_and_time(2018, 12, 17, 10, 38, 30).unwrap());
        assert!(dt > DateTime::from_date_and_time(2018, 10, 17, 10, 38, 30).unwrap());
        // day
        assert!(dt < DateTime::from_date_and_time(2018, 11, 18, 10, 38, 30).unwrap());
        assert!(dt > DateTime::from_date_and_time(2018, 11, 16, 10, 38, 30).unwrap());
        // hour
        assert!(dt < DateTime::from_date_and_time(2018, 11, 17, 11, 38, 30).unwrap());
        assert!(dt > DateTime::from_date_and_time(2018, 11, 17, 9, 38, 30).unwrap());
        // minute
        assert!(dt < DateTime::from_date_and_time(2018, 11, 17, 10, 39, 30).unwrap());
        assert!(dt > DateTime::from_date_and_time(2018, 11, 17, 10, 37, 30).unwrap());
        // second
        assert!(dt < DateTime::from_date_and_time(2018, 11, 17, 10, 38, 32).unwrap());
        assert_eq!(
            dt.cmp(&DateTime::from_date_and_time(2018, 11, 17, 10, 38, 31).unwrap()),
            Ordering::Equal
        );
        assert!(dt > DateTime::from_date_and_time(2018, 11, 17, 10, 38, 29).unwrap());
        assert!(dt > DateTime::from_date_and_time(2018, 11, 17, 10, 38, 28).unwrap());
    }

    #[test]
    fn datetime_display() {
        use super::DateTime;

        assert_eq!(format!("{}", DateTime::default()), "1980-01-01 00:00:00");
        assert_eq!(
            format!(
                "{}",
                DateTime::from_date_and_time(2018, 11, 17, 10, 38, 30).unwrap()
            ),
            "2018-11-17 10:38:30"
        );
        assert_eq!(
            format!(
                "{}",
                DateTime::from_date_and_time(2107, 12, 31, 23, 59, 58).unwrap()
            ),
            "2107-12-31 23:59:58"
        );
    }

    #[test]
    fn datetime_bounds() {
        use super::DateTime;

        assert!(DateTime::from_date_and_time(2000, 1, 1, 23, 59, 60).is_ok());
        assert!(DateTime::from_date_and_time(2000, 1, 1, 24, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(2000, 1, 1, 0, 60, 0).is_err());
        assert!(DateTime::from_date_and_time(2000, 1, 1, 0, 0, 61).is_err());

        assert!(DateTime::from_date_and_time(2107, 12, 31, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(1980, 1, 1, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(1979, 1, 1, 0, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(1980, 0, 1, 0, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(1980, 1, 0, 0, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(2108, 12, 31, 0, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(2107, 13, 31, 0, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(2107, 12, 32, 0, 0, 0).is_err());

        assert!(DateTime::from_date_and_time(2018, 1, 31, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 2, 28, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 2, 29, 0, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(2018, 3, 31, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 4, 30, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 4, 31, 0, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(2018, 5, 31, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 6, 30, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 6, 31, 0, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(2018, 7, 31, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 8, 31, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 9, 30, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 9, 31, 0, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(2018, 10, 31, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 11, 30, 0, 0, 0).is_ok());
        assert!(DateTime::from_date_and_time(2018, 11, 31, 0, 0, 0).is_err());
        assert!(DateTime::from_date_and_time(2018, 12, 31, 0, 0, 0).is_ok());

        // leap year: divisible by 4
        assert!(DateTime::from_date_and_time(2024, 2, 29, 0, 0, 0).is_ok());
        // leap year: divisible by 100 and by 400
        assert!(DateTime::from_date_and_time(2000, 2, 29, 0, 0, 0).is_ok());
        // common year: divisible by 100 but not by 400
        assert!(DateTime::from_date_and_time(2100, 2, 29, 0, 0, 0).is_err());
    }

    #[cfg(all(feature = "time", feature = "deprecated-time"))]
    #[test]
    fn datetime_try_from_offset_datetime() {
        use time::macros::datetime;

        use super::DateTime;

        // 2018-11-17 10:38:30
        let dt = DateTime::try_from(datetime!(2018-11-17 10:38:30 UTC)).unwrap();
        assert_eq!(dt.year(), 2018);
        assert_eq!(dt.month(), 11);
        assert_eq!(dt.day(), 17);
        assert_eq!(dt.hour(), 10);
        assert_eq!(dt.minute(), 38);
        assert_eq!(dt.second(), 30);
    }

    #[cfg(feature = "time")]
    #[test]
    fn datetime_try_from_primitive_datetime() {
        use time::macros::datetime;

        use super::DateTime;

        // 2018-11-17 10:38:30
        let dt = DateTime::try_from(datetime!(2018-11-17 10:38:30)).unwrap();
        assert_eq!(dt.year(), 2018);
        assert_eq!(dt.month(), 11);
        assert_eq!(dt.day(), 17);
        assert_eq!(dt.hour(), 10);
        assert_eq!(dt.minute(), 38);
        assert_eq!(dt.second(), 30);
    }

    #[cfg(feature = "time")]
    #[test]
    fn datetime_try_from_bounds() {
        use super::DateTime;
        use time::macros::datetime;

        // 1979-12-31 23:59:59
        assert!(DateTime::try_from(datetime!(1979-12-31 23:59:59)).is_err());

        // 1980-01-01 00:00:00
        assert!(DateTime::try_from(datetime!(1980-01-01 00:00:00)).is_ok());

        // 2107-12-31 23:59:59
        assert!(DateTime::try_from(datetime!(2107-12-31 23:59:59)).is_ok());

        // 2108-01-01 00:00:00
        assert!(DateTime::try_from(datetime!(2108-01-01 00:00:00)).is_err());
    }

    #[cfg(all(feature = "time", feature = "deprecated-time"))]
    #[test]
    fn offset_datetime_try_from_datetime() {
        use time::OffsetDateTime;
        use time::macros::datetime;

        use super::DateTime;

        // 2018-11-17 10:38:30 UTC
        let dt =
            OffsetDateTime::try_from(DateTime::try_from_msdos(0x4D71, 0x54CF).unwrap()).unwrap();
        assert_eq!(dt, datetime!(2018-11-17 10:38:30 UTC));
    }

    #[cfg(feature = "time")]
    #[test]
    fn primitive_datetime_try_from_datetime() {
        use time::PrimitiveDateTime;
        use time::macros::datetime;

        use super::DateTime;

        // 2018-11-17 10:38:30
        let dt =
            PrimitiveDateTime::try_from(DateTime::try_from_msdos(0x4D71, 0x54CF).unwrap()).unwrap();
        assert_eq!(dt, datetime!(2018-11-17 10:38:30));
    }

    #[cfg(all(feature = "time", feature = "deprecated-time"))]
    #[test]
    fn offset_datetime_try_from_bounds() {
        use super::DateTime;
        use time::OffsetDateTime;

        // 1980-00-00 00:00:00
        assert!(
            OffsetDateTime::try_from(unsafe { DateTime::from_msdos_unchecked(0x0000, 0x0000) })
                .is_err()
        );

        // 2107-15-31 31:63:62
        assert!(
            OffsetDateTime::try_from(unsafe { DateTime::from_msdos_unchecked(0xFFFF, 0xFFFF) })
                .is_err()
        );
    }

    #[cfg(feature = "time")]
    #[test]
    fn primitive_datetime_try_from_bounds() {
        use super::DateTime;
        use time::PrimitiveDateTime;

        // 1980-00-00 00:00:00
        assert!(
            PrimitiveDateTime::try_from(unsafe { DateTime::from_msdos_unchecked(0x0000, 0x0000) })
                .is_err()
        );

        // 2107-15-31 31:63:62
        assert!(
            PrimitiveDateTime::try_from(unsafe { DateTime::from_msdos_unchecked(0xFFFF, 0xFFFF) })
                .is_err()
        );
    }

    #[cfg(feature = "jiff-02")]
    #[test]
    fn datetime_try_from_civil_datetime() {
        use jiff::civil;

        use super::DateTime;

        // 2018-11-17 10:38:30
        let dt = DateTime::try_from(civil::datetime(2018, 11, 17, 10, 38, 30, 0)).unwrap();
        assert_eq!(dt.year(), 2018);
        assert_eq!(dt.month(), 11);
        assert_eq!(dt.day(), 17);
        assert_eq!(dt.hour(), 10);
        assert_eq!(dt.minute(), 38);
        assert_eq!(dt.second(), 30);
    }

    #[cfg(feature = "jiff-02")]
    #[test]
    fn datetime_try_from_civil_datetime_bounds() {
        use jiff::civil;

        use super::DateTime;

        // 1979-12-31 23:59:59
        assert!(DateTime::try_from(civil::datetime(1979, 12, 31, 23, 59, 59, 0)).is_err());

        // 1980-01-01 00:00:00
        assert!(DateTime::try_from(civil::datetime(1980, 1, 1, 0, 0, 0, 0)).is_ok());

        // 2107-12-31 23:59:59
        assert!(DateTime::try_from(civil::datetime(2107, 12, 31, 23, 59, 59, 0)).is_ok());

        // 2108-01-01 00:00:00
        assert!(DateTime::try_from(civil::datetime(2108, 1, 1, 0, 0, 0, 0)).is_err());
    }

    #[cfg(feature = "jiff-02")]
    #[test]
    fn civil_datetime_try_from_datetime() {
        use jiff::civil;

        use super::DateTime;

        // 2018-11-17 10:38:30 UTC
        let dt =
            civil::DateTime::try_from(DateTime::try_from_msdos(0x4D71, 0x54CF).unwrap()).unwrap();
        assert_eq!(dt, civil::datetime(2018, 11, 17, 10, 38, 30, 0));
    }

    #[cfg(feature = "jiff-02")]
    #[test]
    fn civil_datetime_try_from_datetime_bounds() {
        use jiff::civil;

        use super::DateTime;

        // 1980-00-00 00:00:00
        assert!(
            civil::DateTime::try_from(unsafe { DateTime::from_msdos_unchecked(0x0000, 0x0000) })
                .is_err()
        );

        // 2107-15-31 31:63:62
        assert!(
            civil::DateTime::try_from(unsafe { DateTime::from_msdos_unchecked(0xFFFF, 0xFFFF) })
                .is_err()
        );
    }

    #[test]
    fn time_conversion() {
        use super::DateTime;
        let dt = DateTime::try_from_msdos(0x4D71, 0x54CF).unwrap();
        assert_eq!(dt.year(), 2018);
        assert_eq!(dt.month(), 11);
        assert_eq!(dt.day(), 17);
        assert_eq!(dt.hour(), 10);
        assert_eq!(dt.minute(), 38);
        assert_eq!(dt.second(), 30);

        let dt = DateTime::try_from((0x4D71, 0x54CF)).unwrap();
        assert_eq!(dt.year(), 2018);
        assert_eq!(dt.month(), 11);
        assert_eq!(dt.day(), 17);
        assert_eq!(dt.hour(), 10);
        assert_eq!(dt.minute(), 38);
        assert_eq!(dt.second(), 30);

        assert_eq!(<(u16, u16)>::from(dt), (0x4D71, 0x54CF));
    }

    #[test]
    fn time_out_of_bounds() {
        use super::DateTime;
        let dt = unsafe { DateTime::from_msdos_unchecked(0xFFFF, 0xFFFF) };
        assert_eq!(dt.year(), 2107);
        assert_eq!(dt.month(), 15);
        assert_eq!(dt.day(), 31);
        assert_eq!(dt.hour(), 31);
        assert_eq!(dt.minute(), 63);
        assert_eq!(dt.second(), 62);

        let dt = unsafe { DateTime::from_msdos_unchecked(0x0000, 0x0000) };
        assert_eq!(dt.year(), 1980);
        assert_eq!(dt.month(), 0);
        assert_eq!(dt.day(), 0);
        assert_eq!(dt.hour(), 0);
        assert_eq!(dt.minute(), 0);
        assert_eq!(dt.second(), 0);
    }

    #[cfg(feature = "time")]
    #[test]
    fn time_at_january() {
        use super::DateTime;
        use time::{OffsetDateTime, PrimitiveDateTime};

        // 2020-01-01 00:00:00
        let clock = OffsetDateTime::from_unix_timestamp(1_577_836_800).unwrap();

        assert!(DateTime::try_from(PrimitiveDateTime::new(clock.date(), clock.time())).is_ok());
    }

    #[test]
    fn test_is_leap_year() {
        use crate::DateTime;
        assert!(DateTime::is_leap_year(2000));
        assert!(!DateTime::is_leap_year(2026));
        assert!(!DateTime::is_leap_year(2027));
        assert!(DateTime::is_leap_year(2028));
        assert!(DateTime::is_leap_year(1600));
        assert!(DateTime::is_leap_year(2400));
        assert!(!DateTime::is_leap_year(1900));
        assert!(!DateTime::is_leap_year(2100));
    }
}
