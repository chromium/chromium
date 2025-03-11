// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::str::FromStr;

use crate::{AnyCalendarKind, AsCalendar, Calendar, Date, Iso, RangeError};
use ixdtf::parsers::records::IxdtfParseRecord;
use ixdtf::parsers::IxdtfParser;
use ixdtf::ParseError as IxdtfError;

/// An error returned from parsing an IXDTF string to an `icu_calendar` type.
#[derive(Debug, displaydoc::Display)]
#[non_exhaustive]
pub enum ParseError {
    /// Syntax error in the IXDTF string.
    #[displaydoc("Syntax error in the IXDTF string: {0}")]
    Syntax(IxdtfError),
    /// Value is out of range.
    #[displaydoc("Value out of range: {0}")]
    Range(RangeError),
    /// The IXDTF is missing fields required for parsing into the chosen type.
    MissingFields,
    /// The IXDTF specifies an unknown calendar.
    UnknownCalendar,
    /// Expected a different calendar.
    #[displaydoc("Expected calendar {0} but found calendar {1}")]
    MismatchedCalendar(AnyCalendarKind, AnyCalendarKind),
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

impl FromStr for Date<Iso> {
    type Err = ParseError;
    fn from_str(ixdtf_str: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(ixdtf_str, Iso)
    }
}

impl<A: AsCalendar> Date<A> {
    /// Creates a [`Date`] in the given calendar from an IXDTF syntax string.
    ///
    /// Returns an error if the string has a calendar annotation that does not
    /// match the calendar argument, unless the argument is [`Iso`].
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::{Date, Gregorian};
    ///
    /// let date = Date::try_from_str("2024-07-17", Gregorian).unwrap();
    /// let date =
    ///     Date::try_from_str("2024-07-17[u-ca=gregory]", Gregorian).unwrap();
    /// let _ =
    ///     Date::try_from_str("2024-07-17[u-ca=julian]", Gregorian).unwrap_err();
    ///
    /// assert_eq!(date.year().era_year_or_extended(), 2024);
    /// assert_eq!(
    ///     date.month().standard_code,
    ///     icu::calendar::types::MonthCode(tinystr::tinystr!(4, "M07"))
    /// );
    /// assert_eq!(date.day_of_month().0, 17);
    /// ```
    pub fn try_from_str(ixdtf_str: &str, calendar: A) -> Result<Self, ParseError> {
        Self::try_from_utf8(ixdtf_str.as_bytes(), calendar)
    }

    /// Creates a [`Date`] in the given calendar from an IXDTF syntax string.
    ///
    /// Returns an error if the string has a calendar annotation that does not
    /// match the calendar argument.
    ///
    /// See [`Self::try_from_str()`].
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    pub fn try_from_utf8(ixdtf_str: &[u8], calendar: A) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse()?;
        Self::try_from_ixdtf_record(&ixdtf_record, calendar)
    }

    #[doc(hidden)]
    pub fn try_from_ixdtf_record(
        ixdtf_record: &IxdtfParseRecord,
        calendar: A,
    ) -> Result<Self, ParseError> {
        let date_record = ixdtf_record.date.ok_or(ParseError::MissingFields)?;
        let iso = Date::try_new_iso(date_record.year, date_record.month, date_record.day)?;

        if let Some(ixdtf_calendar) = ixdtf_record.calendar {
            let parsed_calendar = crate::AnyCalendarKind::get_for_bcp47_bytes(ixdtf_calendar)
                .ok_or(ParseError::UnknownCalendar)?;
            let expected_calendar = calendar
                .as_calendar()
                .any_calendar_kind()
                .ok_or(ParseError::UnknownCalendar)?;
            if parsed_calendar != expected_calendar && expected_calendar != AnyCalendarKind::Iso {
                return Err(ParseError::MismatchedCalendar(
                    expected_calendar,
                    parsed_calendar,
                ));
            }
        }
        Ok(iso.to_calendar(calendar))
    }
}
