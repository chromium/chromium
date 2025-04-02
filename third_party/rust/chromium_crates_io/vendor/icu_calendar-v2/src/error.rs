// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::types::{Era, MonthCode};
use displaydoc::Display;

#[derive(Debug, Copy, Clone, PartialEq, Display)]
/// Error type for date creation.
#[non_exhaustive]
pub enum DateError {
    /// A field is out of range for its domain.
    #[displaydoc("The {field} = {value} argument is out of range {min}..={max}")]
    Range {
        /// The field that is out of range, such as "year"
        field: &'static str,
        /// The actual value
        value: i32,
        /// The minimum value (inclusive). This might not be tight.
        min: i32,
        /// The maximum value (inclusive). This might not be tight.
        max: i32,
    },
    /// Unknown era
    #[displaydoc("Unknown era {0:?}")]
    UnknownEra(Era),
    /// Unknown month code
    #[displaydoc("Unknown month code {0:?}")]
    UnknownMonthCode(MonthCode),
}

impl core::error::Error for DateError {}

#[derive(Debug, Copy, Clone, PartialEq, Display)]
/// An argument is out of range for its domain.
#[displaydoc("The {field} = {value} argument is out of range {min}..={max}")]
#[allow(clippy::exhaustive_structs)]
pub struct RangeError {
    /// The argument that is out of range, such as "year"
    pub field: &'static str,
    /// The actual value
    pub value: i32,
    /// The minimum value (inclusive). This might not be tight.
    pub min: i32,
    /// The maximum value (inclusive). This might not be tight.
    pub max: i32,
}

impl core::error::Error for RangeError {}

impl From<RangeError> for DateError {
    fn from(value: RangeError) -> Self {
        let RangeError {
            field,
            value,
            min,
            max,
        } = value;
        DateError::Range {
            field,
            value,
            min,
            max,
        }
    }
}
