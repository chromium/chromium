// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::convert::TryFrom;

use core::str::FromStr;

use crate::Decimal;
use crate::LimitError;
use crate::ParseError;

/// A [`FixedInteger`] is a [`Decimal`] with no fractional part.
///
///
/// # Examples
///
/// ```
/// # use std::str::FromStr;
/// use fixed_decimal::Decimal;
/// use fixed_decimal::FixedInteger;
/// use fixed_decimal::LimitError;
///
/// assert_eq!(Decimal::from(FixedInteger::from(5)), Decimal::from(5));
/// assert_eq!(
///     FixedInteger::try_from(Decimal::from(5)),
///     Ok(FixedInteger::from(5))
/// );
/// assert_eq!(
///     FixedInteger::try_from(Decimal::from_str("05").unwrap()),
///     Ok(FixedInteger::from_str("05").unwrap())
/// );
/// assert_eq!(
///     FixedInteger::try_from(Decimal::from_str("5.0").unwrap()),
///     Err(LimitError)
/// );
/// ```
#[derive(Debug, Clone, PartialEq, Default)]
pub struct FixedInteger(Decimal);

impl From<FixedInteger> for Decimal {
    fn from(value: FixedInteger) -> Self {
        value.0
    }
}

macro_rules! impl_fixed_integer_from_integer_type {
    ($type:ident) => {
        impl From<$type> for FixedInteger {
            fn from(value: $type) -> Self {
                FixedInteger(Decimal::from(value))
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

writeable::impl_writeable_delegate!(FixedInteger, |&self| &self.0);

impl TryFrom<Decimal> for FixedInteger {
    type Error = LimitError;
    fn try_from(signed_fd: Decimal) -> Result<Self, Self::Error> {
        if signed_fd.absolute.magnitude_range().start() != &0 {
            Err(LimitError)
        } else {
            Ok(FixedInteger(signed_fd))
        }
    }
}

impl FixedInteger {
    #[inline]
    /// Parses a [`FixedInteger`]
    pub fn try_from_str(s: &str) -> Result<Self, ParseError> {
        Self::try_from_utf8(s.as_bytes())
    }

    /// See [`Self::try_from_utf8`]
    pub fn try_from_utf8(code_units: &[u8]) -> Result<Self, ParseError> {
        FixedInteger::try_from(Decimal::try_from_utf8(code_units)?)
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

#[test]
fn test_from_str_scientific_bounds() {
    assert_eq!(
        Decimal::from(FixedInteger::from_str("0e5").unwrap()).to_string(),
        "0"
    );
    assert_eq!(FixedInteger::from_str("1e99999"), Err(ParseError::Limit));
    assert_eq!(FixedInteger::from_str("1e-99999"), Err(ParseError::Limit));
    assert_eq!(
        FixedInteger::from_str("1e999999999999999999999999999999"),
        Err(ParseError::Limit)
    );
    assert_eq!(
        FixedInteger::from_str("1e-999999999999999999999999999999"),
        Err(ParseError::Limit)
    );
    assert_eq!(FixedInteger::from_str("10e32767"), Err(ParseError::Limit));
    assert_eq!(FixedInteger::from_str("1e-"), Err(ParseError::Syntax));
    assert_eq!(FixedInteger::from_str("0e-"), Err(ParseError::Syntax));
    assert!(FixedInteger::from_str("1e32767").is_ok());
}
