// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{
    zone::{iana::IanaParserBorrowed, models, InvalidOffsetError, UtcOffset, UtcOffsetCalculator},
    DateTime, Time, TimeZone, TimeZoneInfo, ZonedDateTime,
};
use core::str::FromStr;
use icu_calendar::{AnyCalendarKind, AsCalendar, Date, DateError, Iso, RangeError};
use ixdtf::{
    parsers::{
        records::{
            DateRecord, IxdtfParseRecord, TimeRecord, TimeZoneAnnotation, TimeZoneRecord,
            UtcOffsetRecord, UtcOffsetRecordOrZ,
        },
        IxdtfParser,
    },
    ParseError as IxdtfParseError,
};

/// The error type for parsing IXDTF syntax strings in `icu_time`.
#[derive(Debug, PartialEq, displaydoc::Display)]
#[non_exhaustive]
pub enum ParseError {
    /// Syntax error for IXDTF string.
    #[displaydoc("Syntax error in the IXDTF string: {0}")]
    Syntax(IxdtfParseError),
    /// Parsed record is out of valid date range.
    #[displaydoc("Value out of range: {0}")]
    Range(RangeError),
    /// Parsed date and time records were not a valid ISO date.
    #[displaydoc("Parsed date and time records were not a valid ISO date: {0}")]
    Date(DateError),
    /// There were missing fields required to parse component.
    MissingFields,
    /// There were two offsets provided that were not consistent with each other.
    InconsistentTimeUtcOffsets,
    /// There was an invalid Offset.
    InvalidOffsetError,
    /// Parsed fractional digits had excessive precision beyond nanosecond.
    ExcessivePrecision,
    /// The set of time zone fields was not expected for the given type.
    /// For example, if a named time zone was present with offset-only parsing,
    /// or an offset was present with named-time-zone-only parsing.
    #[displaydoc("The set of time zone fields was not expected for the given type")]
    MismatchedTimeZoneFields,
    /// An unknown calendar was provided.
    UnknownCalendar,
    /// Expected a different calendar.
    #[displaydoc("Expected calendar {0} but found calendar {1}")]
    MismatchedCalendar(AnyCalendarKind, AnyCalendarKind),
    /// A timezone calculation is required to interpret this string, which is not supported.
    ///
    /// # Example
    /// ```
    /// use icu::calendar::Iso;
    /// use icu::time::{ZonedDateTime, ParseError, zone::IanaParser};
    ///
    /// // This timestamp is in UTC, and requires a time zone calculation in order to display a Zurich time.
    /// assert_eq!(
    ///     ZonedDateTime::try_loose_from_str("2024-08-12T12:32:00Z[Europe/Zurich]", Iso, IanaParser::new()).unwrap_err(),
    ///     ParseError::RequiresCalculation,
    /// );
    ///
    /// // These timestamps are in local time
    /// ZonedDateTime::try_loose_from_str("2024-08-12T14:32:00+02:00[Europe/Zurich]", Iso, IanaParser::new()).unwrap();
    /// ZonedDateTime::try_loose_from_str("2024-08-12T14:32:00[Europe/Zurich]", Iso, IanaParser::new()).unwrap();
    /// ```
    #[displaydoc(
        "A timezone calculation is required to interpret this string, which is not supported"
    )]
    RequiresCalculation,
}

impl core::error::Error for ParseError {}

impl From<IxdtfParseError> for ParseError {
    fn from(value: IxdtfParseError) -> Self {
        Self::Syntax(value)
    }
}

impl From<RangeError> for ParseError {
    fn from(value: RangeError) -> Self {
        Self::Range(value)
    }
}

impl From<DateError> for ParseError {
    fn from(value: DateError) -> Self {
        Self::Date(value)
    }
}

impl From<InvalidOffsetError> for ParseError {
    fn from(_: InvalidOffsetError) -> Self {
        Self::InvalidOffsetError
    }
}

impl From<icu_calendar::ParseError> for ParseError {
    fn from(value: icu_calendar::ParseError) -> Self {
        match value {
            icu_calendar::ParseError::MissingFields => Self::MissingFields,
            icu_calendar::ParseError::Range(r) => Self::Range(r),
            icu_calendar::ParseError::Syntax(s) => Self::Syntax(s),
            icu_calendar::ParseError::UnknownCalendar => Self::UnknownCalendar,
            _ => unreachable!(),
        }
    }
}

impl UtcOffset {
    fn try_from_utc_offset_record(record: UtcOffsetRecord) -> Result<Self, ParseError> {
        let hour_seconds = i32::from(record.hour) * 3600;
        let minute_seconds = i32::from(record.minute) * 60;
        Self::try_from_seconds(
            i32::from(record.sign as i8)
                * (hour_seconds + minute_seconds + i32::from(record.second)),
        )
        .map_err(Into::into)
    }
}

struct Intermediate<'a> {
    offset: Option<UtcOffsetRecord>,
    is_z: bool,
    iana_identifier: Option<&'a [u8]>,
    date: DateRecord,
    time: TimeRecord,
}

impl<'a> Intermediate<'a> {
    fn try_from_ixdtf_record(ixdtf_record: &'a IxdtfParseRecord) -> Result<Self, ParseError> {
        let (offset, is_z, iana_identifier) = match ixdtf_record {
            // empty
            IxdtfParseRecord {
                offset: None,
                tz: None,
                ..
            } => (None, false, None),
            // -0800
            IxdtfParseRecord {
                offset: Some(UtcOffsetRecordOrZ::Offset(offset)),
                tz: None,
                ..
            } => (Some(*offset), false, None),
            // Z
            IxdtfParseRecord {
                offset: Some(UtcOffsetRecordOrZ::Z),
                tz: None,
                ..
            } => (None, true, None),
            // [-0800]
            IxdtfParseRecord {
                offset: None,
                tz:
                    Some(TimeZoneAnnotation {
                        tz: TimeZoneRecord::Offset(offset),
                        ..
                    }),
                ..
            } => (Some(*offset), false, None),
            // -0800[-0800]
            IxdtfParseRecord {
                offset: Some(UtcOffsetRecordOrZ::Offset(offset)),
                tz:
                    Some(TimeZoneAnnotation {
                        tz: TimeZoneRecord::Offset(offset1),
                        ..
                    }),
                ..
            } => {
                if offset != offset1 {
                    return Err(ParseError::InconsistentTimeUtcOffsets);
                }
                (Some(*offset), false, None)
            }
            // -0800[America/Los_Angeles]
            IxdtfParseRecord {
                offset: Some(UtcOffsetRecordOrZ::Offset(offset)),
                tz:
                    Some(TimeZoneAnnotation {
                        tz: TimeZoneRecord::Name(iana_identifier),
                        ..
                    }),
                ..
            } => (Some(*offset), false, Some(*iana_identifier)),
            // Z[-0800]
            IxdtfParseRecord {
                offset: Some(UtcOffsetRecordOrZ::Z),
                tz:
                    Some(TimeZoneAnnotation {
                        tz: TimeZoneRecord::Offset(offset),
                        ..
                    }),
                ..
            } => (Some(*offset), true, None),
            // Z[America/Los_Angeles]
            IxdtfParseRecord {
                offset: Some(UtcOffsetRecordOrZ::Z),
                tz:
                    Some(TimeZoneAnnotation {
                        tz: TimeZoneRecord::Name(iana_identifier),
                        ..
                    }),
                ..
            } => (None, true, Some(*iana_identifier)),
            // [America/Los_Angeles]
            IxdtfParseRecord {
                offset: None,
                tz:
                    Some(TimeZoneAnnotation {
                        tz: TimeZoneRecord::Name(iana_identifier),
                        ..
                    }),
                ..
            } => (None, false, Some(*iana_identifier)),
            // non_exhaustive match: maybe something like [u-tz=uslax] in the future
            IxdtfParseRecord {
                tz: Some(TimeZoneAnnotation { tz, .. }),
                ..
            } => {
                debug_assert!(false, "unexpected TimeZoneRecord: {tz:?}");
                (None, false, None)
            }
        };
        let IxdtfParseRecord {
            date: Some(date),
            time: Some(time),
            ..
        } = *ixdtf_record
        else {
            // Date or time was missing
            return Err(ParseError::MismatchedTimeZoneFields);
        };
        Ok(Self {
            offset,
            is_z,
            iana_identifier,
            date,
            time,
        })
    }

    fn offset_only(self) -> Result<UtcOffset, ParseError> {
        let None = self.iana_identifier else {
            return Err(ParseError::MismatchedTimeZoneFields);
        };
        if self.is_z {
            if let Some(offset) = self.offset {
                if offset != UtcOffsetRecord::zero() {
                    return Err(ParseError::RequiresCalculation);
                }
            }
            return Ok(UtcOffset::zero());
        }
        let Some(offset) = self.offset else {
            return Err(ParseError::MismatchedTimeZoneFields);
        };
        UtcOffset::try_from_utc_offset_record(offset)
    }

    fn location_only(
        self,
        iana_parser: IanaParserBorrowed<'_>,
    ) -> Result<TimeZoneInfo<models::AtTime>, ParseError> {
        let None = self.offset else {
            return Err(ParseError::MismatchedTimeZoneFields);
        };
        let Some(iana_identifier) = self.iana_identifier else {
            if self.is_z {
                return Err(ParseError::RequiresCalculation);
            }
            return Err(ParseError::MismatchedTimeZoneFields);
        };
        let time_zone_id = iana_parser.parse_from_utf8(iana_identifier);
        let date = Date::<Iso>::try_new_iso(self.date.year, self.date.month, self.date.day)?;
        let time = Time::try_from_time_record(&self.time)?;
        let offset = match time_zone_id.as_str() {
            "utc" | "gmt" => Some(UtcOffset::zero()),
            _ => None,
        };
        Ok(time_zone_id.with_offset(offset).at_time((date, time)))
    }

    fn loose(
        self,
        iana_parser: IanaParserBorrowed<'_>,
    ) -> Result<TimeZoneInfo<models::AtTime>, ParseError> {
        let time_zone_id = match self.iana_identifier {
            Some(iana_identifier) => {
                if self.is_z {
                    return Err(ParseError::RequiresCalculation);
                }
                iana_parser.parse_from_utf8(iana_identifier)
            }
            None if self.is_z => TimeZone(tinystr::tinystr!(8, "utc")),
            None => TimeZone::unknown(),
        };
        let offset = match self.offset {
            Some(offset) => {
                if self.is_z && offset != UtcOffsetRecord::zero() {
                    return Err(ParseError::RequiresCalculation);
                }
                Some(UtcOffset::try_from_utc_offset_record(offset)?)
            }
            None => match time_zone_id.as_str() {
                "utc" | "gmt" => Some(UtcOffset::zero()),
                _ if self.is_z => Some(UtcOffset::zero()),
                _ => None,
            },
        };
        let date = Date::<Iso>::try_new_iso(self.date.year, self.date.month, self.date.day)?;
        let time = Time::try_from_time_record(&self.time)?;
        Ok(time_zone_id.with_offset(offset).at_time((date, time)))
    }

    fn full(
        self,
        iana_parser: IanaParserBorrowed<'_>,
        offset_calculator: &UtcOffsetCalculator,
    ) -> Result<TimeZoneInfo<models::Full>, ParseError> {
        let Some(offset) = self.offset else {
            return Err(ParseError::MismatchedTimeZoneFields);
        };
        let Some(iana_identifier) = self.iana_identifier else {
            return Err(ParseError::MismatchedTimeZoneFields);
        };
        let time_zone_id = iana_parser.parse_from_utf8(iana_identifier);
        let date = Date::try_new_iso(self.date.year, self.date.month, self.date.day)?;
        let time = Time::try_from_time_record(&self.time)?;
        let offset = UtcOffset::try_from_utc_offset_record(offset)?;
        Ok(time_zone_id
            .with_offset(Some(offset))
            .at_time((date, time))
            .infer_zone_variant(offset_calculator))
    }
}

impl<A: AsCalendar> ZonedDateTime<A, UtcOffset> {
    /// Create a [`ZonedDateTime`] in any calendar from an IXDTF syntax string.
    ///
    /// Returns an error if the string has a calendar annotation that does not
    /// match the calendar argument, unless the argument is [`Iso`].
    ///
    /// This function is "strict": the string should have only an offset and no named time zone.
    pub fn try_offset_only_from_str(ixdtf_str: &str, calendar: A) -> Result<Self, ParseError> {
        Self::try_offset_only_from_utf8(ixdtf_str.as_bytes(), calendar)
    }

    /// Create a [`ZonedDateTime`] in any calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// See [`Self:try_offset_only_from_str`](Self::try_offset_only_from_str).
    pub fn try_offset_only_from_utf8(ixdtf_str: &[u8], calendar: A) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let date = Date::try_from_ixdtf_record(&ixdtf_record, calendar)?;
        let time = Time::try_from_ixdtf_record(&ixdtf_record)?;
        let zone = Intermediate::try_from_ixdtf_record(&ixdtf_record)?.offset_only()?;
        Ok(ZonedDateTime { date, time, zone })
    }
}

impl<A: AsCalendar> ZonedDateTime<A, TimeZoneInfo<models::AtTime>> {
    /// Create a [`ZonedDateTime`] in any calendar from an IXDTF syntax string.
    ///
    /// Returns an error if the string has a calendar annotation that does not
    /// match the calendar argument, unless the argument is [`Iso`].
    ///
    /// This function is "strict": the string should have only a named time zone and no offset.
    pub fn try_location_only_from_str(
        ixdtf_str: &str,
        calendar: A,
        iana_parser: IanaParserBorrowed,
    ) -> Result<Self, ParseError> {
        Self::try_location_only_from_utf8(ixdtf_str.as_bytes(), calendar, iana_parser)
    }

    /// Create a [`ZonedDateTime`] in any calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// See [`Self::try_location_only_from_str`].
    pub fn try_location_only_from_utf8(
        ixdtf_str: &[u8],
        calendar: A,
        iana_parser: IanaParserBorrowed,
    ) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let date = Date::try_from_ixdtf_record(&ixdtf_record, calendar)?;
        let time = Time::try_from_ixdtf_record(&ixdtf_record)?;
        let zone =
            Intermediate::try_from_ixdtf_record(&ixdtf_record)?.location_only(iana_parser)?;
        Ok(ZonedDateTime { date, time, zone })
    }

    /// Create a [`ZonedDateTime`] in any calendar from an IXDTF syntax string.
    ///
    /// Returns an error if the string has a calendar annotation that does not
    /// match the calendar argument, unless the argument is [`Iso`].
    ///
    /// This function is "loose": the string can have an offset, and named time zone, both, or
    /// neither. If the named time zone is missing, it is returned as Etc/Unknown.
    ///
    /// The zone variant is _not_ calculated with this function. If you need it, use
    /// [`Self::try_from_str`].
    pub fn try_loose_from_str(
        ixdtf_str: &str,
        calendar: A,
        iana_parser: IanaParserBorrowed,
    ) -> Result<Self, ParseError> {
        Self::try_loose_from_utf8(ixdtf_str.as_bytes(), calendar, iana_parser)
    }

    /// Create a [`ZonedDateTime`] in any calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// See [`Self::try_loose_from_str`].
    pub fn try_loose_from_utf8(
        ixdtf_str: &[u8],
        calendar: A,
        iana_parser: IanaParserBorrowed,
    ) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let date = Date::try_from_ixdtf_record(&ixdtf_record, calendar)?;
        let time = Time::try_from_ixdtf_record(&ixdtf_record)?;
        let zone = Intermediate::try_from_ixdtf_record(&ixdtf_record)?.loose(iana_parser)?;
        Ok(ZonedDateTime { date, time, zone })
    }
}

impl<A: AsCalendar> ZonedDateTime<A, TimeZoneInfo<models::Full>> {
    /// Create a [`ZonedDateTime`] in any calendar from an IXDTF syntax string.
    ///
    /// Returns an error if the string has a calendar annotation that does not
    /// match the calendar argument, unless the argument is [`Iso`].
    ///
    /// The string should have both an offset and a named time zone.
    ///
    /// For more information on IXDTF, see the [`ixdtf`] crate.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use icu_calendar::cal::Hebrew;
    /// use icu_time::{
    ///     zone::{IanaParser, TimeZoneVariant, UtcOffset, UtcOffsetCalculator},
    ///     TimeZone, TimeZoneInfo, ZonedDateTime,
    /// };
    /// use tinystr::tinystr;
    ///
    /// let zoneddatetime = ZonedDateTime::try_from_str(
    ///     "2024-08-08T12:08:19-05:00[America/Chicago][u-ca=hebrew]",
    ///     Hebrew,
    ///     IanaParser::new(),
    ///     &UtcOffsetCalculator::new(),
    /// )
    /// .unwrap();
    ///
    /// assert_eq!(zoneddatetime.date.year().extended_year, 5784);
    /// assert_eq!(
    ///     zoneddatetime.date.month().standard_code,
    ///     icu::calendar::types::MonthCode(tinystr::tinystr!(4, "M11"))
    /// );
    /// assert_eq!(zoneddatetime.date.day_of_month().0, 4);
    ///
    /// assert_eq!(zoneddatetime.time.hour.number(), 12);
    /// assert_eq!(zoneddatetime.time.minute.number(), 8);
    /// assert_eq!(zoneddatetime.time.second.number(), 19);
    /// assert_eq!(zoneddatetime.time.subsecond.number(), 0);
    /// assert_eq!(
    ///     zoneddatetime.zone.time_zone_id(),
    ///     TimeZone(tinystr!(8, "uschi"))
    /// );
    /// assert_eq!(
    ///     zoneddatetime.zone.offset(),
    ///     Some(UtcOffset::try_from_seconds(-18000).unwrap())
    /// );
    /// assert_eq!(zoneddatetime.zone.zone_variant(), TimeZoneVariant::Daylight);
    /// let (_, _) = zoneddatetime.zone.local_time();
    /// ```
    ///
    /// An IXDTF string can provide a time zone in two parts: the DateTime UTC Offset or the Time Zone
    /// Annotation. A DateTime UTC Offset is the time offset as laid out by RFC3339; meanwhile, the Time
    /// Zone Annotation is the annotation laid out by RFC9557 and is defined as a UTC offset or IANA Time
    /// Zone identifier.
    ///
    /// ## DateTime UTC Offsets
    ///
    /// Below is an example of a time zone from a DateTime UTC Offset. The syntax here is familiar to a RFC3339
    /// DateTime string.
    ///
    /// ```
    /// use icu_calendar::Iso;
    /// use icu_time::{zone::UtcOffset, TimeZoneInfo, ZonedDateTime};
    ///
    /// let tz_from_offset = ZonedDateTime::try_offset_only_from_str(
    ///     "2024-08-08T12:08:19-05:00",
    ///     Iso,
    /// )
    /// .unwrap();
    ///
    /// assert_eq!(
    ///     tz_from_offset.zone,
    ///     UtcOffset::try_from_seconds(-18000).unwrap()
    /// );
    /// ```
    ///
    /// ## Time Zone Annotations
    ///
    /// Below is an example of a time zone being provided by a time zone annotation.
    ///
    /// ```
    /// use icu_calendar::Iso;
    /// use icu_time::{
    ///     zone::{IanaParser, TimeZoneVariant, UtcOffset},
    ///     TimeZone, TimeZoneInfo, ZonedDateTime,
    /// };
    /// use tinystr::tinystr;
    ///
    /// let tz_from_offset_annotation = ZonedDateTime::try_offset_only_from_str(
    ///     "2024-08-08T12:08:19[-05:00]",
    ///     Iso,
    /// )
    /// .unwrap();
    /// let tz_from_iana_annotation = ZonedDateTime::try_location_only_from_str(
    ///     "2024-08-08T12:08:19[America/Chicago]",
    ///     Iso,
    ///     IanaParser::new(),
    /// )
    /// .unwrap();
    ///
    /// assert_eq!(
    ///     tz_from_offset_annotation.zone,
    ///     UtcOffset::try_from_seconds(-18000).unwrap()
    /// );
    ///
    /// assert_eq!(
    ///     tz_from_iana_annotation.zone.time_zone_id(),
    ///     TimeZone(tinystr!(8, "uschi"))
    /// );
    /// assert_eq!(tz_from_iana_annotation.zone.offset(), None);
    /// ```
    ///
    /// ## DateTime UTC Offset and Time Zone Annotations.
    ///
    /// An IXDTF string may contain both a DateTime UTC Offset and Time Zone Annotation. This is fine as long as
    /// the time zone parts can be deemed as inconsistent or unknown consistency.
    ///
    /// ### DateTime UTC Offset with IANA identifier annotation
    ///
    /// In cases where the DateTime UTC Offset is provided and the IANA identifier, some validity checks are performed.
    ///
    /// ```
    /// use icu_calendar::Iso;
    /// use icu_time::{TimeZoneInfo, ZonedDateTime, TimeZone, ParseError, zone::{UtcOffset, TimeZoneVariant, IanaParser, UtcOffsetCalculator}};
    /// use tinystr::tinystr;
    ///
    /// let consistent_tz_from_both = ZonedDateTime::try_from_str("2024-08-08T12:08:19-05:00[America/Chicago]", Iso, IanaParser::new(), &UtcOffsetCalculator::new()).unwrap();
    ///
    ///
    /// assert_eq!(consistent_tz_from_both.zone.time_zone_id(), TimeZone(tinystr!(8, "uschi")));
    /// assert_eq!(consistent_tz_from_both.zone.offset(), Some(UtcOffset::try_from_seconds(-18000).unwrap()));
    /// assert_eq!(consistent_tz_from_both.zone.zone_variant(), TimeZoneVariant::Daylight);
    /// let (_, _) = consistent_tz_from_both.zone.local_time();
    ///
    /// // There is no name for America/Los_Angeles never at -05:00 (at least in 2024), so either the
    /// // time zone or the offset are wrong.
    /// // The only valid way to display this zoned datetime is "GMT-5", so we drop the time zone.
    /// assert_eq!(
    ///     ZonedDateTime::try_from_str("2024-08-08T12:08:19-05:00[America/Los_Angeles]", Iso, IanaParser::new(), &UtcOffsetCalculator::new())
    ///     .unwrap().zone.time_zone_id(),
    ///     TimeZone::unknown()
    /// );
    ///
    /// // We don't know that America/Los_Angeles didn't use standard time (-08:00) in August, but we have a
    /// // name for Los Angeles at -8 (Pacific Standard Time), so this parses successfully.
    /// assert!(
    ///     ZonedDateTime::try_from_str("2024-08-08T12:08:19-08:00[America/Los_Angeles]", Iso, IanaParser::new(), &UtcOffsetCalculator::new()).is_ok()
    /// );
    /// ```
    ///
    /// ### DateTime UTC offset with UTC Offset annotation.
    ///
    /// These annotations must always be consistent as they should be either the same value or are inconsistent.
    ///
    /// ```
    /// use icu_calendar::Iso;
    /// use icu_time::{
    ///     zone::UtcOffset, ParseError, TimeZone, TimeZoneInfo, ZonedDateTime,
    /// };
    /// use tinystr::tinystr;
    ///
    /// let consistent_tz_from_both = ZonedDateTime::try_offset_only_from_str(
    ///     "2024-08-08T12:08:19-05:00[-05:00]",
    ///     Iso,
    /// )
    /// .unwrap();
    ///
    /// assert_eq!(
    ///     consistent_tz_from_both.zone,
    ///     UtcOffset::try_from_seconds(-18000).unwrap()
    /// );
    ///
    /// let inconsistent_tz_from_both = ZonedDateTime::try_offset_only_from_str(
    ///     "2024-08-08T12:08:19-05:00[+05:00]",
    ///     Iso,
    /// );
    ///
    /// assert!(matches!(
    ///     inconsistent_tz_from_both,
    ///     Err(ParseError::InconsistentTimeUtcOffsets)
    /// ));
    /// ```
    pub fn try_from_str(
        ixdtf_str: &str,
        calendar: A,
        iana_parser: IanaParserBorrowed,
        offset_calculator: &UtcOffsetCalculator,
    ) -> Result<Self, ParseError> {
        Self::try_from_utf8(
            ixdtf_str.as_bytes(),
            calendar,
            iana_parser,
            offset_calculator,
        )
    }

    /// Create a [`ZonedDateTime`] in any calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// See [`Self::try_from_str`].
    pub fn try_from_utf8(
        ixdtf_str: &[u8],
        calendar: A,
        iana_parser: IanaParserBorrowed,
        offset_calculator: &UtcOffsetCalculator,
    ) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let date = Date::try_from_ixdtf_record(&ixdtf_record, calendar)?;
        let time = Time::try_from_ixdtf_record(&ixdtf_record)?;
        let zone = Intermediate::try_from_ixdtf_record(&ixdtf_record)?
            .full(iana_parser, offset_calculator)?;

        Ok(ZonedDateTime { date, time, zone })
    }
}

impl FromStr for DateTime<Iso> {
    type Err = ParseError;
    fn from_str(ixdtf_str: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(ixdtf_str, Iso)
    }
}

impl<A: AsCalendar> DateTime<A> {
    /// Creates a [`DateTime`] in any calendar from an IXDTF syntax string.
    ///
    /// Returns an error if the string has a calendar annotation that does not
    /// match the calendar argument, unless the argument is [`Iso`].
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Hebrew;
    /// use icu::time::DateTime;
    ///
    /// let datetime =
    ///     DateTime::try_from_str("2024-07-17T16:01:17.045[u-ca=hebrew]", Hebrew)
    ///         .unwrap();
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
    /// assert_eq!(datetime.time.subsecond.number(), 45000000);
    /// ```
    pub fn try_from_str(ixdtf_str: &str, calendar: A) -> Result<Self, ParseError> {
        Self::try_from_utf8(ixdtf_str.as_bytes(), calendar)
    }

    /// Creates a [`DateTime`] in any calendar from an IXDTF syntax string.
    ///
    /// See [`Self::try_from_str()`].
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    pub fn try_from_utf8(ixdtf_str: &[u8], calendar: A) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let date = Date::try_from_ixdtf_record(&ixdtf_record, calendar)?;
        let time = Time::try_from_ixdtf_record(&ixdtf_record)?;
        Ok(Self { date, time })
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
    /// use icu::time::Time;
    ///
    /// let time = Time::try_from_str("16:01:17.045").unwrap();
    ///
    /// assert_eq!(time.hour.number(), 16);
    /// assert_eq!(time.minute.number(), 1);
    /// assert_eq!(time.second.number(), 17);
    /// assert_eq!(time.subsecond.number(), 45000000);
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
        Self::try_from_time_record(&time_record)
    }

    fn try_from_time_record(time_record: &TimeRecord) -> Result<Self, ParseError> {
        let nanosecond = time_record
            .fraction
            .map(|fraction| {
                fraction
                    .to_nanoseconds()
                    .ok_or(ParseError::ExcessivePrecision)
            })
            .transpose()?
            .unwrap_or_default();

        Ok(Self::try_new(
            time_record.hour,
            time_record.minute,
            time_record.second,
            nanosecond,
        )?)
    }
}

impl FromStr for Time {
    type Err = ParseError;
    fn from_str(ixdtf_str: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(ixdtf_str)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::TimeZone;

    #[test]
    fn max_possible_ixdtf_utc_offset() {
        assert_eq!(
            ZonedDateTime::try_offset_only_from_str("2024-08-08T12:08:19+23:59:60.999999999", Iso)
                .unwrap_err(),
            ParseError::InvalidOffsetError
        );
    }

    #[test]
    fn zone_calculations() {
        ZonedDateTime::try_offset_only_from_str("2024-08-08T12:08:19Z", Iso).unwrap();
        assert_eq!(
            ZonedDateTime::try_offset_only_from_str("2024-08-08T12:08:19Z[+08:00]", Iso)
                .unwrap_err(),
            ParseError::RequiresCalculation
        );
        assert_eq!(
            ZonedDateTime::try_offset_only_from_str("2024-08-08T12:08:19Z[Europe/Zurich]", Iso)
                .unwrap_err(),
            ParseError::MismatchedTimeZoneFields
        );
    }

    #[test]
    fn future_zone() {
        let result = ZonedDateTime::try_loose_from_str(
            "2024-08-08T12:08:19[Future/Zone]",
            Iso,
            IanaParserBorrowed::new(),
        )
        .unwrap();
        assert_eq!(result.zone.time_zone_id(), TimeZone::unknown());
        assert_eq!(result.zone.offset(), None);
    }

    #[test]
    fn lax() {
        ZonedDateTime::try_location_only_from_str(
            "2024-10-18T15:44[America/Los_Angeles]",
            icu_calendar::cal::Gregorian,
            IanaParserBorrowed::new(),
        )
        .unwrap();
    }
}
