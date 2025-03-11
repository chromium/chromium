// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::provider::fields;
use displaydoc::Display;

/// A low-level pattern parsing error.
///
/// These strings follow the recommendations for the serde::de::Unexpected::Other type.
/// <https://docs.serde.rs/serde/de/enum.Unexpected.html#variant.Other>
///
/// Serde will generate an error such as:
/// "invalid value: unclosed literal in pattern, expected a valid UTS 35 pattern string at line 1 column 12"
#[derive(Display, Debug, PartialEq, Copy, Clone)]
#[allow(missing_docs)]
#[non_exhaustive]
pub enum PatternError {
    #[displaydoc("{0:?} invalid field length in pattern")]
    FieldLengthInvalid(fields::FieldSymbol),
    #[displaydoc("unknown substitution {0} in pattern")]
    UnknownSubstitution(char),
    #[displaydoc("invalid symbol {0} in pattern")]
    InvalidSymbol(char),
    #[displaydoc("unclosed literal in pattern")]
    UnclosedLiteral,
    #[displaydoc("unclosed placeholder in pattern")]
    UnclosedPlaceholder,
    #[displaydoc("plural pattern variants are only supported for week-of-month and week-of-year")]
    UnsupportedPluralPivot,
}

impl core::error::Error for PatternError {}

impl From<fields::Error> for PatternError {
    fn from(input: fields::Error) -> Self {
        match input {
            fields::Error::InvalidLength(symbol) => Self::FieldLengthInvalid(symbol),
        }
    }
}
