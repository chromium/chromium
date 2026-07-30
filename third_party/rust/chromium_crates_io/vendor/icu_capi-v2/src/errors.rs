// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use ffi::*;

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    #[cfg(feature = "datetime")]
    use diplomat_runtime::DiplomatOption;

    #[cfg(feature = "datetime")]
    use crate::unstable::calendar::ffi::CalendarKind;

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu_provider::DataError, Struct, compact)]
    #[diplomat::rust_link(icu_provider::DataErrorKind, Enum, compact)]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub enum DataError {
        Unknown = 0x00,
        MarkerNotFound = 0x01,
        IdentifierNotFound = 0x02,
        InvalidRequest = 0x03,
        InconsistentData = 0x04,
        Downcast = 0x05,
        Deserialize = 0x06,
        Custom = 0x07,
        Io = 0x08,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::locale::ParseError, Enum, compact)]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub enum LocaleParseError {
        Unknown = 0x00,
        Language = 0x01,
        Subtag = 0x02,
        Extension = 0x03,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(fixed_decimal::ParseError, Enum, compact)]
    #[cfg(any(feature = "decimal", feature = "plurals"))]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub enum DecimalParseError {
        Unknown = 0x00,
        Limit = 0x01,
        Syntax = 0x02,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[diplomat::rust_link(fixed_decimal::LimitError, Struct, compact)]
    #[cfg(feature = "decimal")]
    #[diplomat::attr(auto, error)]
    pub struct DecimalLimitError;

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::calendar::RangeError, Struct, compact)]
    #[diplomat::rust_link(icu::calendar::DateError, Enum, compact)]
    #[diplomat::rust_link(icu::calendar::error::LunisolarRangeError, Enum, hidden)]
    #[diplomat::rust_link(icu::calendar::error::MonthCodeParseError, Enum, compact)]
    #[diplomat::rust_link(icu::calendar::error::DateNewError, Enum, compact)]
    #[cfg(feature = "calendar")]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub enum CalendarError {
        Unknown = 0x00,
        OutOfRange = 0x01,
        UnknownEra = 0x02,
        UnknownMonthCode = 0x03,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::calendar::error::DateFromFieldsError, Enum, compact)]
    #[cfg(feature = "calendar")]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub enum CalendarDateFromFieldsError {
        Unknown = 0x00,
        InvalidDay = 0x01,
        InvalidOrdinalMonth = 0x09,
        InvalidEra = 0x02,
        MonthCodeInvalidSyntax = 0x03,
        MonthNotInCalendar = 0x04,
        MonthNotInYear = 0x05,
        InconsistentYear = 0x06,
        InconsistentMonth = 0x07,
        NotEnoughFields = 0x08,
        TooManyFields = 0x0A,
        Overflow = 0x0B,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::calendar::error::DateAddError, Enum, compact)]
    #[cfg(feature = "calendar")]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub enum CalendarDateAddError {
        Unknown = 0x00,
        InvalidDay = 0x01,
        MonthNotInYear = 0x02,
        Overflow = 0x03,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::calendar::error::MismatchedCalendarError, Struct, compact)]
    #[cfg(feature = "calendar")]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub struct CalendarMismatchedCalendarError;

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::calendar::error::DateDurationParseError, Enum, compact)]
    #[cfg(feature = "calendar")]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub enum DateDurationParseError {
        InvalidStructure = 0x00,
        TimeNotSupported = 0x01,
        MissingValue = 0x02,
        DuplicateUnit = 0x03,
        NumberOverflow = 0x04,
        PlusNotAllowed = 0x05,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::calendar::ParseError, Enum, compact)]
    #[diplomat::rust_link(icu::time::ParseError, Enum, compact)]
    #[cfg(feature = "calendar")]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub enum Rfc9557ParseError {
        Unknown = 0x00,
        InvalidSyntax = 0x01,
        OutOfRange = 0x02,
        MissingFields = 0x03,
        UnknownCalendar = 0x04,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[diplomat::rust_link(icu::time::zone::InvalidOffsetError, Struct, compact)]
    #[cfg(feature = "datetime")]
    #[diplomat::attr(auto, error)]
    pub struct TimeZoneInvalidOffsetError;

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::datetime::DateTimeFormatterLoadError, Enum, compact)]
    #[diplomat::rust_link(icu::datetime::pattern::PatternLoadError, Enum, compact)]
    #[diplomat::rust_link(icu_provider::DataError, Struct, compact)]
    #[diplomat::rust_link(icu_provider::DataErrorKind, Enum, compact)]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub enum DateTimeFormatterLoadError {
        Unknown = 0x00,

        InvalidDateFields = 0x8_01,
        UnsupportedLength = 0x8_03,
        ConflictingField = 0x8_09,
        FormatterTooSpecific = 0x8_0A,

        DataMarkerNotFound = 0x01,
        DataIdentifierNotFound = 0x02,
        DataInvalidRequest = 0x03,
        DataInconsistentData = 0x04,
        DataDowncast = 0x05,
        DataDeserialize = 0x06,
        DataCustom = 0x07,
        DataIo = 0x08,
    }

    #[cfg(feature = "datetime")]
    #[diplomat::rust_link(icu::datetime::MismatchedCalendarError, Struct)]
    #[diplomat::attr(auto, error)]
    pub struct DateTimeMismatchedCalendarError {
        pub this_kind: CalendarKind,
        pub date_kind: DiplomatOption<CalendarKind>,
    }

    /// An error when formatting a datetime.
    ///
    /// Currently never returned by any API.
    #[cfg(feature = "datetime")]
    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(
        icu::datetime::unchecked::FormattedDateTimeUncheckedError,
        Enum,
        compact
    )]
    #[non_exhaustive]
    #[diplomat::attr(auto, error)]
    pub enum DateTimeWriteError {
        Unknown = 0x00,
        /// Unused
        MissingTimeZoneVariant = 0x01,
    }
}

impl From<icu_provider::DataError> for DataError {
    fn from(e: icu_provider::DataError) -> Self {
        match e.kind {
            icu_provider::DataErrorKind::MarkerNotFound => Self::MarkerNotFound,
            icu_provider::DataErrorKind::IdentifierNotFound => Self::IdentifierNotFound,
            icu_provider::DataErrorKind::InvalidRequest => Self::InvalidRequest,
            icu_provider::DataErrorKind::InconsistentData(..) => Self::InconsistentData,
            icu_provider::DataErrorKind::Downcast(..) => Self::Downcast,
            icu_provider::DataErrorKind::Deserialize => Self::Deserialize,
            icu_provider::DataErrorKind::Custom => Self::Custom,
            #[cfg(all(
                feature = "provider_fs",
                not(any(target_arch = "wasm32", target_os = "none"))
            ))]
            icu_provider::DataErrorKind::Io(..) => Self::Io,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "calendar")]
impl From<icu_calendar::RangeError> for CalendarError {
    fn from(_: icu_calendar::RangeError) -> Self {
        Self::OutOfRange
    }
}

#[cfg(feature = "calendar")]
impl From<icu_calendar::error::MonthCodeParseError> for CalendarError {
    fn from(value: icu_calendar::error::MonthCodeParseError) -> Self {
        match value {
            icu_calendar::error::MonthCodeParseError::InvalidSyntax => Self::UnknownMonthCode,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "calendar")]
impl From<icu_calendar::DateError> for CalendarError {
    fn from(e: icu_calendar::DateError) -> Self {
        match e {
            icu_calendar::DateError::Range { .. } => Self::OutOfRange,
            icu_calendar::DateError::UnknownEra => Self::UnknownEra,
            icu_calendar::DateError::UnknownMonthCode(..) => Self::UnknownMonthCode,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "calendar")]
impl From<icu_calendar::error::DateFromFieldsError> for CalendarDateFromFieldsError {
    fn from(e: icu_calendar::error::DateFromFieldsError) -> Self {
        match e {
            icu_calendar::error::DateFromFieldsError::InvalidDay { .. } => Self::InvalidDay,
            icu_calendar::error::DateFromFieldsError::InvalidOrdinalMonth { .. } => {
                Self::InvalidOrdinalMonth
            }
            icu_calendar::error::DateFromFieldsError::InvalidEra => Self::InvalidEra,
            icu_calendar::error::DateFromFieldsError::MonthCodeInvalidSyntax => {
                Self::MonthCodeInvalidSyntax
            }
            icu_calendar::error::DateFromFieldsError::MonthNotInCalendar => {
                Self::MonthNotInCalendar
            }
            icu_calendar::error::DateFromFieldsError::MonthNotInYear => Self::MonthNotInYear,
            icu_calendar::error::DateFromFieldsError::InconsistentYear => Self::InconsistentYear,
            icu_calendar::error::DateFromFieldsError::InconsistentMonth => Self::InconsistentMonth,
            icu_calendar::error::DateFromFieldsError::NotEnoughFields => Self::NotEnoughFields,
            icu_calendar::error::DateFromFieldsError::TooManyFields => Self::TooManyFields,
            icu_calendar::error::DateFromFieldsError::Overflow => Self::Overflow,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "calendar")]
impl From<icu_calendar::error::DateNewError> for CalendarError {
    fn from(e: icu_calendar::error::DateNewError) -> Self {
        match e {
            icu_calendar::error::DateNewError::InvalidDay { .. } => Self::OutOfRange,
            icu_calendar::error::DateNewError::InvalidEra => Self::UnknownEra,
            icu_calendar::error::DateNewError::MonthNotInCalendar => Self::UnknownMonthCode,
            icu_calendar::error::DateNewError::MonthNotInYear => Self::UnknownMonthCode,
            icu_calendar::error::DateNewError::InvalidYear => Self::OutOfRange,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "calendar")]
impl From<icu_calendar::error::DateAddError> for CalendarDateAddError {
    fn from(e: icu_calendar::error::DateAddError) -> Self {
        match e {
            icu_calendar::error::DateAddError::InvalidDay { .. } => Self::InvalidDay,
            icu_calendar::error::DateAddError::MonthNotInYear => Self::MonthNotInYear,
            icu_calendar::error::DateAddError::Overflow => Self::Overflow,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "calendar")]
impl From<icu_calendar::error::DateDurationParseError> for DateDurationParseError {
    fn from(e: icu_calendar::error::DateDurationParseError) -> Self {
        match e {
            icu_calendar::error::DateDurationParseError::InvalidStructure => Self::InvalidStructure,
            icu_calendar::error::DateDurationParseError::TimeNotSupported => Self::TimeNotSupported,
            icu_calendar::error::DateDurationParseError::MissingValue => Self::MissingValue,
            icu_calendar::error::DateDurationParseError::DuplicateUnit => Self::DuplicateUnit,
            icu_calendar::error::DateDurationParseError::NumberOverflow => Self::NumberOverflow,
            icu_calendar::error::DateDurationParseError::PlusNotAllowed => Self::PlusNotAllowed,
            _ => unreachable!(),
        }
    }
}

#[cfg(feature = "calendar")]
impl From<icu_calendar::ParseError> for Rfc9557ParseError {
    fn from(e: icu_calendar::ParseError) -> Self {
        match e {
            icu_calendar::ParseError::Syntax(_) => Self::InvalidSyntax,
            icu_calendar::ParseError::MissingFields => Self::MissingFields,
            icu_calendar::ParseError::Range(_) => Self::OutOfRange,
            icu_calendar::ParseError::UnknownCalendar => Self::UnknownCalendar,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "calendar")]
impl From<icu_time::ParseError> for Rfc9557ParseError {
    fn from(e: icu_time::ParseError) -> Self {
        match e {
            icu_time::ParseError::Syntax(_) => Self::InvalidSyntax,
            icu_time::ParseError::MissingFields => Self::MissingFields,
            icu_time::ParseError::Range(_) => Self::OutOfRange,
            icu_time::ParseError::UnknownCalendar => Self::UnknownCalendar,
            // TODO
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "datetime")]
impl From<icu_datetime::DateTimeFormatterLoadError> for DateTimeFormatterLoadError {
    fn from(e: icu_datetime::DateTimeFormatterLoadError) -> Self {
        match e {
            icu_datetime::DateTimeFormatterLoadError::Names(
                icu_datetime::pattern::PatternLoadError::ConflictingField { .. },
            ) => Self::ConflictingField,
            icu_datetime::DateTimeFormatterLoadError::Names(
                icu_datetime::pattern::PatternLoadError::UnsupportedLength(_),
            ) => Self::UnsupportedLength,
            icu_datetime::DateTimeFormatterLoadError::Names(
                icu_datetime::pattern::PatternLoadError::FormatterTooSpecific(_),
            ) => Self::FormatterTooSpecific,
            icu_datetime::DateTimeFormatterLoadError::Names(
                icu_datetime::pattern::PatternLoadError::Data(data_error, _),
            ) => data_error.into(),
            icu_datetime::DateTimeFormatterLoadError::Data(data_error) => data_error.into(),
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "datetime")]
impl From<icu_provider::DataError> for DateTimeFormatterLoadError {
    fn from(e: icu_provider::DataError) -> Self {
        match e.kind {
            icu_provider::DataErrorKind::MarkerNotFound => Self::DataMarkerNotFound,
            icu_provider::DataErrorKind::IdentifierNotFound => Self::DataIdentifierNotFound,
            icu_provider::DataErrorKind::InvalidRequest => Self::DataInvalidRequest,
            icu_provider::DataErrorKind::InconsistentData(..) => Self::DataInconsistentData,
            icu_provider::DataErrorKind::Downcast(..) => Self::DataDowncast,
            icu_provider::DataErrorKind::Deserialize => Self::DataDeserialize,
            icu_provider::DataErrorKind::Custom => Self::DataCustom,
            #[cfg(all(
                feature = "provider_fs",
                not(any(target_arch = "wasm32", target_os = "none"))
            ))]
            icu_provider::DataErrorKind::Io(..) => Self::DataIo,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "datetime")]
impl From<icu_datetime::pattern::PatternLoadError> for ffi::DateTimeFormatterLoadError {
    fn from(value: icu_datetime::pattern::PatternLoadError) -> Self {
        match value {
            icu_datetime::pattern::PatternLoadError::ConflictingField { .. } => {
                Self::ConflictingField
            }
            icu_datetime::pattern::PatternLoadError::UnsupportedLength(_) => {
                Self::UnsupportedLength
            }
            icu_datetime::pattern::PatternLoadError::FormatterTooSpecific(_) => {
                Self::FormatterTooSpecific
            }
            icu_datetime::pattern::PatternLoadError::Data(data_error, _) => data_error.into(),
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "datetime")]
impl From<icu_datetime::MismatchedCalendarError> for ffi::DateTimeMismatchedCalendarError {
    fn from(value: icu_datetime::MismatchedCalendarError) -> Self {
        Self {
            this_kind: value.this_kind.into(),
            date_kind: value.date_kind.map(Into::into).into(),
        }
    }
}

#[cfg(feature = "datetime")]
impl From<icu_datetime::unchecked::FormattedDateTimeUncheckedError> for DateTimeWriteError {
    fn from(err: icu_datetime::unchecked::FormattedDateTimeUncheckedError) -> Self {
        debug_assert!(false, "unexpected datetime formatting error: {err}");
        Self::Unknown
    }
}

#[cfg(any(feature = "decimal", feature = "plurals"))]
impl From<fixed_decimal::ParseError> for DecimalParseError {
    fn from(e: fixed_decimal::ParseError) -> Self {
        match e {
            fixed_decimal::ParseError::Limit => Self::Limit,
            fixed_decimal::ParseError::Syntax => Self::Syntax,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "decimal")]
impl From<fixed_decimal::LimitError> for DecimalLimitError {
    fn from(_: fixed_decimal::LimitError) -> Self {
        Self
    }
}

impl From<icu_locale_core::ParseError> for LocaleParseError {
    fn from(e: icu_locale_core::ParseError) -> Self {
        match e {
            icu_locale_core::ParseError::InvalidLanguage => Self::Language,
            icu_locale_core::ParseError::InvalidSubtag => Self::Subtag,
            icu_locale_core::ParseError::InvalidExtension => Self::Extension,
            icu_locale_core::ParseError::DuplicatedExtension => Self::Extension,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "datetime")]
impl From<icu_time::zone::InvalidOffsetError> for TimeZoneInvalidOffsetError {
    fn from(_: icu_time::zone::InvalidOffsetError) -> Self {
        Self
    }
}
