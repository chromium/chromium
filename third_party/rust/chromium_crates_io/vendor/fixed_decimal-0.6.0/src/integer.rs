// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::convert::TryFrom;
use core::fmt;

use core::str::FromStr;

use crate::FixedDecimal;
use crate::LimitError;
use crate::ParseError;

/// A [`FixedInteger`] is a [`FixedDecimal`] with no fractional part.
///
///
/// # Examples
///
/// ```
/// # use std::str::FromStr;
/// use fixed_decimal::FixedDecimal;
/// use fixed_decimal::FixedInteger;
/// use fixed_decimal::LimitError;
///
/// assert_eq!(
///     FixedDecimal::from(FixedInteger::from(5)),
///     FixedDecimal::from(5)
/// );
/// assert_eq!(
///     FixedInteger::try_from(FixedDecimal::from(5)),
///     Ok(FixedInteger::from(5))
/// );
/// assert_eq!(
///     FixedInteger::try_from(FixedDecimal::from_str("05").unwrap()),
///     Ok(FixedInteger::from_str("05").unwrap())
/// );
/// assert_eq!(
///     FixedInteger::try_from(FixedDecimal::from_str("5.0").unwrap()),
///     Err(LimitError)
/// );
/// ```
#[derive(Debug, Clone, PartialEq, Default)]
pub struct FixedInteger(FixedDecimal);

impl From<FixedInteger> for FixedDecimal {
    fn from(value: FixedInteger) -> Self {
        value.0
    }
}

macro_rules! impl_fixed_integer_from_integer_type {
    ($type:ident) => {
        impl From<$type> for FixedInteger {
            fn from(value: $type) -> Self {
                FixedInteger(FixedDecimal::from(value))
            }
        }
    };
}

impl_fixed_integer_from_integer_type!(isize);
impl_fixed_integer_from_integer_type!(i128);
impl_fixed_integer_from_integer_type!(i64);
impl_fixed_integer_from_integer_type!(i32);
impl_fixed_integer_from_integer_type!(i16);
impl_fixed_integer_from_integer_type!(i8);
impl_fixed_integer_from_integer_type!(usize);
impl_fixed_integer_from_integer_type!(u128);
impl_fixed_integer_from_integer_type!(u64);
impl_fixed_integer_from_integer_type!(u32);
impl_fixed_integer_from_integer_type!(u16);
impl_fixed_integer_from_integer_type!(u8);

impl writeable::Writeable for FixedInteger {
    fn write_to<W: fmt::Write + ?Sized>(&self, sink: &mut W) -> fmt::Result {
        self.0.write_to(sink)
    }
}

impl TryFrom<FixedDecimal> for FixedInteger {
    type Error = LimitError;
    fn try_from(value: FixedDecimal) -> Result<Self, Self::Error> {
        if value.magnitude_range().start() != &0 {
            Err(LimitError)
        } else {
            Ok(FixedInteger(value))
        }
    }
}

impl FixedInteger {
    #[inline]
    pub fn try_from_str(s: &str) -> Result<Self, ParseError> {
        Self::try_from_utf8(s.as_bytes())
    }

    pub fn try_from_utf8(code_units: &[u8]) -> Result<Self, ParseError> {
        FixedInteger::try_from(FixedDecimal::try_from_utf8(code_units)?)
            .map_err(|LimitError| ParseError::Limit)
    }
}

impl FromStr for FixedInteger {
    type Err = ParseError;
    #[inline]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(s)
    }
}
