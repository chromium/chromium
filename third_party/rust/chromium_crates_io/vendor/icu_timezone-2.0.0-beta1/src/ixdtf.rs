// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{
    provider::{names::IanaToBcp47MapV3Marker, ZoneOffsetPeriodV1Marker},
    time_zone::models,
    CustomZonedDateTime, InvalidOffsetError, TimeZoneBcp47Id, TimeZoneIdMapper,
    TimeZoneIdMapperBorrowed, TimeZoneInfo, UtcOffset, ZoneOffsetCalculator, ZoneOffsets,
    ZoneVariant,
};
#[cfg(feature = "compiled_data")]
use icu_calendar::AnyCalendar;
use icu_calendar::{Date, DateError, DateTime, Iso, RangeError, Time};
use icu_provider::prelude::*;
use ixdtf::{
    parsers::records::{
        DateRecord, IxdtfParseRecord, TimeRecord, TimeZoneAnnotation, TimeZoneRecord,
        UtcOffsetRecord, UtcOffsetRecordOrZ,
    },
    ParseError as IxdtfParseError,
};

/// The error type for parsing IXDTF syntax strings in `icu_timezone`.
#[derive(Debug, PartialEq)]
#[non_exhaustive]
pub enum ParseError {
    /// Syntax error for IXDTF string.
    Syntax(IxdtfParseError),
    /// Parsed record is out of valid date range.
    Range(RangeError),
    /// Parsed date and time records were not a valid ISO date.
    Date(DateError),
    /// There were missing fields required to parse component.
    MissingFields,
    /// There were two offsets provided that were not consistent with each other.
    InconsistentTimeZoneOffsets,
    /// There was an invalid Offset.
    InvalidOffsetError,
    /// The set of time zone fields was not expected for the given type.
    /// For example, if a named time zone was present with offset-only parsing,
    /// or an offset was present with named-time-zone-only parsing.
    MismatchedTimeZoneFields,
    /// An unknown calendar was provided.
    UnknownCalendar,
    /// A timezone calculation is required to interpret this string, which is not supported.
    ///
    /// # Example
    /// ```
    /// use icu::timezone::{IxdtfParser, ParseError};
    ///
    /// // This timestamp is in UTC, and requires a time zone calculation in order to display a Zurich time.
    /// assert_eq!(
    ///     IxdtfParser::new().try_loose_iso_from_str("2024-08-12T12:32:00Z[Europe/Zurich]").unwrap_err(),
    ///     ParseError::RequiresCalculation,
    /// );
    ///
    /// // These timestamps are in local time
    /// IxdtfParser::new().try_loose_iso_from_str("2024-08-12T14:32:00+02:00[Europe/Zurich]").unwrap();
    /// IxdtfParser::new().try_loose_iso_from_str("2024-08-12T14:32:00[Europe/Zurich]").unwrap();
    /// ```
    RequiresCalculation,
}

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

#[derive(Debug)]
/// ✨ *Enabled with the `ixdtf` Cargo feature.*
pub struct IxdtfParser {
    mapper: TimeZoneIdMapper,
    offsets: ZoneOffsetCalculator,
}

impl IxdtfParser {
    /// Creates a new `IxdtfParser` from compiled data.
    #[cfg(feature = "compiled_data")]
    #[allow(clippy::new_without_default)]
    pub fn new() -> Self {
        Self {
            mapper: TimeZoneIdMapper::new().static_to_owned(),
            offsets: ZoneOffsetCalculator::new(),
        }
    }

    icu_provider::gen_any_buffer_data_constructors!(
        () -> error: DataError,
        functions: [
            new: skip,
            try_new_with_any_provider,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<P>(provider: &P) -> Result<Self, DataError>
    where
        P: ?Sized + DataProvider<ZoneOffsetPeriodV1Marker> + DataProvider<IanaToBcp47MapV3Marker>,
    {
        Ok(Self {
            mapper: TimeZoneIdMapper::try_new_unstable(provider)?,
            offsets: ZoneOffsetCalculator::try_new_unstable(provider)?,
        })
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
                    return Err(ParseError::InconsistentTimeZoneOffsets);
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
        mapper: TimeZoneIdMapperBorrowed<'_>,
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
        let time_zone_id = mapper.iana_bytes_to_bcp47(iana_identifier);
        let iso = DateTime::<Iso>::try_new_iso(
            self.date.year,
            self.date.month,
            self.date.day,
            self.time.hour,
            self.time.minute,
            self.time.second,
        )?;
        let offset = match time_zone_id.as_str() {
            "utc" | "gmt" => Some(UtcOffset::zero()),
            _ => None,
        };
        Ok(time_zone_id
            .with_offset(offset)
            .at_time((iso.date, iso.time)))
    }

    fn loose(
        self,
        mapper: TimeZoneIdMapperBorrowed<'_>,
    ) -> Result<TimeZoneInfo<models::AtTime>, ParseError> {
        let time_zone_id = match self.iana_identifier {
            Some(iana_identifier) => {
                if self.is_z {
                    return Err(ParseError::RequiresCalculation);
                }
                mapper.iana_bytes_to_bcp47(iana_identifier)
            }
            None if self.is_z => TimeZoneBcp47Id(tinystr::tinystr!(8, "utc")),
            None => TimeZoneBcp47Id::unknown(),
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
        let iso = DateTime::<Iso>::try_new_iso(
            self.date.year,
            self.date.month,
            self.date.day,
            self.time.hour,
            self.time.minute,
            self.time.second,
        )?;
        Ok(time_zone_id
            .with_offset(offset)
            .at_time((iso.date, iso.time)))
    }

    fn full(
        self,
        mapper: TimeZoneIdMapperBorrowed<'_>,
        zone_offset_calculator: &ZoneOffsetCalculator,
    ) -> Result<TimeZoneInfo<models::Full>, ParseError> {
        let Some(offset) = self.offset else {
            return Err(ParseError::MismatchedTimeZoneFields);
        };
        let Some(iana_identifier) = self.iana_identifier else {
            return Err(ParseError::MismatchedTimeZoneFields);
        };
        let time_zone_id = mapper.iana_bytes_to_bcp47(iana_identifier);
        let date = Date::try_new_iso(self.date.year, self.date.month, self.date.day)?;
        let time = Time::try_new(self.time.hour, self.time.minute, self.time.second, 0)?;
        let offset = UtcOffset::try_from_utc_offset_record(offset)?;
        let zone_variant = match zone_offset_calculator
            .compute_offsets_from_time_zone(time_zone_id, (date, time))
        {
            Some(ZoneOffsets { standard, daylight }) => {
                if offset == standard {
                    ZoneVariant::Standard
                } else if Some(offset) == daylight {
                    ZoneVariant::Daylight
                } else {
                    return Err(ParseError::InvalidOffsetError);
                }
            }
            None => {
                // time_zone_id not found; Etc/Unknown?
                debug_assert_eq!(time_zone_id.0.as_str(), "unk");
                ZoneVariant::Standard
            }
        };
        Ok(time_zone_id
            .with_offset(Some(offset))
            .at_time((date, time))
            .with_zone_variant(zone_variant))
    }
}

impl IxdtfParser {
    /// Create a [`CustomZonedDateTime`] in ISO-8601 calendar from an IXDTF syntax string.
    ///
    /// This function is "strict": the string should have only an offset and no named time zone.
    pub fn try_offset_only_iso_from_str(
        &self,
        ixdtf_str: &str,
    ) -> Result<CustomZonedDateTime<Iso, UtcOffset>, ParseError> {
        self.try_offset_only_iso_from_utf8(ixdtf_str.as_bytes())
    }

    /// Create a [`CustomZonedDateTime`] in ISO-8601 calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// This function is "strict": the string should have only an offset and no named time zone.
    pub fn try_offset_only_iso_from_utf8(
        &self,
        ixdtf_str: &[u8],
    ) -> Result<CustomZonedDateTime<Iso, UtcOffset>, ParseError> {
        let ixdtf_record = ixdtf::parsers::IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let intermediate = Intermediate::try_from_ixdtf_record(&ixdtf_record)?;
        let time_zone = intermediate.offset_only()?;
        self.try_iso_from_ixdtf_record(&ixdtf_record, time_zone)
    }

    /// Create a [`CustomZonedDateTime`] in ISO-8601 calendar from an IXDTF syntax string.
    ///
    /// This function is "strict": the string should have only a named time zone and no offset.
    pub fn try_location_only_iso_from_str(
        &self,
        ixdtf_str: &str,
    ) -> Result<CustomZonedDateTime<Iso, TimeZoneInfo<models::AtTime>>, ParseError> {
        self.try_location_only_iso_from_utf8(ixdtf_str.as_bytes())
    }

    /// Create a [`CustomZonedDateTime`] in ISO-8601 calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// This function is "strict": the string should have only a named time zone and no offset.
    pub fn try_location_only_iso_from_utf8(
        &self,
        ixdtf_str: &[u8],
    ) -> Result<CustomZonedDateTime<Iso, TimeZoneInfo<models::AtTime>>, ParseError> {
        let ixdtf_record = ixdtf::parsers::IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let intermediate = Intermediate::try_from_ixdtf_record(&ixdtf_record)?;
        let time_zone = intermediate.location_only(self.mapper.as_borrowed())?;
        self.try_iso_from_ixdtf_record(&ixdtf_record, time_zone)
    }

    /// Create a [`CustomZonedDateTime`] in ISO-8601 calendar from an IXDTF syntax string.
    ///
    /// This function is "loose": the string can have an offset, and named time zone, both, or
    /// neither. If the named time zone is missing, it is returned as Etc/Unknown.
    ///
    /// The zone variant is _not_ calculated with this function. If you need it, use
    /// [`Self::try_iso_from_str`].
    pub fn try_loose_iso_from_str(
        &self,
        ixdtf_str: &str,
    ) -> Result<CustomZonedDateTime<Iso, TimeZoneInfo<models::AtTime>>, ParseError> {
        self.try_loose_iso_from_utf8(ixdtf_str.as_bytes())
    }

    /// Create a [`CustomZonedDateTime`] in ISO-8601 calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// This function is "loose": the string can have an offset, and named time zone, both, or
    /// neither. If the named time zone is missing, it is returned as Etc/Unknown.
    ///
    /// The zone variant is _not_ calculated with this function. If you need it, use
    /// [`Self::try_iso_from_utf8`].
    pub fn try_loose_iso_from_utf8(
        &self,
        ixdtf_str: &[u8],
    ) -> Result<CustomZonedDateTime<Iso, TimeZoneInfo<models::AtTime>>, ParseError> {
        let ixdtf_record = ixdtf::parsers::IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let intermediate = Intermediate::try_from_ixdtf_record(&ixdtf_record)?;
        let time_zone = intermediate.loose(self.mapper.as_borrowed())?;
        self.try_iso_from_ixdtf_record(&ixdtf_record, time_zone)
    }

    /// Create a [`CustomZonedDateTime`] in ISO-8601 calendar from an IXDTF syntax string.
    ///
    /// The string should have both an offset and a named time zone.
    ///
    /// ```
    /// use icu_timezone::{
    ///     IxdtfParser, TimeZoneBcp47Id, TimeZoneInfo, UtcOffset, ZoneVariant,
    /// };
    /// use tinystr::tinystr;
    ///
    /// let zoneddatetime = IxdtfParser::new()
    ///     .try_iso_from_str("2024-08-08T12:08:19-05:00[America/Chicago]")
    ///     .unwrap();
    ///
    /// assert_eq!(zoneddatetime.date.year().extended_year, 2024);
    /// assert_eq!(
    ///     zoneddatetime.date.month().standard_code,
    ///     icu::calendar::types::MonthCode(tinystr::tinystr!(4, "M08"))
    /// );
    /// assert_eq!(zoneddatetime.date.day_of_month().0, 8);
    ///
    /// assert_eq!(zoneddatetime.time.hour.number(), 12);
    /// assert_eq!(zoneddatetime.time.minute.number(), 8);
    /// assert_eq!(zoneddatetime.time.second.number(), 19);
    /// assert_eq!(zoneddatetime.time.nanosecond.number(), 0);
    /// assert_eq!(
    ///     zoneddatetime.zone.time_zone_id(),
    ///     TimeZoneBcp47Id(tinystr!(8, "uschi"))
    /// );
    /// assert_eq!(
    ///     zoneddatetime.zone.offset(),
    ///     Some(UtcOffset::try_from_seconds(-18000).unwrap())
    /// );
    /// assert_eq!(zoneddatetime.zone.zone_variant(), ZoneVariant::Daylight);
    /// let (_, _) = zoneddatetime.zone.local_time();
    /// ```
    ///
    /// For more information on date, time, and time zone parsing,
    /// see [`Self::try_from_str`].
    pub fn try_iso_from_str(
        &self,
        ixdtf_str: &str,
    ) -> Result<CustomZonedDateTime<Iso, TimeZoneInfo<models::Full>>, ParseError> {
        self.try_iso_from_utf8(ixdtf_str.as_bytes())
    }

    /// Create a [`CustomZonedDateTime`] in ISO-8601 calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// The string should have both an offset and a named time zone.
    ///
    /// See [`Self::try_iso_from_str`].
    pub fn try_iso_from_utf8(
        &self,
        ixdtf_str: &[u8],
    ) -> Result<CustomZonedDateTime<Iso, TimeZoneInfo<models::Full>>, ParseError> {
        let ixdtf_record = ixdtf::parsers::IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let intermediate = Intermediate::try_from_ixdtf_record(&ixdtf_record)?;
        let time_zone = intermediate.full(self.mapper.as_borrowed(), &self.offsets)?;
        self.try_iso_from_ixdtf_record(&ixdtf_record, time_zone)
    }

    /// Create a [`CustomZonedDateTime`] in any calendar from an IXDTF syntax string.
    ///
    /// This function is "strict": the string should have only an offset and no named time zone.
    #[cfg(feature = "compiled_data")]
    pub fn try_offset_only_from_str(
        &self,
        ixdtf_str: &str,
    ) -> Result<CustomZonedDateTime<AnyCalendar, UtcOffset>, ParseError> {
        self.try_offset_only_from_utf8(ixdtf_str.as_bytes())
    }

    /// Create a [`CustomZonedDateTime`] in any calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// This function is "strict": the string should have only an offset and no named time zone.
    #[cfg(feature = "compiled_data")]
    pub fn try_offset_only_from_utf8(
        &self,
        ixdtf_str: &[u8],
    ) -> Result<CustomZonedDateTime<AnyCalendar, UtcOffset>, ParseError> {
        let ixdtf_record = ixdtf::parsers::IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let intermediate = Intermediate::try_from_ixdtf_record(&ixdtf_record)?;
        let time_zone = intermediate.offset_only()?;
        self.try_from_ixdtf_record(&ixdtf_record, time_zone)
    }

    /// Create a [`CustomZonedDateTime`] in any calendar from an IXDTF syntax string.
    ///
    /// This function is "strict": the string should have only a named time zone and no offset.
    #[cfg(feature = "compiled_data")]
    pub fn try_location_only_from_str(
        &self,
        ixdtf_str: &str,
    ) -> Result<CustomZonedDateTime<AnyCalendar, TimeZoneInfo<models::AtTime>>, ParseError> {
        self.try_location_only_from_utf8(ixdtf_str.as_bytes())
    }

    /// Create a [`CustomZonedDateTime`] in any calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// This function is "strict": the string should have only a named time zone and no offset.
    #[cfg(feature = "compiled_data")]
    pub fn try_location_only_from_utf8(
        &self,
        ixdtf_str: &[u8],
    ) -> Result<CustomZonedDateTime<AnyCalendar, TimeZoneInfo<models::AtTime>>, ParseError> {
        let ixdtf_record = ixdtf::parsers::IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let intermediate = Intermediate::try_from_ixdtf_record(&ixdtf_record)?;
        let time_zone = intermediate.location_only(self.mapper.as_borrowed())?;
        self.try_from_ixdtf_record(&ixdtf_record, time_zone)
    }

    /// Create a [`CustomZonedDateTime`] in any calendar from an IXDTF syntax string.
    ///
    /// This function is "loose": the string can have an offset, and named time zone, both, or
    /// neither. If the named time zone is missing, it is returned as Etc/Unknown.
    ///
    /// The zone variant is _not_ calculated with this function. If you need it, use
    /// [`Self::try_from_str`].
    #[cfg(feature = "compiled_data")]
    pub fn try_loose_from_str(
        &self,
        ixdtf_str: &str,
    ) -> Result<CustomZonedDateTime<AnyCalendar, TimeZoneInfo<models::AtTime>>, ParseError> {
        self.try_loose_from_utf8(ixdtf_str.as_bytes())
    }

    /// Create a [`CustomZonedDateTime`] in any calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// This function is "loose": the string can have an offset, and named time zone, both, or
    /// neither. If the named time zone is missing, it is returned as Etc/Unknown.
    ///
    /// The zone variant is _not_ calculated with this function. If you need it, use
    /// [`Self::try_from_utf8`].
    #[cfg(feature = "compiled_data")]
    pub fn try_loose_from_utf8(
        &self,
        ixdtf_str: &[u8],
    ) -> Result<CustomZonedDateTime<AnyCalendar, TimeZoneInfo<models::AtTime>>, ParseError> {
        let ixdtf_record = ixdtf::parsers::IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let intermediate = Intermediate::try_from_ixdtf_record(&ixdtf_record)?;
        let time_zone = intermediate.loose(self.mapper.as_borrowed())?;
        self.try_from_ixdtf_record(&ixdtf_record, time_zone)
    }

    /// Create a [`CustomZonedDateTime`] in any calendar from an IXDTF syntax string.
    ///
    /// The string should have both an offset and a named time zone.
    ///
    /// For more information on IXDTF, see the [`ixdtf`] crate.
    ///
    /// This is a convenience constructor that uses compiled data. For custom data providers,
    /// use [`ixdtf`] and/or the other primitives in this crate such as [`TimeZoneIdMapper`].
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use icu_timezone::{
    ///     IxdtfParser, TimeZoneBcp47Id, TimeZoneInfo, UtcOffset, ZoneVariant,
    /// };
    /// use tinystr::tinystr;
    ///
    /// let zoneddatetime = IxdtfParser::new()
    ///     .try_from_str("2024-08-08T12:08:19-05:00[America/Chicago][u-ca=hebrew]")
    ///     .unwrap();
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
    /// assert_eq!(zoneddatetime.time.nanosecond.number(), 0);
    /// assert_eq!(
    ///     zoneddatetime.zone.time_zone_id(),
    ///     TimeZoneBcp47Id(tinystr!(8, "uschi"))
    /// );
    /// assert_eq!(
    ///     zoneddatetime.zone.offset(),
    ///     Some(UtcOffset::try_from_seconds(-18000).unwrap())
    /// );
    /// assert_eq!(zoneddatetime.zone.zone_variant(), ZoneVariant::Daylight);
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
    /// use icu_timezone::{IxdtfParser, TimeZoneInfo, UtcOffset};
    ///
    /// let tz_from_offset = IxdtfParser::new()
    ///     .try_offset_only_from_str("2024-08-08T12:08:19-05:00")
    ///     .unwrap();
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
    /// use icu_timezone::{
    ///     IxdtfParser, TimeZoneBcp47Id, TimeZoneInfo, UtcOffset, ZoneVariant,
    /// };
    /// use tinystr::tinystr;
    ///
    /// let tz_from_offset_annotation = IxdtfParser::new()
    ///     .try_offset_only_from_str("2024-08-08T12:08:19[-05:00]")
    ///     .unwrap();
    /// let tz_from_iana_annotation = IxdtfParser::new()
    ///     .try_location_only_from_str("2024-08-08T12:08:19[America/Chicago]")
    ///     .unwrap();
    ///
    /// assert_eq!(
    ///     tz_from_offset_annotation.zone,
    ///     UtcOffset::try_from_seconds(-18000).unwrap()
    /// );
    ///
    /// assert_eq!(
    ///     tz_from_iana_annotation.zone.time_zone_id(),
    ///     TimeZoneBcp47Id(tinystr!(8, "uschi"))
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
    /// use icu_timezone::{TimeZoneInfo, IxdtfParser, UtcOffset, TimeZoneBcp47Id, ZoneVariant, ParseError};
    /// use tinystr::tinystr;
    ///
    /// let consistent_tz_from_both = IxdtfParser::new().try_from_str("2024-08-08T12:08:19-05:00[America/Chicago]").unwrap();
    ///
    ///
    /// assert_eq!(consistent_tz_from_both.zone.time_zone_id(), TimeZoneBcp47Id(tinystr!(8, "uschi")));
    /// assert_eq!(consistent_tz_from_both.zone.offset(), Some(UtcOffset::try_from_seconds(-18000).unwrap()));
    /// assert_eq!(consistent_tz_from_both.zone.zone_variant(), ZoneVariant::Daylight);
    /// let (_, _) = consistent_tz_from_both.zone.local_time();
    ///
    /// // We know that America/Los_Angeles never used a -05:00 offset at any time of the year 2024
    /// assert_eq!(
    ///     IxdtfParser::new().try_from_str("2024-08-08T12:08:19-05:00[America/Los_Angeles]").unwrap_err(),
    ///     ParseError::InvalidOffsetError
    /// );
    ///
    /// // We don't know that America/Los_Angeles didn't use standard time (-08:00) in August
    /// assert!(
    ///     IxdtfParser::new().try_from_str("2024-08-08T12:08:19-08:00[America/Los_Angeles]").is_ok()
    /// );
    /// ```
    ///
    /// ### DateTime UTC offset with UTC Offset annotation.
    ///
    /// These annotations must always be consistent as they should be either the same value or are inconsistent.
    ///
    /// ```
    /// use icu_timezone::{
    ///     IxdtfParser, ParseError, TimeZoneBcp47Id, TimeZoneInfo, UtcOffset,
    /// };
    /// use tinystr::tinystr;
    ///
    /// let consistent_tz_from_both = IxdtfParser::new()
    ///     .try_offset_only_from_str("2024-08-08T12:08:19-05:00[-05:00]")
    ///     .unwrap();
    ///
    /// assert_eq!(
    ///     consistent_tz_from_both.zone,
    ///     UtcOffset::try_from_seconds(-18000).unwrap()
    /// );
    ///
    /// let inconsistent_tz_from_both = IxdtfParser::new()
    ///     .try_offset_only_from_str("2024-08-08T12:08:19-05:00[+05:00]");
    ///
    /// assert!(matches!(
    ///     inconsistent_tz_from_both,
    ///     Err(ParseError::InconsistentTimeZoneOffsets)
    /// ));
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn try_from_str(
        &self,
        ixdtf_str: &str,
    ) -> Result<CustomZonedDateTime<AnyCalendar, TimeZoneInfo<models::Full>>, ParseError> {
        self.try_from_utf8(ixdtf_str.as_bytes())
    }

    /// Create a [`CustomZonedDateTime`] in any calendar from IXDTF syntax UTF-8 bytes.
    ///
    /// The string should have both an offset and a named time zone.
    ///
    /// See [`Self::try_from_str`].
    #[cfg(feature = "compiled_data")]
    pub fn try_from_utf8(
        &self,
        ixdtf_str: &[u8],
    ) -> Result<CustomZonedDateTime<AnyCalendar, TimeZoneInfo<models::Full>>, ParseError> {
        let ixdtf_record = ixdtf::parsers::IxdtfParser::from_utf8(ixdtf_str).parse()?;
        let intermediate = Intermediate::try_from_ixdtf_record(&ixdtf_record)?;
        let time_zone = intermediate.full(self.mapper.as_borrowed(), &self.offsets)?;
        self.try_from_ixdtf_record(&ixdtf_record, time_zone)
    }

    #[cfg(feature = "compiled_data")]
    fn try_from_ixdtf_record<Z>(
        &self,
        ixdtf_record: &IxdtfParseRecord,
        zone: Z,
    ) -> Result<CustomZonedDateTime<AnyCalendar, Z>, ParseError> {
        let iso_zdt = self.try_iso_from_ixdtf_record(ixdtf_record, zone)?;

        // Find the calendar (based off icu_calendar's AnyCalendar try_from)
        let calendar_id = ixdtf_record.calendar.unwrap_or(b"iso");
        let calendar_kind = icu_calendar::AnyCalendarKind::get_for_bcp47_bytes(calendar_id)
            .ok_or(ParseError::UnknownCalendar)?;
        let calendar = AnyCalendar::new_for_kind(calendar_kind);

        Ok(CustomZonedDateTime {
            date: iso_zdt.date.to_any().to_calendar(calendar),
            time: iso_zdt.time,
            zone: iso_zdt.zone,
        })
    }

    fn try_iso_from_ixdtf_record<Z>(
        &self,
        ixdtf_record: &IxdtfParseRecord,
        zone: Z,
    ) -> Result<CustomZonedDateTime<Iso, Z>, ParseError> {
        let date_record = ixdtf_record.date.ok_or(ParseError::MissingFields)?;
        let date = Date::try_new_iso(date_record.year, date_record.month, date_record.day)?;
        let time_record = ixdtf_record.time.ok_or(ParseError::MissingFields)?;
        let time = Time::try_new(
            time_record.hour,
            time_record.minute,
            time_record.second,
            time_record.nanosecond,
        )?;

        Ok(CustomZonedDateTime { date, time, zone })
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::TimeZoneBcp47Id;

    #[test]
    fn max_possible_ixdtf_utc_offset() {
        assert_eq!(
            IxdtfParser::new()
                .try_offset_only_iso_from_str("2024-08-08T12:08:19+23:59:60.999999999")
                .unwrap_err(),
            ParseError::InvalidOffsetError
        );
    }

    #[test]
    fn zone_calculations() {
        IxdtfParser::new()
            .try_offset_only_iso_from_str("2024-08-08T12:08:19Z")
            .unwrap();
        assert_eq!(
            IxdtfParser::new()
                .try_offset_only_iso_from_str("2024-08-08T12:08:19Z[+08:00]")
                .unwrap_err(),
            ParseError::RequiresCalculation
        );
        assert_eq!(
            IxdtfParser::new()
                .try_offset_only_iso_from_str("2024-08-08T12:08:19Z[Europe/Zurich]")
                .unwrap_err(),
            ParseError::MismatchedTimeZoneFields
        );
    }

    #[test]
    fn future_zone() {
        let result = IxdtfParser::new()
            .try_loose_from_str("2024-08-08T12:08:19[Future/Zone]")
            .unwrap();
        assert_eq!(result.zone.time_zone_id(), TimeZoneBcp47Id::unknown());
        assert_eq!(result.zone.offset(), None);
    }
}
