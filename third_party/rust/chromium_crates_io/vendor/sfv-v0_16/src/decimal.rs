use std::fmt;

use crate::{Error, Integer, error};

/// A structured field value [decimal].
///
/// Decimals have 12 digits of integer precision and 3 digits of fractional precision.
///
/// [decimal]: <https://httpwg.org/specs/rfc9651.html#decimal>
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub struct Decimal(Integer);

impl Decimal {
    /// The minimum value for a parsed or serialized decimal: `-999_999_999_999.999`.
    pub const MIN: Self = Self::from_integer_scaled_1000(Integer::MIN);

    /// The maximum value for a parsed or serialized decimal: `999_999_999_999.999`.
    pub const MAX: Self = Self::from_integer_scaled_1000(Integer::MAX);

    /// `0.0`.
    pub const ZERO: Self = Self(Integer::ZERO);

    /// Returns the decimal as an integer multiplied by 1000.
    ///
    /// The conversion is guaranteed to be precise.
    ///
    /// # Example
    ///
    /// ```
    /// let decimal = sfv::Decimal::try_from(1.234).unwrap();
    /// assert_eq!(i64::from(decimal.as_integer_scaled_1000()), 1234);
    /// ```
    #[must_use]
    pub fn as_integer_scaled_1000(&self) -> Integer {
        self.0
    }

    /// Creates a decimal from an integer multiplied by 1000.
    ///
    /// The conversion is guaranteed to be precise.
    ///
    /// # Example
    ///
    /// ```
    /// let decimal = sfv::Decimal::from_integer_scaled_1000(sfv::integer(1234));
    /// #[allow(clippy::float_cmp)]
    /// assert_eq!(f64::from(decimal), 1.234);
    /// ```
    #[must_use]
    pub const fn from_integer_scaled_1000(v: Integer) -> Self {
        Self(v)
    }
}

impl fmt::Display for Decimal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let v = i64::from(self.as_integer_scaled_1000());

        if v == 0 {
            return f.write_str("0.0");
        }

        let sign = if v < 0 { "-" } else { "" };
        let v = v.abs();
        let i_part = v / 1000;
        let f_part = v % 1000;

        if f_part % 100 == 0 {
            write!(f, "{}{}.{}", sign, i_part, f_part / 100)
        } else if f_part % 10 == 0 {
            write!(f, "{}{}.{:02}", sign, i_part, f_part / 10)
        } else {
            write!(f, "{sign}{i_part}.{f_part:03}")
        }
    }
}

macro_rules! impl_from_integer {
    ($($from: ty,)+) => {
        $(
            impl From<$from> for Decimal {
                fn from(v: $from) -> Decimal {
                    Self(Integer::from_validated_i64(i64::from(v) * 1000))
                }
            }
        )+
    }
}

impl_from_integer! {
    i8,
    i16,
    i32,
    u8,
    u16,
    u32,
}

macro_rules! impl_try_from_integer {
    ($($from: ty,)+) => {
        $(
            impl TryFrom<$from> for Decimal {
                type Error = Error;

                fn try_from(v: $from) -> Result<Decimal, Error> {
                    match v.checked_mul(1000) {
                        None => Err(error::Repr::OutOfRange.into()),
                        Some(v) => Integer::try_from(v).map(Decimal),
                    }
                }
            }
        )+
    }
}

impl_try_from_integer! {
    i64,
    i128,
    isize,
    u64,
    u128,
    usize,
}

impl From<Decimal> for f64 {
    #[allow(clippy::cast_precision_loss)]
    fn from(v: Decimal) -> Self {
        let v = i64::from(v.as_integer_scaled_1000());
        (v as f64) / 1000.0
    }
}

/// Prefer using [`Decimal::from_integer_scaled_1000`] for precise conversions.
impl TryFrom<f32> for Decimal {
    type Error = Error;

    fn try_from(v: f32) -> Result<Decimal, Self::Error> {
        Self::try_from(f64::from(v))
    }
}

/// Prefer using [`Decimal::from_integer_scaled_1000`] for precise conversions.
impl TryFrom<f64> for Decimal {
    type Error = Error;

    #[allow(clippy::float_cmp)]
    fn try_from(v: f64) -> Result<Decimal, Error> {
        if v.is_nan() {
            return Err(error::Repr::NaN.into());
        }

        // Exact decimal ties like 0.5015 and 2.0005 are not exact in binary
        // floating point, so `v * 1000.0` can land 1 ULP below or above the
        // half-integer midpoint (e.g. 501.49999999999994 or 2000.5000000000002).
        // For all in-range decimals (`|v| < 10^12`), the candidate half-integer
        // `midpoint = N + 0.5` and `1000.0` are both exact in `f64`, so IEEE 754
        // division `midpoint / 1000.0` produces the exact same `f64` as the
        // decimal tie `(N + 0.5) / 1000`. If it matches `v`, snap to `midpoint`
        // before rounding ties to even.
        // See https://github.com/httpwg/structured-field-tests/issues/113.
        let scaled = v * 1000.0;
        let midpoint = scaled.floor() + 0.5;
        let v = if midpoint / 1000.0 == v {
            midpoint
        } else {
            scaled
        }
        .round_ties_even();
        // Only excessively clever options exist for this conversion, so use "as"
        // Note that this relies on saturating casts for values > i64::MAX
        // See https://github.com/rust-lang/rust/issues/10184
        #[allow(clippy::cast_possible_truncation)]
        Integer::try_from(v as i64).map(Decimal)
    }
}

impl TryFrom<Integer> for Decimal {
    type Error = Error;

    fn try_from(v: Integer) -> Result<Decimal, Error> {
        i64::from(v).try_into()
    }
}
