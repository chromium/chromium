// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::cmp::{Ord, PartialOrd};
use core::fmt;
use displaydoc::Display;
use zerovec::ule::{AsULE, UleError, ULE};

/// An error relating to the length of a field within a date pattern.
#[derive(Display, Debug, PartialEq, Copy, Clone)]
#[non_exhaustive]
pub enum LengthError {
    /// The length of the field string within the pattern is invalid, according to
    /// the field type and its supported field patterns in LDML. See [`FieldLength`].
    #[displaydoc("Invalid length")]
    InvalidLength,
}

#[cfg(feature = "std")]
impl std::error::Error for LengthError {}

/// An enum representing the length of a field within a date or time formatting pattern string.
///
/// Such strings represent fields as a letter occurring 1 or more times in a row, ex:
/// `MMM`, `dd`, `y`.  See the
/// [LDML documentation in UTS 35](https://unicode.org/reports/tr35/tr35-dates.html#Date_Format_Patterns)
/// for more details.
#[derive(Debug, Eq, PartialEq, Clone, Copy, Ord, PartialOrd)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::fields))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[allow(clippy::exhaustive_enums)] // part of data struct
pub enum FieldLength {
    /// Numeric: minimum digits
    ///
    /// Text: same as [`Self::Three`]
    One,
    /// Numeric: pad to 2 digits
    ///
    /// Text: same as [`Self::Three`]
    Two,
    /// Numeric: pad to 3 digits
    ///
    /// Text: Abbreviated format.
    Three,
    /// Numeric: pad to 4 digits
    ///
    /// Text: Wide format.
    Four,
    /// Numeric: pad to 5 digits
    ///
    /// Text: Narrow format.
    Five,
    /// Numeric: pad to 6 digits
    ///
    /// Text: Short format.
    Six,
    /// FieldLength::One (numeric), but overridden with a different numbering system
    NumericOverride(FieldNumericOverrides),
}

/// First index used for numeric overrides in compact FieldLength representation
///
/// Currently 17 due to decision in <https://unicode-org.atlassian.net/browse/CLDR-17217>,
/// may become 16 if the `> 16` is updated to a ` >= 16`
const FIRST_NUMERIC_OVERRIDE: u8 = 17;
/// Last index used for numeric overrides
const LAST_NUMERIC_OVERRIDE: u8 = 31;

impl FieldLength {
    #[inline]
    pub(crate) fn idx(&self) -> u8 {
        match self {
            FieldLength::One => 1,
            FieldLength::Two => 2,
            FieldLength::Three => 3,
            FieldLength::Four => 4,
            FieldLength::Five => 5,
            FieldLength::Six => 6,
            FieldLength::NumericOverride(o) => FIRST_NUMERIC_OVERRIDE
                .saturating_add(*o as u8)
                .min(LAST_NUMERIC_OVERRIDE),
        }
    }

    #[inline]
    pub(crate) fn from_idx(idx: u8) -> Result<Self, LengthError> {
        Ok(match idx {
            1 => Self::One,
            2 => Self::Two,
            3 => Self::Three,
            4 => Self::Four,
            5 => Self::Five,
            6 => Self::Six,
            idx if (FIRST_NUMERIC_OVERRIDE..=LAST_NUMERIC_OVERRIDE).contains(&idx) => {
                Self::NumericOverride((idx - FIRST_NUMERIC_OVERRIDE).try_into()?)
            }
            _ => return Err(LengthError::InvalidLength),
        })
    }

    #[inline]
    pub(crate) fn to_len(self) -> usize {
        match self {
            FieldLength::One => 1,
            FieldLength::Two => 2,
            FieldLength::Three => 3,
            FieldLength::Four => 4,
            FieldLength::Five => 5,
            FieldLength::Six => 6,
            FieldLength::NumericOverride(o) => FIRST_NUMERIC_OVERRIDE as usize + o as usize,
        }
    }

    /// UTS 35 defines several 1 and 2 symbols to be the same as 3 symbols (abbreviated).
    /// For example, 'a' represents an abbreviated day period, the same as 'aaa'.
    ///
    /// This function maps field lengths 1 and 2 to field length 3.
    pub(crate) fn numeric_to_abbr(self) -> Self {
        match self {
            FieldLength::One | FieldLength::Two => FieldLength::Three,
            other => other,
        }
    }
}

#[repr(transparent)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct FieldLengthULE(u8);

impl AsULE for FieldLength {
    type ULE = FieldLengthULE;
    fn to_unaligned(self) -> Self::ULE {
        FieldLengthULE(self.idx())
    }
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        #[allow(clippy::unwrap_used)] // OK because the ULE is pre-validated
        Self::from_idx(unaligned.0).unwrap()
    }
}

impl FieldLengthULE {
    #[inline]
    pub(crate) fn validate_byte(byte: u8) -> Result<(), UleError> {
        FieldLength::from_idx(byte)
            .map(|_| ())
            .map_err(|_| UleError::parse::<FieldLength>())
    }
}

// Safety checklist for ULE:
//
// 1. Must not include any uninitialized or padding bytes (true since transparent over a ULE).
// 2. Must have an alignment of 1 byte (true since transparent over a ULE).
// 3. ULE::validate_bytes() checks that the given byte slice represents a valid slice.
// 4. ULE::validate_bytes() checks that the given byte slice has a valid length
//    (true since transparent over a type of size 1).
// 5. All other methods must be left with their default impl.
// 6. Byte equality is semantic equality.
unsafe impl ULE for FieldLengthULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        for byte in bytes {
            Self::validate_byte(*byte)?;
        }
        Ok(())
    }
}

/// Various numeric overrides for datetime patterns
/// as found in CLDR
#[derive(Debug, Eq, PartialEq, Clone, Copy, Ord, PartialOrd)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::fields))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[non_exhaustive]
pub enum FieldNumericOverrides {
    /// `hanidec`
    Hanidec = 0,
    /// `hanidays`
    Hanidays = 1,
    /// `hebr`
    Hebr = 2,
    /// `romanlow`
    Romanlow = 3,
    /// `jpnyear`
    Jpnyear = 4,
}

impl TryFrom<u8> for FieldNumericOverrides {
    type Error = LengthError;
    fn try_from(other: u8) -> Result<Self, LengthError> {
        Ok(match other {
            0 => Self::Hanidec,
            1 => Self::Hanidays,
            2 => Self::Hebr,
            3 => Self::Romanlow,
            4 => Self::Jpnyear,
            _ => return Err(LengthError::InvalidLength),
        })
    }
}

impl FieldNumericOverrides {
    /// Convert this to the corresponding string code
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Hanidec => "hanidec",
            Self::Hanidays => "hanidays",
            Self::Hebr => "hebr",
            Self::Romanlow => "romanlow",
            Self::Jpnyear => "jpnyear",
        }
    }
}

impl fmt::Display for FieldNumericOverrides {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.as_str().fmt(f)
    }
}
