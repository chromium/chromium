// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use ffi::*;

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::provider::DataError, Struct, compact)]
    #[diplomat::rust_link(icu::provider::DataErrorKind, Enum, compact)]
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
    pub enum FixedDecimalParseError {
        Unknown = 0x00,
        Limit = 0x01,
        Syntax = 0x02,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[diplomat::rust_link(fixed_decimal::LimitError, Struct, compact)]
    #[cfg(feature = "decimal")]
    pub struct FixedDecimalLimitError;

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::calendar::RangeError, Struct, compact)]
    #[diplomat::rust_link(icu::calendar::DateError, Enum, compact)]
    #[cfg(any(feature = "datetime", feature = "timezone", feature = "calendar"))]
    pub enum CalendarError {
        Unknown = 0x00,
        OutOfRange = 0x01,
        UnknownEra = 0x02,
        UnknownMonthCode = 0x03,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::calendar::ParseError, Enum, compact)]
    #[cfg(any(feature = "datetime", feature = "timezone", feature = "calendar"))]
    pub enum CalendarParseError {
        Unknown = 0x00,
        InvalidSyntax = 0x01,
        OutOfRange = 0x02,
        MissingFields = 0x03,
        UnknownCalendar = 0x04,
    }

    #[derive(Debug, PartialEq, Eq)]
    #[diplomat::rust_link(icu::timezone::InvalidOffsetError, Struct, compact)]
    #[cfg(any(feature = "datetime", feature = "timezone"))]
    pub struct TimeZoneInvalidOffsetError;

    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::datetime::pattern::PatternLoadError, Enum, compact)]
    #[diplomat::rust_link(icu::provider::DataError, Struct, compact)]
    #[diplomat::rust_link(icu::provider::DataErrorKind, Enum, compact)]
    pub enum DateTimeFormatterLoadError {
        Unknown = 0x00,

        UnsupportedLength = 0x8_03,
        DuplicateField = 0x8_09,
        TypeTooSpecific = 0x8_0A,

        DataMarkerNotFound = 0x01,
        DataIdentifierNotFound = 0x02,
        DataInvalidRequest = 0x03,
        DataInconsistentData = 0x04,
        DataDowncast = 0x05,
        DataDeserialize = 0x06,
        DataCustom = 0x07,
        DataIo = 0x08,
    }

    // TODO: This type is currently never constructed, as all formatters perform lossy formatting.
    #[derive(Debug, PartialEq, Eq)]
    #[repr(C)]
    #[diplomat::rust_link(icu::datetime::DateTimeWriteError, Enum, compact)]
    pub enum DateTimeFormatError {
        Unknown = 0x00,
        MissingInputField = 0x01,
        ZoneInfoMissingFields = 0x02, // FFI-only error
        InvalidEra = 0x03,
        InvalidMonthCode = 0x04,
        InvalidCyclicYear = 0x05,
        NamesNotLoaded = 0x10,
        FixedDecimalFormatterNotLoaded = 0x11,
        UnsupportedField = 0x12,
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

#[cfg(any(feature = "datetime", feature = "timezone", feature = "calendar"))]
impl From<icu_calendar::RangeError> for CalendarError {
    fn from(_: icu_calendar::RangeError) -> Self {
        Self::OutOfRange
    }
}

#[cfg(any(feature = "datetime", feature = "timezone", feature = "calendar"))]
impl From<icu_calendar::DateError> for CalendarError {
    fn from(e: icu_calendar::DateError) -> Self {
        match e {
            icu_calendar::DateError::Range { .. } => Self::OutOfRange,
            icu_calendar::DateError::UnknownEra(..) => Self::UnknownEra,
            icu_calendar::DateError::UnknownMonthCode(..) => Self::UnknownMonthCode,
            _ => Self::Unknown,
        }
    }
}

#[cfg(any(feature = "datetime", feature = "timezone", feature = "calendar"))]
impl From<icu_calendar::ParseError> for CalendarParseError {
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

#[cfg(feature = "datetime")]
impl From<icu_datetime::DateTimeFormatterLoadError> for DateTimeFormatterLoadError {
    fn from(e: icu_datetime::DateTimeFormatterLoadError) -> Self {
        match e {
            icu_datetime::DateTimeFormatterLoadError::Names(
                icu_datetime::pattern::PatternLoadError::ConflictingField(_),
            ) => Self::DuplicateField,
            icu_datetime::DateTimeFormatterLoadError::Names(
                icu_datetime::pattern::PatternLoadError::UnsupportedLength(_),
            ) => Self::UnsupportedLength,
            icu_datetime::DateTimeFormatterLoadError::Names(
                icu_datetime::pattern::PatternLoadError::TypeTooSpecific(_),
            ) => Self::TypeTooSpecific,
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
impl From<icu_datetime::DateTimeWriteError> for DateTimeFormatError {
    fn from(value: icu_datetime::DateTimeWriteError) -> Self {
        match value {
            icu_datetime::DateTimeWriteError::MissingInputField(..) => Self::MissingInputField,
            icu_datetime::DateTimeWriteError::InvalidEra(..) => Self::InvalidEra,
            icu_datetime::DateTimeWriteError::InvalidMonthCode(..) => Self::InvalidMonthCode,
            icu_datetime::DateTimeWriteError::InvalidCyclicYear { .. } => Self::InvalidCyclicYear,
            icu_datetime::DateTimeWriteError::NamesNotLoaded(..) => Self::NamesNotLoaded,
            icu_datetime::DateTimeWriteError::FixedDecimalFormatterNotLoaded => {
                Self::FixedDecimalFormatterNotLoaded
            }
            icu_datetime::DateTimeWriteError::UnsupportedField(..) => Self::UnsupportedField,
            _ => Self::Unknown,
        }
    }
}

#[cfg(any(feature = "decimal", feature = "plurals"))]
impl From<fixed_decimal::ParseError> for FixedDecimalParseError {
    fn from(e: fixed_decimal::ParseError) -> Self {
        match e {
            fixed_decimal::ParseError::Limit => Self::Limit,
            fixed_decimal::ParseError::Syntax => Self::Syntax,
            _ => Self::Unknown,
        }
    }
}

#[cfg(feature = "decimal")]
impl From<fixed_decimal::LimitError> for FixedDecimalLimitError {
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

#[cfg(any(feature = "timezone", feature = "datetime"))]
impl From<icu_timezone::InvalidOffsetError> for TimeZoneInvalidOffsetError {
    fn from(_: icu_timezone::InvalidOffsetError) -> Self {
        Self
    }
}
