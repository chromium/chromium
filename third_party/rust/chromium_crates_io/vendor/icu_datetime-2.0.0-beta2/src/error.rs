// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::pattern::PatternLoadError;
use displaydoc::Display;
use icu_calendar::{
    any_calendar::AnyCalendarKind,
    types::{FormattingEra, MonthCode},
};
use icu_provider::DataError;

#[cfg(doc)]
use crate::pattern::FixedCalendarDateTimeNames;
#[cfg(doc)]
use icu_calendar::types::YearInfo;
#[cfg(doc)]
use icu_decimal::DecimalFormatter;

/// An error from constructing a formatter.
#[derive(Display, Debug, Copy, Clone, PartialEq)]
#[non_exhaustive]
pub enum DateTimeFormatterLoadError {
    /// An error while loading display names for a field.
    #[displaydoc("{0}")]
    Names(PatternLoadError),
    /// An error while loading some other required data,
    /// such as skeleton patterns or calendar conversions.
    #[displaydoc("{0}")]
    Data(DataError),
}

impl core::error::Error for DateTimeFormatterLoadError {}

impl From<DataError> for DateTimeFormatterLoadError {
    fn from(error: DataError) -> Self {
        Self::Data(error)
    }
}

/// The specific field for which an error occurred.
#[derive(Display, Debug, Copy, Clone, PartialEq)]
pub struct ErrorField(pub(crate) crate::provider::fields::Field);

/// An error from mixing calendar types in a formatter.
#[derive(Display, Debug, Copy, Clone, PartialEq)]
#[displaydoc("DateTimeFormatter for {this_kind} calendar was given a {date_kind:?} calendar")]
#[non_exhaustive]
pub struct MismatchedCalendarError {
    /// The calendar kind of the target object (formatter).
    pub this_kind: AnyCalendarKind,
    /// The calendar kind of the input object (date being formatted).
    /// Can be `None` if the input calendar was not specified.
    pub date_kind: Option<AnyCalendarKind>,
}

impl core::error::Error for MismatchedCalendarError {}

#[non_exhaustive]
#[derive(Debug, PartialEq, Copy, Clone, displaydoc::Display)]
/// Error for `TryWriteable` implementations
pub enum DateTimeWriteError {
    /// The [`MonthCode`] of the input is not valid for this calendar.
    ///
    /// This is guaranteed not to happen for `icu::calendar` inputs, but may happen for custom inputs.
    ///
    /// The output will contain the raw [`MonthCode`] as a fallback value.
    #[displaydoc("Invalid month {0:?}")]
    InvalidMonthCode(MonthCode),
    /// The [`FormattingEra`] of the input is not valid for this calendar.
    ///
    /// This is guaranteed not to happen for `icu::calendar` inputs, but may happen for custom inputs.
    ///
    /// The output will contain [`FormattingEra::fallback_name`] as the fallback.
    #[displaydoc("Invalid era {0:?}")]
    InvalidEra(FormattingEra),
    /// The [`YearInfo::cyclic`] of the input is not valid for this calendar.
    ///
    /// This is guaranteed not to happen for `icu::calendar` inputs, but may happen for custom inputs.
    ///
    /// The output will contain [`YearInfo::extended_year`] as a fallback value.
    #[displaydoc("Invalid cyclic year {value} (maximum {max})")]
    InvalidCyclicYear {
        /// Value
        value: usize,
        /// Max
        max: usize,
    },

    /// The [`DecimalFormatter`] has not been loaded.
    ///
    /// This *only* happens if the formatter has been created using
    /// [`FixedCalendarDateTimeNames::with_pattern_unchecked`], the pattern requires decimal
    /// formatting, and the decimal formatter was not loaded.
    ///
    /// The output will contain fallback values using Latin numerals.
    #[displaydoc("DecimalFormatter not loaded")]
    DecimalFormatterNotLoaded,
    /// The localized names for a field have not been loaded.
    ///
    /// This *only* happens if the formatter has been created using
    /// [`FixedCalendarDateTimeNames::with_pattern_unchecked`], and the pattern requires names
    /// that were not loaded.
    ///
    /// The output will contain fallback values using field identifiers (such as `tue` for `Weekday::Tuesday`,
    /// `M02` for month 2, etc.).
    #[displaydoc("Names for {0:?} not loaded")]
    NamesNotLoaded(ErrorField),
    /// An input field (such as "hour" or "month") is missing.
    ///
    /// This *only* happens if the formatter has been created using
    /// [`FixedCalendarDateTimeNames::with_pattern_unchecked`], and the pattern requires fields
    /// that are not returned by the input type.
    ///
    /// The output will contain the string `{X}` instead, where `X` is the symbol for which the input is missing.
    #[displaydoc("Incomplete input, missing value for {0:?}")]
    MissingInputField(&'static str),
    /// The pattern contains a field that has a valid symbol but invalid length.
    ///
    /// This *only* happens if the formatter has been created using
    /// [`FixedCalendarDateTimeNames::with_pattern_unchecked`], and the pattern contains
    /// a field with a length not supported in formatting.
    ///
    /// The output will contain fallback values similar to [`DateTimeWriteError::NamesNotLoaded`].
    #[displaydoc("Field length for {0:?} is invalid")]
    UnsupportedLength(ErrorField),
    /// Unsupported field
    ///
    /// This *only* happens if the formatter has been created using
    /// [`FixedCalendarDateTimeNames::with_pattern_unchecked`], and the pattern contains unsupported fields.
    ///
    /// The output will contain the string `{unsupported:X}`, where `X` is the symbol of the unsupported field.
    #[displaydoc("Unsupported field {0:?}")]
    UnsupportedField(ErrorField),
}

impl core::error::Error for DateTimeWriteError {}
