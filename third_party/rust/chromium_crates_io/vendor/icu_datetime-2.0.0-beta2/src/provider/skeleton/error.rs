// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::provider::fields;
use displaydoc::Display;

/// These strings follow the recommendations for the serde::de::Unexpected::Other type.
/// <https://docs.serde.rs/serde/de/enum.Unexpected.html#variant.Other>
///
/// Serde will generate an error such as:
/// "invalid value: unclosed literal in pattern, expected a valid UTS 35 pattern string at line 1 column 12"
#[derive(Display, Debug, Copy, Clone, PartialEq)]
#[allow(missing_docs)]
#[non_exhaustive]
pub enum SkeletonError {
    #[displaydoc("field too long in skeleton")]
    InvalidFieldLength,
    #[displaydoc("duplicate field in skeleton")]
    DuplicateField,
    #[displaydoc("symbol unknown {0} in skeleton")]
    SymbolUnknown(char),
    #[displaydoc("symbol invalid {0} in skeleton")]
    SymbolInvalid(u8),
    #[displaydoc("symbol unimplemented {0} in skeleton")]
    SymbolUnimplemented(char),
    #[displaydoc("unimplemented field {0} in skeleton")]
    UnimplementedField(char),
    #[displaydoc("skeleton has a variant subtag")]
    SkeletonHasVariant,
    #[displaydoc("{0}")]
    Fields(fields::Error),
}

impl core::error::Error for SkeletonError {}

impl From<fields::Error> for SkeletonError {
    fn from(e: fields::Error) -> Self {
        SkeletonError::Fields(e)
    }
}

impl From<fields::LengthError> for SkeletonError {
    fn from(_: fields::LengthError) -> Self {
        Self::InvalidFieldLength
    }
}

impl From<fields::SymbolError> for SkeletonError {
    fn from(symbol_error: fields::SymbolError) -> Self {
        match symbol_error {
            fields::SymbolError::Invalid(ch) => match ch {
                b'-' => Self::SkeletonHasVariant,
                _ => Self::SymbolInvalid(ch),
            },
            fields::SymbolError::InvalidIndex(_) => unimplemented!(),
            fields::SymbolError::Unknown(ch) => {
                // NOTE: If you remove a symbol due to it now being supported,
                //       make sure to regenerate data: cargo make bakeddata components/datetime.
                match ch {
                    // TODO(#487) - Flexible day periods
                    'B'
                    // TODO(#501) - Quarters
                    | 'Q' | 'q'
                    // Extended year
                    | 'u'
                    // TODO(#5643) - Weeks
                    | 'Y' | 'w' | 'W'
                    // Modified Julian Day
                    | 'g'
                    => Self::SymbolUnimplemented(ch),
                    _ => Self::SymbolUnknown(ch),
                }
            }
        }
    }
}
