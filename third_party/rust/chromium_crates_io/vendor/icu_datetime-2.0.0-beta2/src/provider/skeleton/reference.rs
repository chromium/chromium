// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Reference `Skeleton` implementation for parsing.

use super::error::SkeletonError;
use crate::provider::fields::{self, Field, FieldLength, FieldSymbol};
#[cfg(feature = "datagen")]
use crate::provider::pattern::reference::Pattern;
use alloc::vec::Vec;
use core::convert::TryFrom;
use smallvec::SmallVec;

/// A [`Skeleton`] is used to represent what types of `Field`s are present in a [`Pattern`]. The
/// ordering of the [`Skeleton`]'s `Field`s have no bearing on the ordering of the `Field`s and
/// `Literal`s in the [`Pattern`].
///
/// A [`Skeleton`] is a [`Vec`]`<Field>`, but with the invariant that it is sorted according to the canonical
/// sort order. This order is sorted according to the most significant `Field` to the least significant.
/// For example, a field with a `Minute` symbol would precede a field with a `Second` symbol.
/// This order is documented as the order of fields as presented in the
/// [UTS 35 Date Field Symbol Table](https://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table)
///
/// The `Field`s are only sorted in the [`Skeleton`] in order to provide a deterministic
/// serialization strategy, and to provide a faster [`Skeleton`] matching operation.
#[derive(Debug, Eq, PartialEq, Clone, Ord, PartialOrd)]
// TODO(#876): Use ZeroVec instead of SmallVec
pub struct Skeleton(pub(crate) SmallVec<[fields::Field; 5]>);

impl Skeleton {
    pub(crate) fn fields_iter(&self) -> impl Iterator<Item = &Field> {
        self.0.iter()
    }

    pub(crate) fn fields_len(&self) -> usize {
        self.0.len()
    }

    /// Return the underlying fields as a slice.
    pub fn as_slice(&self) -> &[fields::Field] {
        self.0.as_slice()
    }
}

impl From<SmallVec<[fields::Field; 5]>> for Skeleton {
    fn from(fields: SmallVec<[fields::Field; 5]>) -> Self {
        Self(fields)
    }
}

impl From<Vec<fields::Field>> for Skeleton {
    fn from(fields: Vec<fields::Field>) -> Self {
        Self(fields.into())
    }
}

impl From<&[fields::Field]> for Skeleton {
    fn from(fields: &[fields::Field]) -> Self {
        Self(fields.into())
    }
}

/// Convert a Pattern into a Skeleton. This will remove all of the string literals, and sort
/// the fields into the canonical sort order. Not all fields are supported by Skeletons, so map
/// fields into skeleton-appropriate ones. For instance, in the "ja" locale the pattern "aK:mm"
/// gets transformed into the skeleton "hmm".
///
/// At the time of this writing, it's being used for applying hour cycle preferences and should not
/// be exposed as a public API for end users.
#[doc(hidden)]
#[cfg(feature = "datagen")]
impl From<&Pattern> for Skeleton {
    fn from(pattern: &Pattern) -> Self {
        let mut fields: SmallVec<[fields::Field; 5]> = SmallVec::new();
        for item in pattern.items() {
            if let crate::provider::pattern::PatternItem::Field(field) = item {
                let mut field = *field;

                // Skeletons only have a subset of available fields, these are then mapped to more
                // specific fields for the patterns they expand to.
                field.symbol = match field.symbol {
                    // Only the format varieties are used in the skeletons, the matched patterns
                    // will be more specific.
                    FieldSymbol::Month(_) => FieldSymbol::Month(fields::Month::Format),
                    FieldSymbol::Weekday(_) => FieldSymbol::Weekday(fields::Weekday::Format),

                    // Only flexible day periods are used in skeletons, ignore all others.
                    FieldSymbol::DayPeriod(fields::DayPeriod::AmPm)
                    | FieldSymbol::DayPeriod(fields::DayPeriod::NoonMidnight) => continue,
                    // TODO(#487) - Flexible day periods should be included here.
                    // FieldSymbol::DayPeriod(fields::DayPeriod::Flexible) => {
                    //     FieldSymbol::DayPeriod(fields::DayPeriod::Flexible)
                    // }

                    // Only the H12 and H23 symbols are used in skeletons, while the patterns may
                    // contain H11 or H23 depending on the localization.
                    FieldSymbol::Hour(fields::Hour::H11) | FieldSymbol::Hour(fields::Hour::H12) => {
                        FieldSymbol::Hour(fields::Hour::H12)
                    }
                    FieldSymbol::Hour(fields::Hour::H23) | FieldSymbol::Hour(fields::Hour::H24) => {
                        FieldSymbol::Hour(fields::Hour::H23)
                    }

                    // Pass through all of the following preferences unchanged.
                    FieldSymbol::Minute
                    | FieldSymbol::Second(_)
                    | FieldSymbol::TimeZone(_)
                    | FieldSymbol::DecimalSecond(_)
                    | FieldSymbol::Era
                    | FieldSymbol::Year(_)
                    | FieldSymbol::Week(_)
                    | FieldSymbol::Day(_) => field.symbol,
                };

                // Only insert if it's a unique field.
                if let Err(pos) = fields.binary_search(&field) {
                    fields.insert(pos, field)
                }
            }
        }
        Self(fields)
    }
}

/// Parse a string into a list of fields. This trait implementation validates the input string to
/// verify that fields are correct. If the fields are out of order, this returns an error that
/// contains the fields, which gives the callee a chance to sort the fields with the
/// `From<SmallVec<[fields::Field; 5]>> for Skeleton` trait.
impl TryFrom<&str> for Skeleton {
    type Error = SkeletonError;
    fn try_from(skeleton_string: &str) -> Result<Self, Self::Error> {
        let mut fields: SmallVec<[fields::Field; 5]> = SmallVec::new();

        let mut iter = skeleton_string.chars().peekable();
        while let Some(ch) = iter.next() {
            // Go through the chars to count how often it's repeated.
            let mut field_length: u8 = 1;
            while let Some(next_ch) = iter.peek() {
                if *next_ch != ch {
                    break;
                }
                field_length += 1;
                iter.next();
            }

            // Convert the byte to a valid field symbol.
            let field_symbol = if ch == 'Z' {
                match field_length {
                    1..=3 => {
                        field_length = 4;
                        FieldSymbol::try_from('x')?
                    }
                    4 => FieldSymbol::try_from('O')?,
                    5 => {
                        field_length = 4;
                        FieldSymbol::try_from('X')?
                    }
                    _ => FieldSymbol::try_from(ch)?,
                }
            } else {
                FieldSymbol::try_from(ch)?
            };
            let field = Field::from((field_symbol, FieldLength::from_idx(field_length)?));

            match fields.binary_search(&field) {
                Ok(_) => return Err(SkeletonError::DuplicateField),
                Err(pos) => fields.insert(pos, field),
            }
        }

        Ok(Self::from(fields))
    }
}

#[cfg(feature = "datagen")]
impl core::fmt::Display for Skeleton {
    fn fmt(&self, formatter: &mut core::fmt::Formatter) -> core::fmt::Result {
        use core::fmt::Write;
        for field in self.fields_iter() {
            let ch: char = field.symbol.into();
            for _ in 0..field.length.to_len() {
                formatter.write_char(ch)?;
            }
        }
        Ok(())
    }
}
