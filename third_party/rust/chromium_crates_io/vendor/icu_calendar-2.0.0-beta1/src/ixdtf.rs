// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::str::FromStr;

use crate::{AnyCalendar, Date, DateTime, Iso, RangeError, Time};
use ixdtf::parsers::records::IxdtfParseRecord;
use ixdtf::parsers::IxdtfParser;
use ixdtf::ParseError as IxdtfError;

/// An error returned from parsing an IXDTF string to an `icu_calendar` type.
#[derive(Debug)]
#[non_exhaustive]
pub enum ParseError {
    /// Syntax error in the IXDTF string.
    Syntax(IxdtfError),
    /// Value is out of range.
    Range(RangeError),
    /// The IXDTF is missing fields required for parsing into the chosen type.
    MissingFields,
    /// The IXDTF specifies an unknown calendar.
    UnknownCalendar,
}

impl From<RangeError> for ParseError {
    fn from(value: RangeError) -> Self {
        Self::Range(value)
    }
}

impl From<IxdtfError> for ParseError {
    fn from(value: IxdtfError) -> Self {
        Self::Syntax(value)
    }
}

impl AnyCalendar {
    #[cfg(feature = "compiled_data")]
    fn try_from_ixdtf_record(ixdtf_record: &IxdtfParseRecord) -> Result<Self, ParseError> {
        let calendar_id = ixdtf_record.calendar.unwrap_or(b"iso");
        let calendar_kind = crate::AnyCalendarKind::get_for_bcp47_bytes(calendar_id)
            .ok_or(ParseError::UnknownCalendar)?;
        let calendar = AnyCalendar::new_for_kind(calendar_kind);
        Ok(calendar)
    }
}

impl Date<Iso> {
    /// Creates a [`Date`] in the ISO-8601 calendar from an IXDTF syntax string.
    ///
    /// Ignores any calendar annotations in the string.
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::Date;
    ///
    /// let date = Date::try_iso_from_str("2024-07-17").unwrap();
    ///
    /// assert_eq!(date.year().era_year_or_extended(), 2024);
    /// assert_eq!(
    ///     date.month().standard_code,
    ///     icu::calendar::types::MonthCode(tinystr::tinystr!(4, "M07"))
    /// );
    /// assert_eq!(date.day_of_month().0, 17);
    /// ```
    pub fn try_iso_from_str(ixdtf_str: &str) -> Result<Self, ParseError> {
        Self::try_iso_from_utf8(ixdtf_str.as_bytes())
    }

    /// Creates a [`Date`] in the ISO-8601 calendar from an IXDTF syntax string.
    ///
    /// See [`Self::try_iso_from_str()`].
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    pub fn try_iso_from_utf8(ixdtf_str: &[u8]) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse()?;
        Self::try_from_ixdtf_record(&ixdtf_record)
    }

    fn try_from_ixdtf_record(ixdtf_record: &IxdtfParseRecord) -> Result<Self, ParseError> {
        let date_record = ixdtf_record.date.ok_or(ParseError::MissingFields)?;
        let date = Self::try_new_iso(date_record.year, date_record.month, date_record.day)?;
        Ok(date)
    }
}

impl FromStr for Date<Iso> {
    type Err = ParseError;
    fn from_str(ixdtf_str: &str) -> Result<Self, Self::Err> {
        Self::try_iso_from_str(ixdtf_str)
    }
}

impl Date<AnyCalendar> {
    /// Creates a [`Date`] in any calendar from an IXDTF syntax string with compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` and `ixdtf` Cargo features.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::Date;
    ///
    /// let date = Date::try_from_str("2024-07-17[u-ca=hebrew]").unwrap();
    ///
    /// assert_eq!(date.year().era_year_or_extended(), 5784);
    /// assert_eq!(
    ///     date.month().standard_code,
    ///     icu::calendar::types::MonthCode(tinystr::tinystr!(4, "M10"))
    /// );
    /// assert_eq!(date.day_of_month().0, 11);
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_from_str(ixdtf_str: &str) -> Result<Self, ParseError> {
        Self::try_from_utf8(ixdtf_str.as_bytes())
    }

    /// Creates a [`Date`] in any calendar from an IXDTF syntax string with compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` and `ixdtf` Cargo features.*
    ///
    /// See [`Self::try_from_str()`].
    #[cfg(feature = "compiled_data")]
    pub fn try_from_utf8(ixdtf_str: &[u8]) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let iso_date = Date::<Iso>::try_from_ixdtf_record(&ixdtf_record)?;
        let calendar = AnyCalendar::try_from_ixdtf_record(&ixdtf_record)?;
        let date = iso_date.to_any().to_calendar(calendar);
        Ok(date)
    }
}

#[cfg(feature = "compiled_data")]
impl FromStr for Date<AnyCalendar> {
    type Err = ParseError;
    fn from_str(ixdtf_str: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(ixdtf_str)
    }
}

impl Time {
    /// Creates a [`Time`] from an IXDTF syntax string of a time.
    ///
    /// Does not support parsing an IXDTF string with a date and time; for that, use [`DateTime`].
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::Time;
    ///
    /// let time = Time::try_from_str("16:01:17.045").unwrap();
    ///
    /// assert_eq!(time.hour.number(), 16);
    /// assert_eq!(time.minute.number(), 1);
    /// assert_eq!(time.second.number(), 17);
    /// assert_eq!(time.nanosecond.number(), 45000000);
    /// ```
    pub fn try_from_str(ixdtf_str: &str) -> Result<Self, ParseError> {
        Self::try_from_utf8(ixdtf_str.as_bytes())
    }

    /// Creates a [`Time`] in the ISO-8601 calendar from an IXDTF syntax string.
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    ///
    /// See [`Self::try_from_str()`].
    pub fn try_from_utf8(ixdtf_str: &[u8]) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse_time()?;
        Self::try_from_ixdtf_record(&ixdtf_record)
    }

    fn try_from_ixdtf_record(ixdtf_record: &IxdtfParseRecord) -> Result<Self, ParseError> {
        let time_record = ixdtf_record.time.ok_or(ParseError::MissingFields)?;
        let time = Self::try_new(
            time_record.hour,
            time_record.minute,
            time_record.second,
            time_record.nanosecond,
        )?;
        Ok(time)
    }
}

impl FromStr for Time {
    type Err = ParseError;
    fn from_str(ixdtf_str: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(ixdtf_str)
    }
}

impl DateTime<Iso> {
    /// Creates a [`DateTime`] in the ISO-8601 calendar from an IXDTF syntax string.
    ///
    /// Ignores any calendar annotations in the string.
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::DateTime;
    ///
    /// let datetime =
    ///     DateTime::try_iso_from_str("2024-07-17T16:01:17.045").unwrap();
    ///
    /// assert_eq!(datetime.date.year().era_year_or_extended(), 2024);
    /// assert_eq!(
    ///     datetime.date.month().standard_code,
    ///     icu::calendar::types::MonthCode(tinystr::tinystr!(4, "M07"))
    /// );
    /// assert_eq!(datetime.date.day_of_month().0, 17);
    ///
    /// assert_eq!(datetime.time.hour.number(), 16);
    /// assert_eq!(datetime.time.minute.number(), 1);
    /// assert_eq!(datetime.time.second.number(), 17);
    /// assert_eq!(datetime.time.nanosecond.number(), 45000000);
    /// ```
    pub fn try_iso_from_str(ixdtf_str: &str) -> Result<Self, ParseError> {
        Self::try_iso_from_utf8(ixdtf_str.as_bytes())
    }

    /// Creates a [`DateTime`] in the ISO-8601 calendar from an IXDTF syntax string.
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    ///
    /// See [`Self::try_iso_from_str()`].
    pub fn try_iso_from_utf8(ixdtf_str: &[u8]) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse()?;
        Self::try_from_ixdtf_record(&ixdtf_record)
    }

    fn try_from_ixdtf_record(ixdtf_record: &IxdtfParseRecord) -> Result<Self, ParseError> {
        let date = Date::<Iso>::try_from_ixdtf_record(ixdtf_record)?;
        let time = Time::try_from_ixdtf_record(ixdtf_record)?;
        Ok(Self::new(date, time))
    }
}

impl FromStr for DateTime<Iso> {
    type Err = ParseError;
    fn from_str(ixdtf_str: &str) -> Result<Self, Self::Err> {
        Self::try_iso_from_str(ixdtf_str)
    }
}

impl DateTime<AnyCalendar> {
    /// Creates a [`DateTime`] in any calendar from an IXDTF syntax string with compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` and `ixdtf` Cargo features.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::DateTime;
    ///
    /// let datetime =
    ///     DateTime::try_from_str("2024-07-17T16:01:17.045[u-ca=hebrew]").unwrap();
    ///
    /// assert_eq!(datetime.date.year().era_year_or_extended(), 5784);
    /// assert_eq!(
    ///     datetime.date.month().standard_code,
    ///     icu::calendar::types::MonthCode(tinystr::tinystr!(4, "M10"))
    /// );
    /// assert_eq!(datetime.date.day_of_month().0, 11);
    ///
    /// assert_eq!(datetime.time.hour.number(), 16);
    /// assert_eq!(datetime.time.minute.number(), 1);
    /// assert_eq!(datetime.time.second.number(), 17);
    /// assert_eq!(datetime.time.nanosecond.number(), 45000000);
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_from_str(ixdtf_str: &str) -> Result<Self, ParseError> {
        Self::try_from_utf8(ixdtf_str.as_bytes())
    }

    /// Creates a [`DateTime`] in any calendar from an IXDTF syntax string with compiled data.
    ///
    /// See [`Self::try_from_str()`].
    ///
    /// ✨ *Enabled with the `compiled_data` and `ixdtf` Cargo features.*
    #[cfg(feature = "compiled_data")]
    pub fn try_from_utf8(ixdtf_str: &[u8]) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let iso_datetime = DateTime::<Iso>::try_from_ixdtf_record(&ixdtf_record)?;
        let calendar = AnyCalendar::try_from_ixdtf_record(&ixdtf_record)?;
        let datetime = iso_datetime.to_any().to_calendar(calendar);
        Ok(datetime)
    }
}

#[cfg(feature = "compiled_data")]
impl FromStr for DateTime<AnyCalendar> {
    type Err = ParseError;
    fn from_str(ixdtf_str: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(ixdtf_str)
    }
}
