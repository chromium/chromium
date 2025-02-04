// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use smallvec::SmallVec;

use core::cmp;
use core::cmp::Ordering;
use core::fmt;
use core::ops::RangeInclusive;
use core::str::FromStr;

use crate::uint_iterator::IntIterator;

#[cfg(feature = "ryu")]
use crate::LimitError;
use crate::ParseError;

// FixedDecimal assumes usize (digits.len()) is at least as big as a u16
#[cfg(not(any(
    target_pointer_width = "16",
    target_pointer_width = "32",
    target_pointer_width = "64"
)))]
compile_error!("The fixed_decimal crate only works if usizes are at least the size of a u16");

/// A struct containing decimal digits with efficient iteration and manipulation by magnitude
/// (power of 10).
///
/// Supports a mantissa of non-zero digits and a number of leading and trailing
/// zeros, as well as an optional sign; used for formatting and plural selection.
///
/// # Data Types
///
/// The following types can be converted to a `FixedDecimal`:
///
/// - Integers, signed and unsigned
/// - Strings representing an arbitrary-precision decimal
/// - Floating point values (using the `ryu` feature)
///
/// To create a [`FixedDecimal`] with fraction digits, either create it from an integer and then
/// call [`FixedDecimal::multiplied_pow10`], create it from a string, or (when the `ryu` feature is
/// enabled) create it from a floating point value using [`FixedDecimal::try_from_f64`].
///
/// # Magnitude and Position
///
/// Each digit in a `FixedDecimal` is indexed by a *magnitude*, or the digit's power of 10.
/// Illustration for the number "12.34":
///
/// | Magnitude | Digit | Description      |
/// |-----------|-------|------------------|
/// | 1         | 1     | Tens place       |
/// | 0         | 2     | Ones place       |
/// | -1        | 3     | Tenths place     |
/// | -2        | 4     | Hundredths place |
///
/// Some functions deal with a *position* for the purpose of padding, truncating, or rounding a
/// number. In these cases, the position sits between the corresponding magnitude of that position
/// and the next lower significant digit.
/// Illustration:
///
/// ```text
/// Position:   2   0  -2
/// Number:     |1|2.3|4|
/// Position:     1  -1
/// ```
///
/// Expected output of various operations, all with input "12.34":
///
/// | Operation                | Position  | Expected Result |
/// |--------------------------|-----------|-----------------|
/// | Truncate to tens         |         1 |   10            |
/// | Truncate to tenths       |        -1 |   12.3          |
/// | Pad to ten thousands     |         4 | 0012.34         |
/// | Pad to ten thousandths   |        -4 |   12.3400       |
///
/// # Examples
///
/// ```
/// use fixed_decimal::FixedDecimal;
///
/// let mut dec = FixedDecimal::from(250);
/// assert_eq!("250", dec.to_string());
///
/// dec.multiply_pow10(-2);
/// assert_eq!("2.50", dec.to_string());
/// ```
#[derive(Debug, Clone, PartialEq)]
pub struct FixedDecimal {
    /// List of digits; digits\[0\] is the most significant.
    ///
    /// Invariants:
    /// - Must not include leading or trailing zeros
    /// - Length must not exceed (magnitude - lower_magnitude + 1)
    // TODO: Consider using a nibble array
    digits: SmallVec<[u8; 8]>,

    /// Power of 10 of digits\[0\].
    ///
    /// Invariants:
    /// - <= upper_magnitude
    /// - >= lower_magnitude
    magnitude: i16,

    /// Power of 10 of the most significant digit, which may be zero.
    ///
    /// Invariants:
    /// - >= 0
    /// - >= magnitude
    upper_magnitude: i16,

    /// Power of 10 of the least significant digit, which may be zero.
    ///
    /// Invariants:
    /// - <= 0
    /// - <= magnitude
    lower_magnitude: i16,

    /// The sign; note that a positive value may be represented by either
    /// `Sign::Positive` (corresponding to a prefix +) or `Sign::None`
    /// (corresponding to the absence of a prefix sign).
    sign: Sign,
}

/// A specification of the sign used when formatting a number.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
#[allow(clippy::exhaustive_enums)]
// There are only 3 sign values, and they correspond to the low-level data model of FixedDecimal and UTS 35.
pub enum Sign {
    /// No sign (implicitly positive, e.g., 1729).
    None,
    /// A negative sign, e.g., -1729.
    Negative,
    /// An explicit positive sign, e.g., +1729.
    Positive,
}

/// Configuration for when to render the minus sign or plus sign.
#[non_exhaustive]
#[derive(Debug, Eq, PartialEq, Clone, Copy)]
pub enum SignDisplay {
    /// Render the sign according to locale preferences. In most cases, this means a minus sign
    /// will be shown on negative numbers, and no sign will be shown on positive numbers.
    Auto,

    /// Do not display the sign. Positive and negative numbers are indistinguishable.
    Never,

    /// Show a minus sign on negative numbers and a plus sign on positive numbers, including zero.
    Always,

    /// Show a minus sign on negative numbers and a plus sign on positive numbers, except do not
    /// show any sign on positive or negative zero.
    ExceptZero,

    /// Show a minus sign on strictly negative numbers. Do not show a sign on positive numbers or
    /// on positive or negative zero.
    ///
    /// This differs from [`Auto`](SignDisplay::Auto) in that it does not render a sign on negative zero.
    Negative,
}

impl Default for FixedDecimal {
    /// Returns a `FixedDecimal` representing zero.
    fn default() -> Self {
        Self {
            digits: SmallVec::new(),
            magnitude: 0,
            upper_magnitude: 0,
            lower_magnitude: 0,
            sign: Sign::None,
        }
    }
}

macro_rules! impl_from_signed_integer_type {
    ($itype:ident, $utype: ident) => {
        impl From<$itype> for FixedDecimal {
            fn from(value: $itype) -> Self {
                let int_iterator: IntIterator<$utype> = value.into();
                let sign = if int_iterator.is_negative {
                    Sign::Negative
                } else {
                    Sign::None
                };
                let mut result = Self::from_ascending(int_iterator)
                    .expect("All built-in integer types should fit");
                result.sign = sign;
                result
            }
        }
    };
}

macro_rules! impl_from_unsigned_integer_type {
    ($utype: ident) => {
        impl From<$utype> for FixedDecimal {
            fn from(value: $utype) -> Self {
                let int_iterator: IntIterator<$utype> = value.into();
                Self::from_ascending(int_iterator).expect("All built-in integer types should fit")
            }
        }
    };
}

impl_from_signed_integer_type!(isize, usize);
impl_from_signed_integer_type!(i128, u128);
impl_from_signed_integer_type!(i64, u64);
impl_from_signed_integer_type!(i32, u32);
impl_from_signed_integer_type!(i16, u16);
impl_from_signed_integer_type!(i8, u8);

impl_from_unsigned_integer_type!(usize);
impl_from_unsigned_integer_type!(u128);
impl_from_unsigned_integer_type!(u64);
impl_from_unsigned_integer_type!(u32);
impl_from_unsigned_integer_type!(u16);
impl_from_unsigned_integer_type!(u8);

/// Mode used in a rounding operation.
///
/// # Comparative table of rounding modes
///
/// | Value | Ceil | Expand | Floor | Trunc | HalfCeil | HalfExpand | HalfFloor | HalfTrunc | HalfEven |
/// |:-----:|:----:|:------:|:-----:|:-----:|:--------:|:----------:|:---------:|:---------:|:--------:|
/// |  +1.8 |  +2  |   +2   |   +1  |   +1  |    +2    |     +2     |     +2    |     +2    |    +2    |
/// |  +1.5 |   "  |    "   |   "   |   "   |     "    |      "     |     +1    |     +1    |     "    |
/// |  +1.2 |   "  |    "   |   "   |   "   |    +1    |     +1     |     "     |     "     |    +1    |
/// |  +0.8 |  +1  |   +1   |   0   |   0   |     "    |      "     |     "     |     "     |     "    |
/// |  +0.5 |   "  |    "   |   "   |   "   |     "    |      "     |     0     |     0     |     0    |
/// |  +0.2 |   "  |    "   |   "   |   "   |     0    |      0     |     "     |     "     |     "    |
/// |  -0.2 |   0  |   -1   |   -1  |   "   |     "    |      "     |     "     |     "     |     "    |
/// |  -0.5 |   "  |    "   |   "   |   "   |     "    |     -1     |     -1    |     "     |     "    |
/// |  -0.8 |   "  |    "   |   "   |   "   |    -1    |      "     |     "     |     -1    |    -1    |
/// |  -1.2 |  -1  |   -2   |   -2  |   -1  |     "    |      "     |     "     |     "     |     "    |
/// |  -1.5 |   "  |    "   |   "   |   "   |     "    |     -2     |     -2    |     "     |    -2    |
/// |  -1.8 |   "  |    "   |   "   |   "   |    -2    |      "     |     "     |     -2    |     "    |
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[non_exhaustive]
pub enum RoundingMode {
    /// Round up, or towards positive infinity.
    Ceil,
    /// Round away from zero, or towards infinity.
    Expand,
    /// Round down, or towards negative infinity.
    Floor,
    /// Round towards zero, or away from infinity.
    Trunc,
    /// Round to the nearest integer, resolving ties by rounding up.
    HalfCeil,
    /// Round to the nearest integer, resolving ties by rounding away from zero.
    HalfExpand,
    /// Round to the nearest integer, resolving ties by rounding down.
    HalfFloor,
    /// Round to the nearest integer, resolving ties by rounding towards zero.
    HalfTrunc,
    /// Round to the nearest integer, resolving ties by rounding towards the nearest even integer.
    HalfEven,
}

/// Increment used in a rounding operation.
///
/// Forces a rounding operation to round to only multiples of the specified increment.
///
/// # Example
///
/// ```
/// use fixed_decimal::{FixedDecimal, RoundingIncrement, RoundingMode};
/// use writeable::assert_writeable_eq;
/// # use std::str::FromStr;
/// let dec = FixedDecimal::from_str("-7.266").unwrap();
/// let mode = RoundingMode::Expand;
/// let increments = [
///     // .266 normally expands to .27 when rounding on position -2...
///     (RoundingIncrement::MultiplesOf1, "-7.27"),
///     // ...however, when rounding to multiples of 2, .266 expands to .28, since the next multiple
///     // of 2 bigger than the least significant digit of the rounded value (7) is 8.
///     (RoundingIncrement::MultiplesOf2, "-7.28"),
///     // .266 expands to .30, since the next multiple of 5 bigger than 7 is 10.
///     (RoundingIncrement::MultiplesOf5, "-7.30"),
///     // .266 expands to .50, since the next multiple of 25 bigger than 27 is 50.
///     // Note how we compare against 27 instead of only 7, because the increment applies to
///     // the two least significant digits of the rounded value instead of only the least
///     // significant digit.
///     (RoundingIncrement::MultiplesOf25, "-7.50"),
/// ];
///
/// for (increment, expected) in increments {
///     assert_writeable_eq!(
///         dec.clone().rounded_with_mode_and_increment(
///             -2,
///             mode,
///             increment
///         ),
///         expected
///     );
/// }
/// ```
#[derive(Debug, Eq, PartialEq, Clone, Copy, Default)]
#[non_exhaustive]
pub enum RoundingIncrement {
    /// Round the least significant digit to any digit (0-9).
    ///
    /// This is the default rounding increment for all the methods that don't take a
    /// `RoundingIncrement` as an argument.
    #[default]
    MultiplesOf1,
    /// Round the least significant digit to multiples of two (0, 2, 4, 6, 8).
    MultiplesOf2,
    /// Round the least significant digit to multiples of five (0, 5).
    MultiplesOf5,
    /// Round the two least significant digits to multiples of twenty-five (0, 25, 50, 75).
    ///
    /// With this increment, the rounding position index will match the least significant digit
    /// of the multiple of 25; e.g. the number .264 expanded at position -2 using increments of 25
    /// will give .50 as a result, since the next multiple of 25 bigger than 26 is 50.
    MultiplesOf25,
}

// Adapters to convert runtime dispatched calls into const-inlined methods.
// This allows reducing the codesize for the common case of no increment.

#[derive(Copy, Clone, PartialEq)]
struct NoIncrement;

trait IncrementLike: Copy + Sized + PartialEq {
    const MULTIPLES_OF_1: Option<Self>;
    const MULTIPLES_OF_2: Option<Self>;
    const MULTIPLES_OF_5: Option<Self>;
    const MULTIPLES_OF_25: Option<Self>;
}

impl IncrementLike for RoundingIncrement {
    const MULTIPLES_OF_1: Option<Self> = Some(Self::MultiplesOf1);

    const MULTIPLES_OF_2: Option<Self> = Some(Self::MultiplesOf2);

    const MULTIPLES_OF_5: Option<Self> = Some(Self::MultiplesOf5);

    const MULTIPLES_OF_25: Option<Self> = Some(Self::MultiplesOf25);
}

impl IncrementLike for NoIncrement {
    const MULTIPLES_OF_1: Option<Self> = Some(Self);

    const MULTIPLES_OF_2: Option<Self> = None;

    const MULTIPLES_OF_5: Option<Self> = None;

    const MULTIPLES_OF_25: Option<Self> = None;
}

impl FixedDecimal {
    /// Initialize a `FixedDecimal` with an iterator of digits in ascending
    /// order of magnitude, starting with the digit at magnitude 0.
    ///
    /// This method is not public; use `TryFrom::<isize>` instead.
    fn from_ascending<T>(digits_iter: T) -> Result<Self, ParseError>
    where
        T: Iterator<Item = u8>,
    {
        // TODO: make X a usize generic to customize the size of this array
        // https://github.com/rust-lang/rust/issues/44580
        // NOTE: 39 is the size required for u128: ceil(log10(u128::MAX)) == 39.
        const X: usize = 39;
        // A temporary structure to allow the digits in the iterator to be reversed.
        // The digits are inserted started from the end, and then a slice is copied
        // into its final destination (result.digits).
        let mut mem: [u8; X] = [0u8; X];
        let mut trailing_zeros: usize = 0;
        let mut i: usize = 0;
        for (x, d) in digits_iter.enumerate() {
            // Take only up to i16::MAX values so that we have enough capacity
            if x > i16::MAX as usize {
                return Err(ParseError::Limit);
            }
            // TODO: Should we check here that `d` is between 0 and 9?
            // That should always be the case if IntIterator is used.
            if i != 0 || d != 0 {
                i += 1;
                match X.checked_sub(i) {
                    #[allow(clippy::indexing_slicing)] // X - i < X
                    Some(v) => mem[v] = d,
                    // This error should be obsolete after X is made generic
                    None => return Err(ParseError::Limit),
                }
            } else {
                trailing_zeros += 1;
            }
        }
        let mut result: Self = Default::default();
        if i != 0 {
            let magnitude = trailing_zeros + i - 1;
            debug_assert!(magnitude <= i16::MAX as usize);
            result.magnitude = magnitude as i16;
            result.upper_magnitude = result.magnitude;
            debug_assert!(i <= X);
            #[allow(clippy::indexing_slicing)] // X - i < X
            result.digits.extend_from_slice(&mem[(X - i)..]);
        }
        #[cfg(debug_assertions)]
        result.check_invariants();
        Ok(result)
    }

    /// Gets the digit at the specified order of magnitude. Returns 0 if the magnitude is out of
    /// range of the currently visible digits.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let dec = FixedDecimal::from(945);
    /// assert_eq!(0, dec.digit_at(-1));
    /// assert_eq!(5, dec.digit_at(0));
    /// assert_eq!(4, dec.digit_at(1));
    /// assert_eq!(9, dec.digit_at(2));
    /// assert_eq!(0, dec.digit_at(3));
    /// ```
    pub fn digit_at(&self, magnitude: i16) -> u8 {
        if magnitude > self.magnitude {
            0 // Leading zero
        } else {
            // The following line can't fail: magnitude <= self.magnitude, by
            // the if statement above, and u16::MAX == i16::MAX - i16::MIN, and
            // usize is asserted to be at least as big as u16.
            let j = crate::ops::i16_abs_sub(self.magnitude, magnitude) as usize;
            match self.digits.get(j) {
                Some(v) => *v,
                None => 0, // Trailing zero
            }
        }
    }

    /// Gets the digit at the specified order of next upper magnitude (magnitude + 1).
    /// Returns 0 if the next upper magnitude is out of range of currently visible digits or
    /// the magnitude is equal to `i16::MAX`.
    fn digit_at_previous_position(&self, magnitude: i16) -> u8 {
        if magnitude == i16::MAX {
            0
        } else {
            self.digit_at(magnitude + 1)
        }
    }

    /// Gets the digit at the specified order of next lower magnitude (magnitude - 1).
    /// Returns 0 if the next lower magnitude is out of range of currently visible digits or the
    /// magnitude is equal to `i16::MIN`.
    fn digit_at_next_position(&self, magnitude: i16) -> u8 {
        if magnitude == i16::MIN {
            0
        } else {
            self.digit_at(magnitude - 1)
        }
    }

    /// Returns the relative ordering of the digits after `magnitude` with respect to 5.
    fn half_at_next_magnitude(&self, magnitude: i16) -> Ordering {
        #[cfg(debug_assertions)] // Depends on having no trailing zeroes.
        self.check_invariants();

        match self.digit_at_next_position(magnitude).cmp(&5) {
            // If the next digit is equal to 5, we can know if we're at exactly 5
            // by comparing the next magnitude with the last nonzero magnitude of
            // the number.
            Ordering::Equal => match (magnitude - 1).cmp(&self.nonzero_magnitude_end()) {
                Ordering::Greater => {
                    // `magnitude - 1` has non-zero digits at its right,
                    // meaning the number overall must be greater than 5.
                    Ordering::Greater
                }
                Ordering::Equal => {
                    // `magnitude - 1` is the last digit of the number,
                    // meaning the number is exactly 5.
                    Ordering::Equal
                }
                Ordering::Less => {
                    debug_assert!(false, "digit `magnitude - 1` should not be zero");
                    Ordering::Less
                }
            },
            // If the next digit is either greater or less than 5,
            // we know that the digits cannot sum to exactly 5.
            ord => ord,
        }
    }

    /// Returns the relative ordering of the digits from `magnitude` onwards
    /// with respect to the half increment of `increment`.
    fn half_increment_at_magnitude<R: IncrementLike>(
        &self,
        magnitude: i16,
        increment: R,
    ) -> Ordering {
        #[cfg(debug_assertions)] // Depends on having no trailing zeroes.
        self.check_invariants();

        match Some(increment) {
            x if x == R::MULTIPLES_OF_1 => self.half_at_next_magnitude(magnitude),
            x if x == R::MULTIPLES_OF_2 => {
                let current_digit = self.digit_at(magnitude);

                // Equivalent to "if current_digit is odd".
                if current_digit & 0x01 == 1 {
                    match magnitude.cmp(&self.nonzero_magnitude_end()) {
                        Ordering::Greater => {
                            // `magnitude` has non-zero digits at its right,
                            // meaning the number overall must be greater than
                            // the half increment.
                            Ordering::Greater
                        }
                        Ordering::Equal => {
                            // `magnitude` is the last digit of the number,
                            // meaning the number is exactly at a half increment.
                            Ordering::Equal
                        }
                        Ordering::Less => {
                            debug_assert!(false, "digit `magnitude` should not be zero");
                            Ordering::Less
                        }
                    }
                } else {
                    // Even numbers are always below the half increment.
                    Ordering::Less
                }
            }
            x if x == R::MULTIPLES_OF_5 => {
                let current_digit = self.digit_at(magnitude);
                match (current_digit % 5).cmp(&2) {
                    Ordering::Equal => {
                        // Need to know if the number exceeds 2.5.
                        self.half_at_next_magnitude(magnitude)
                    }
                    // If the next digit is either greater or less than the half increment,
                    // we know that the digits cannot sum to exactly it.
                    ord => ord,
                }
            }
            x if x == R::MULTIPLES_OF_25 => {
                let current_digit = self.digit_at(magnitude);
                let prev_digit = self.digit_at_previous_position(magnitude);
                let number = prev_digit * 10 + current_digit;

                match (number % 25).cmp(&12) {
                    Ordering::Equal => {
                        // Need to know if the number exceeds 12.5.
                        self.half_at_next_magnitude(magnitude)
                    }
                    // If the next digit is either greater or less than the half increment,
                    // we know that the digits cannot sum to exactly it.
                    ord => ord,
                }
            }
            _ => {
                debug_assert!(false, "increment should be 1, 2, 5, or 25");
                Ordering::Equal
            }
        }
    }

    /// Checks if this number is already rounded to the specified magnitude and
    /// increment.
    fn is_rounded<R: IncrementLike>(&self, position: i16, increment: R) -> bool {
        // The number is rounded if it's already zero, since zero is a multiple of
        // all increments.
        if self.is_zero() {
            return true;
        }

        match Some(increment) {
            x if x == R::MULTIPLES_OF_1 => {
                // The number is rounded if `position` is the last digit
                position <= self.nonzero_magnitude_end()
            }
            x if x == R::MULTIPLES_OF_2 => {
                // The number is rounded if the digit at `position` is zero or
                // the position is the last digit and the digit is a multiple of the increment
                // already.
                match position.cmp(&self.nonzero_magnitude_end()) {
                    Ordering::Less => true,
                    Ordering::Equal => self.digit_at(position) & 0x01 == 0,
                    Ordering::Greater => false,
                }
            }
            x if x == R::MULTIPLES_OF_5 => {
                // The number is rounded if the digit at `position` is zero or
                // the position is the last digit and the digit is a multiple of the increment
                // already.
                match position.cmp(&self.nonzero_magnitude_end()) {
                    Ordering::Less => true,
                    Ordering::Equal => self.digit_at(position) == 5,
                    Ordering::Greater => false,
                }
            }
            x if x == R::MULTIPLES_OF_25 => {
                // The number is rounded if the digits at `position` and `position + 1` are
                // both zeroes or if the last two digits are a multiple of the increment already.
                match position.cmp(&self.nonzero_magnitude_end()) {
                    Ordering::Less => {
                        // `position` cannot be `i16::MAX` since it's less than
                        // `self.nonzero_magnitude_end()`
                        if position + 1 < self.nonzero_magnitude_end() {
                            true
                        } else {
                            // position + 1 is equal to the last digit.
                            // Need to exit if the number is 50.
                            self.digit_at(position + 1) == 5
                        }
                    }
                    Ordering::Equal => {
                        let current_digit = self.digit_at(position);
                        let prev_digit = self.digit_at_previous_position(position);
                        let number = prev_digit * 10 + current_digit;

                        matches!(number, 25 | 75)
                    }
                    Ordering::Greater => false,
                }
            }
            _ => {
                debug_assert!(false, "INCREMENT should be 1, 2, 5, or 25");
                false
            }
        }
    }

    /// Gets the visible range of digit magnitudes, in ascending order of magnitude. Call `.rev()`
    /// on the return value to get the range in descending order. Magnitude 0 is always included,
    /// even if the number has leading or trailing zeros.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let dec: FixedDecimal = "012.340".parse().expect("valid syntax");
    /// assert_eq!(-3..=2, dec.magnitude_range());
    /// ```
    pub const fn magnitude_range(&self) -> RangeInclusive<i16> {
        self.lower_magnitude..=self.upper_magnitude
    }

    /// Gets the magnitude of the largest nonzero digit. If the number is zero, 0 is returned.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let dec: FixedDecimal = "012.340".parse().expect("valid syntax");
    /// assert_eq!(1, dec.nonzero_magnitude_start());
    ///
    /// assert_eq!(0, FixedDecimal::from(0).nonzero_magnitude_start());
    /// ```
    pub fn nonzero_magnitude_start(&self) -> i16 {
        self.magnitude
    }

    /// Gets the magnitude of the smallest nonzero digit. If the number is zero, 0 is returned.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let dec: FixedDecimal = "012.340".parse().expect("valid syntax");
    /// assert_eq!(-2, dec.nonzero_magnitude_end());
    ///
    /// assert_eq!(0, FixedDecimal::from(0).nonzero_magnitude_end());
    /// ```
    pub fn nonzero_magnitude_end(&self) -> i16 {
        if self.is_zero() {
            0
        } else {
            crate::ops::i16_sub_unsigned(self.magnitude, self.digits.len() as u16 - 1)
        }
    }

    /// Returns whether the number has a numeric value of zero.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let dec: FixedDecimal = "000.000".parse().expect("valid syntax");
    /// assert!(dec.is_zero());
    /// ```
    #[inline]
    pub fn is_zero(&self) -> bool {
        self.digits.is_empty()
    }

    /// Clears all the fields and sets the number to zero.
    fn clear(&mut self) {
        self.upper_magnitude = 0;
        self.lower_magnitude = 0;
        self.magnitude = 0;
        self.digits.clear();
        self.sign = Sign::None;

        #[cfg(debug_assertions)]
        self.check_invariants();
    }

    /// Shift the digits of this number by a power of 10.
    ///
    /// Leading or trailing zeros may be added to keep the digit at magnitude 0 (the last digit
    /// before the decimal separator) visible.
    ///
    /// NOTE: if the operation causes overflow, the number will be set to zero.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let mut dec = FixedDecimal::from(42);
    /// assert_eq!("42", dec.to_string());
    ///
    /// dec.multiply_pow10(3);
    /// assert_eq!("42000", dec.to_string());
    /// ```
    pub fn multiply_pow10(&mut self, delta: i16) {
        match delta.cmp(&0) {
            Ordering::Greater => {
                let upper_magnitude = self.upper_magnitude.checked_add(delta);
                match upper_magnitude {
                    Some(upper_magnitude) => {
                        self.upper_magnitude = upper_magnitude;
                        // If we get here, then the magnitude change is in-bounds.
                        let lower_magnitude = self.lower_magnitude + delta;
                        self.lower_magnitude = cmp::min(0, lower_magnitude);
                    }
                    None => {
                        // there is an overflow
                        self.clear();
                    }
                }
            }
            Ordering::Less => {
                let lower_magnitude = self.lower_magnitude.checked_add(delta);
                match lower_magnitude {
                    Some(lower_magnitude) => {
                        self.lower_magnitude = lower_magnitude;
                        // If we get here, then the magnitude change is in-bounds.
                        let upper_magnitude = self.upper_magnitude + delta;
                        self.upper_magnitude = cmp::max(0, upper_magnitude);
                    }
                    None => {
                        // there is an overflow
                        self.clear();
                    }
                }
            }
            Ordering::Equal => {}
        };
        if !self.is_zero() {
            self.magnitude += delta;
        }
        #[cfg(debug_assertions)]
        self.check_invariants();
    }

    /// Returns this number with its digits shifted by a power of 10.
    ///
    /// Leading or trailing zeros may be added to keep the digit at magnitude 0 (the last digit
    /// before the decimal separator) visible.
    ///
    /// NOTE: if the operation causes overflow, the returned number will be zero.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let dec = FixedDecimal::from(42).multiplied_pow10(3);
    /// assert_eq!("42000", dec.to_string());
    /// ```
    pub fn multiplied_pow10(mut self, delta: i16) -> Self {
        self.multiply_pow10(delta);
        self
    }

    /// Returns the sign of this number.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// use fixed_decimal::Sign;
    /// # use std::str::FromStr;
    ///
    /// assert_eq!(FixedDecimal::from_str("1729").unwrap().sign(), Sign::None);
    /// assert_eq!(
    ///     FixedDecimal::from_str("-1729").unwrap().sign(),
    ///     Sign::Negative
    /// );
    /// assert_eq!(
    ///     FixedDecimal::from_str("+1729").unwrap().sign(),
    ///     Sign::Positive
    /// );
    /// ```
    pub fn sign(&self) -> Sign {
        self.sign
    }

    /// Changes the sign of this number to the one given.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// use fixed_decimal::Sign;
    ///
    /// let mut dec = FixedDecimal::from(1729);
    /// assert_eq!("1729", dec.to_string());
    ///
    /// dec.set_sign(Sign::Negative);
    /// assert_eq!("-1729", dec.to_string());
    ///
    /// dec.set_sign(Sign::Positive);
    /// assert_eq!("+1729", dec.to_string());
    ///
    /// dec.set_sign(Sign::None);
    /// assert_eq!("1729", dec.to_string());
    /// ```
    pub fn set_sign(&mut self, sign: Sign) {
        self.sign = sign;
    }

    /// Returns this number with the sign changed to the one given.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// use fixed_decimal::Sign;
    ///
    /// assert_eq!(
    ///     "+1729",
    ///     FixedDecimal::from(1729)
    ///         .with_sign(Sign::Positive)
    ///         .to_string()
    /// );
    /// assert_eq!(
    ///     "1729",
    ///     FixedDecimal::from(-1729).with_sign(Sign::None).to_string()
    /// );
    /// assert_eq!(
    ///     "-1729",
    ///     FixedDecimal::from(1729)
    ///         .with_sign(Sign::Negative)
    ///         .to_string()
    /// );
    /// ```
    pub fn with_sign(mut self, sign: Sign) -> Self {
        self.set_sign(sign);
        self
    }

    /// Sets the sign of this number according to the given sign display strategy.
    ///
    /// # Examples
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// use fixed_decimal::SignDisplay::*;
    ///
    /// let mut dec = FixedDecimal::from(1729);
    /// assert_eq!("1729", dec.to_string());
    /// dec.apply_sign_display(Always);
    /// assert_eq!("+1729", dec.to_string());
    /// ```
    pub fn apply_sign_display(&mut self, sign_display: SignDisplay) {
        use Sign::*;
        match sign_display {
            SignDisplay::Auto => {
                if self.sign != Negative {
                    self.sign = None
                }
            }
            SignDisplay::Always => {
                if self.sign != Negative {
                    self.sign = Positive
                }
            }
            SignDisplay::Never => self.sign = None,
            SignDisplay::ExceptZero => {
                if self.is_zero() {
                    self.sign = None
                } else if self.sign != Negative {
                    self.sign = Positive
                }
            }
            SignDisplay::Negative => {
                if self.sign != Negative || self.is_zero() {
                    self.sign = None
                }
            }
        }
    }

    /// Returns this number with its sign set according to the given sign display strategy.
    ///
    /// # Examples
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// use fixed_decimal::SignDisplay::*;
    ///
    /// assert_eq!(
    ///     "+1729",
    ///     FixedDecimal::from(1729)
    ///         .with_sign_display(ExceptZero)
    ///         .to_string()
    /// );
    /// ```
    pub fn with_sign_display(mut self, sign_display: SignDisplay) -> Self {
        self.apply_sign_display(sign_display);
        self
    }

    /// Returns this number with its leading zeroes removed.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let dec = FixedDecimal::from(123400)
    ///     .multiplied_pow10(-4)
    ///     .padded_start(4);
    /// assert_eq!("0012.3400", dec.to_string());
    ///
    /// assert_eq!("12.3400", dec.trimmed_start().to_string());
    /// ```
    pub fn trimmed_start(mut self) -> Self {
        self.trim_start();
        self
    }

    /// Removes the leading zeroes of this number.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let mut dec = FixedDecimal::from(123400)
    ///     .multiplied_pow10(-4)
    ///     .padded_start(4);
    /// assert_eq!("0012.3400", dec.to_string());
    ///
    /// dec.trim_start();
    /// assert_eq!("12.3400", dec.to_string());
    /// ```
    ///
    /// There is no effect if the most significant digit has magnitude less than zero:
    ///
    /// ```
    /// # use fixed_decimal::FixedDecimal;
    /// let mut dec = FixedDecimal::from(22).multiplied_pow10(-4);
    /// assert_eq!("0.0022", dec.to_string());
    ///
    /// dec.trim_start();
    /// assert_eq!("0.0022", dec.to_string());
    /// ```
    pub fn trim_start(&mut self) {
        self.upper_magnitude = cmp::max(self.magnitude, 0);
        #[cfg(debug_assertions)]
        self.check_invariants();
    }

    /// Returns this number with its trailing zeros removed.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let dec = FixedDecimal::from(123400)
    ///     .multiplied_pow10(-4)
    ///     .padded_start(4);
    /// assert_eq!("0012.3400", dec.to_string());
    ///
    /// assert_eq!("0012.34", dec.trimmed_end().to_string());
    /// ```
    pub fn trimmed_end(mut self) -> Self {
        self.trim_end();
        self
    }

    /// Removes the trailing zeros of this number.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let mut dec = FixedDecimal::from(123400)
    ///     .multiplied_pow10(-4)
    ///     .padded_start(4);
    /// assert_eq!("0012.3400", dec.to_string());
    ///
    /// dec.trim_end();
    /// assert_eq!("0012.34", dec.to_string());
    /// ```
    ///
    /// There is no effect if the least significant digit has magnitude more than zero:
    ///
    /// ```
    /// # use fixed_decimal::FixedDecimal;
    /// let mut dec = FixedDecimal::from(2200);
    /// assert_eq!("2200", dec.to_string());
    ///
    /// dec.trim_end();
    /// assert_eq!("2200", dec.to_string());
    /// ```
    pub fn trim_end(&mut self) {
        self.lower_magnitude = cmp::min(0, self.nonzero_magnitude_end());
        #[cfg(debug_assertions)]
        self.check_invariants();
    }

    /// Returns this number padded with leading zeros on a particular position.
    ///
    /// Negative position numbers have no effect.
    ///
    /// Also see [`FixedDecimal::with_max_position()`].
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let mut dec = FixedDecimal::from(42);
    /// assert_eq!("42", dec.to_string());
    /// assert_eq!("0042", dec.clone().padded_start(4).to_string());
    ///
    /// assert_eq!("042", dec.clone().padded_start(3).to_string());
    ///
    /// assert_eq!("42", dec.clone().padded_start(2).to_string());
    ///
    /// assert_eq!("42", dec.clone().padded_start(1).to_string());
    /// ```
    pub fn padded_start(mut self, position: i16) -> Self {
        self.pad_start(position);
        self
    }

    /// Pads this number with leading zeros on a particular position.
    ///
    /// Negative position numbers have no effect.
    ///
    /// Also see [`FixedDecimal::set_max_position()`].
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let mut dec = FixedDecimal::from(42);
    /// assert_eq!("42", dec.to_string());
    ///
    /// dec.pad_start(4);
    /// assert_eq!("0042", dec.to_string());
    ///
    /// dec.pad_start(3);
    /// assert_eq!("042", dec.to_string());
    ///
    /// dec.pad_start(2);
    /// assert_eq!("42", dec.to_string());
    ///
    /// dec.pad_start(1);
    /// assert_eq!("42", dec.to_string());
    /// ```
    pub fn pad_start(&mut self, position: i16) {
        if position <= 0 {
            return;
        }
        let mut magnitude = position - 1;
        // Do not truncate nonzero digits
        if magnitude <= self.magnitude {
            magnitude = self.magnitude;
        }
        self.upper_magnitude = magnitude;
        #[cfg(debug_assertions)]
        self.check_invariants();
    }

    /// Returns this number padded with trailing zeros on a particular (negative) position.
    /// Will truncate zeros if necessary, but will not truncate other digits.
    ///
    /// Positive position numbers have no effect.
    ///
    /// Also see [`FixedDecimal::trunced()`].
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("123.456").unwrap();
    /// assert_eq!("123.456", dec.to_string());
    ///
    /// assert_eq!("123.456", dec.clone().padded_end(-1).to_string());
    ///
    /// assert_eq!("123.456", dec.clone().padded_end(-2).to_string());
    ///
    /// assert_eq!("123.456000", dec.clone().padded_end(-6).to_string());
    ///
    /// assert_eq!("123.4560", dec.clone().padded_end(-4).to_string());
    /// ```
    pub fn padded_end(mut self, position: i16) -> Self {
        self.pad_end(position);
        self
    }

    /// Pads this number with trailing zeros on a particular (non-positive) position. Will truncate
    /// trailing zeros if necessary, but will not truncate other digits.
    ///
    /// Positive position numbers have no effect.
    ///
    /// Also see [`FixedDecimal::trunc()`].
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("123.456").unwrap();
    /// assert_eq!("123.456", dec.to_string());
    ///
    /// dec.pad_end(-2);
    /// assert_eq!("123.456", dec.to_string());
    ///
    /// dec.pad_end(-6);
    /// assert_eq!("123.456000", dec.to_string());
    ///
    /// let mut dec = FixedDecimal::from_str("123.000").unwrap();
    /// dec.pad_end(0);
    /// assert_eq!("123", dec.to_string());
    /// ```
    pub fn pad_end(&mut self, position: i16) {
        if position > 0 {
            return;
        }
        let bottom_magnitude = self.nonzero_magnitude_end();
        let mut magnitude = position;
        // Do not truncate nonzero digits
        if magnitude >= bottom_magnitude {
            magnitude = bottom_magnitude;
        }
        self.lower_magnitude = magnitude;
        #[cfg(debug_assertions)]
        self.check_invariants();
    }

    /// Returns this number with the leading significant digits truncated to a particular position,
    /// deleting digits if necessary.
    ///
    /// Also see [`FixedDecimal::padded_start()`].
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    /// assert_eq!("4235.970", dec.to_string());
    ///
    /// assert_eq!("04235.970", dec.clone().with_max_position(5).to_string());
    ///
    /// assert_eq!("35.970", dec.clone().with_max_position(2).to_string());
    ///
    /// assert_eq!("5.970", dec.clone().with_max_position(1).to_string());
    ///
    /// assert_eq!("0.970", dec.clone().with_max_position(0).to_string());
    ///
    /// assert_eq!("0.070", dec.clone().with_max_position(-1).to_string());
    ///
    /// assert_eq!("0.000", dec.clone().with_max_position(-2).to_string());
    ///
    /// assert_eq!("0.0000", dec.clone().with_max_position(-4).to_string());
    /// ```
    pub fn with_max_position(mut self, position: i16) -> Self {
        self.set_max_position(position);
        self
    }

    /// Truncates the leading significant digits of this number to a particular position, deleting
    /// digits if necessary.
    ///
    /// Also see [`FixedDecimal::pad_start()`].
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    /// assert_eq!("4235.970", dec.to_string());
    ///
    /// dec.set_max_position(5);
    /// assert_eq!("04235.970", dec.to_string());
    ///
    /// dec.set_max_position(2);
    /// assert_eq!("35.970", dec.to_string());
    ///
    /// dec.set_max_position(1);
    /// assert_eq!("5.970", dec.to_string());
    ///
    /// dec.set_max_position(0);
    /// assert_eq!("0.970", dec.to_string());
    ///
    /// dec.set_max_position(-1);
    /// assert_eq!("0.070", dec.to_string());
    ///
    /// dec.set_max_position(-2);
    /// assert_eq!("0.000", dec.to_string());
    ///
    /// dec.set_max_position(-4);
    /// assert_eq!("0.0000", dec.to_string());
    /// ```
    pub fn set_max_position(&mut self, position: i16) {
        self.lower_magnitude = cmp::min(self.lower_magnitude, position);
        self.upper_magnitude = if position <= 0 { 0 } else { position - 1 };
        if position <= self.nonzero_magnitude_end() {
            self.digits.clear();
            self.magnitude = 0;
            #[cfg(debug_assertions)]
            self.check_invariants();
            return;
        }
        let magnitude = position - 1;
        if self.magnitude >= magnitude {
            let cut = crate::ops::i16_abs_sub(self.magnitude, magnitude) as usize;
            let _ = self.digits.drain(0..cut).count();
            // Count number of leading zeroes
            let extra_zeroes = self.digits.iter().position(|x| *x != 0).unwrap_or(0);
            let _ = self.digits.drain(0..extra_zeroes).count();
            debug_assert!(!self.digits.is_empty());
            self.magnitude = crate::ops::i16_sub_unsigned(magnitude, extra_zeroes as u16);
        }
        #[cfg(debug_assertions)]
        self.check_invariants();
    }

    /// Rounds this number at a particular digit position.
    ///
    /// This uses half to even rounding, which rounds to the nearest integer and resolves ties by
    /// selecting the nearest even integer to the original value.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("-1.5").unwrap();
    /// dec.round(0);
    /// assert_eq!("-2", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.4").unwrap();
    /// dec.round(0);
    /// assert_eq!("0", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.5").unwrap();
    /// dec.round(0);
    /// assert_eq!("0", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.6").unwrap();
    /// dec.round(0);
    /// assert_eq!("1", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("1.5").unwrap();
    /// dec.round(0);
    /// assert_eq!("2", dec.to_string());
    /// ```
    pub fn round(&mut self, position: i16) {
        self.half_even_to_increment_internal(position, NoIncrement)
    }

    /// Returns this number rounded at a particular digit position.
    ///
    /// This uses half to even rounding by default, which rounds to the nearest integer and
    /// resolves ties by selecting the nearest even integer to the original value.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("-1.5").unwrap();
    /// assert_eq!("-2", dec.rounded(0).to_string());
    /// let mut dec = FixedDecimal::from_str("0.4").unwrap();
    /// assert_eq!("0", dec.rounded(0).to_string());
    /// let mut dec = FixedDecimal::from_str("0.5").unwrap();
    /// assert_eq!("0", dec.rounded(0).to_string());
    /// let mut dec = FixedDecimal::from_str("0.6").unwrap();
    /// assert_eq!("1", dec.rounded(0).to_string());
    /// let mut dec = FixedDecimal::from_str("1.5").unwrap();
    /// assert_eq!("2", dec.rounded(0).to_string());
    /// ```
    pub fn rounded(mut self, position: i16) -> Self {
        self.round(position);
        self
    }

    /// Rounds this number towards positive infinity at a particular digit position.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("-1.5").unwrap();
    /// dec.ceil(0);
    /// assert_eq!("-1", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.4").unwrap();
    /// dec.ceil(0);
    /// assert_eq!("1", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.5").unwrap();
    /// dec.ceil(0);
    /// assert_eq!("1", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.6").unwrap();
    /// dec.ceil(0);
    /// assert_eq!("1", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("1.5").unwrap();
    /// dec.ceil(0);
    /// assert_eq!("2", dec.to_string());
    /// ```
    #[inline(never)]
    pub fn ceil(&mut self, position: i16) {
        self.ceil_to_increment_internal(position, NoIncrement);
    }

    /// Returns this number rounded towards positive infinity at a particular digit position.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let dec = FixedDecimal::from_str("-1.5").unwrap();
    /// assert_eq!("-1", dec.ceiled(0).to_string());
    /// let dec = FixedDecimal::from_str("0.4").unwrap();
    /// assert_eq!("1", dec.ceiled(0).to_string());
    /// let dec = FixedDecimal::from_str("0.5").unwrap();
    /// assert_eq!("1", dec.ceiled(0).to_string());
    /// let dec = FixedDecimal::from_str("0.6").unwrap();
    /// assert_eq!("1", dec.ceiled(0).to_string());
    /// let dec = FixedDecimal::from_str("1.5").unwrap();
    /// assert_eq!("2", dec.ceiled(0).to_string());
    /// ```
    pub fn ceiled(mut self, position: i16) -> Self {
        self.ceil(position);
        self
    }

    /// Rounds this number away from zero at a particular digit position.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("-1.5").unwrap();
    /// dec.expand(0);
    /// assert_eq!("-2", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.4").unwrap();
    /// dec.expand(0);
    /// assert_eq!("1", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.5").unwrap();
    /// dec.expand(0);
    /// assert_eq!("1", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.6").unwrap();
    /// dec.expand(0);
    /// assert_eq!("1", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("1.5").unwrap();
    /// dec.expand(0);
    /// assert_eq!("2", dec.to_string());
    /// ```
    #[inline(never)]
    pub fn expand(&mut self, position: i16) {
        self.expand_to_increment_internal(position, NoIncrement)
    }

    /// Returns this number rounded away from zero at a particular digit position.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let dec = FixedDecimal::from_str("-1.5").unwrap();
    /// assert_eq!("-2", dec.expanded(0).to_string());
    /// let dec = FixedDecimal::from_str("0.4").unwrap();
    /// assert_eq!("1", dec.expanded(0).to_string());
    /// let dec = FixedDecimal::from_str("0.5").unwrap();
    /// assert_eq!("1", dec.expanded(0).to_string());
    /// let dec = FixedDecimal::from_str("0.6").unwrap();
    /// assert_eq!("1", dec.expanded(0).to_string());
    /// let dec = FixedDecimal::from_str("1.5").unwrap();
    /// assert_eq!("2", dec.expanded(0).to_string());
    /// ```
    pub fn expanded(mut self, position: i16) -> Self {
        self.expand(position);
        self
    }

    /// Rounds this number towards negative infinity at a particular digit position.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("-1.5").unwrap();
    /// dec.floor(0);
    /// assert_eq!("-2", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.4").unwrap();
    /// dec.floor(0);
    /// assert_eq!("0", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.5").unwrap();
    /// dec.floor(0);
    /// assert_eq!("0", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.6").unwrap();
    /// dec.floor(0);
    /// assert_eq!("0", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("1.5").unwrap();
    /// dec.floor(0);
    /// assert_eq!("1", dec.to_string());
    /// ```
    #[inline(never)]
    pub fn floor(&mut self, position: i16) {
        self.floor_to_increment_internal(position, NoIncrement);
    }

    /// Returns this number rounded towards negative infinity at a particular digit position.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let dec = FixedDecimal::from_str("-1.5").unwrap();
    /// assert_eq!("-2", dec.floored(0).to_string());
    /// let dec = FixedDecimal::from_str("0.4").unwrap();
    /// assert_eq!("0", dec.floored(0).to_string());
    /// let dec = FixedDecimal::from_str("0.5").unwrap();
    /// assert_eq!("0", dec.floored(0).to_string());
    /// let dec = FixedDecimal::from_str("0.6").unwrap();
    /// assert_eq!("0", dec.floored(0).to_string());
    /// let dec = FixedDecimal::from_str("1.5").unwrap();
    /// assert_eq!("1", dec.floored(0).to_string());
    /// ```
    pub fn floored(mut self, position: i16) -> Self {
        self.floor(position);
        self
    }

    /// Rounds this number towards zero at a particular digit position.
    ///
    /// Also see [`FixedDecimal::pad_end()`].
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("-1.5").unwrap();
    /// dec.trunc(0);
    /// assert_eq!("-1", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.4").unwrap();
    /// dec.trunc(0);
    /// assert_eq!("0", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.5").unwrap();
    /// dec.trunc(0);
    /// assert_eq!("0", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("0.6").unwrap();
    /// dec.trunc(0);
    /// assert_eq!("0", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("1.5").unwrap();
    /// dec.trunc(0);
    /// assert_eq!("1", dec.to_string());
    /// ```
    #[inline(never)]
    pub fn trunc(&mut self, position: i16) {
        self.trunc_to_increment_internal(position, NoIncrement)
    }

    /// Returns this number rounded towards zero at a particular digit position.
    ///
    /// Also see [`FixedDecimal::padded_end()`].
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    /// # use std::str::FromStr;
    ///
    /// let dec = FixedDecimal::from_str("-1.5").unwrap();
    /// assert_eq!("-1", dec.trunced(0).to_string());
    /// let dec = FixedDecimal::from_str("0.4").unwrap();
    /// assert_eq!("0", dec.trunced(0).to_string());
    /// let dec = FixedDecimal::from_str("0.5").unwrap();
    /// assert_eq!("0", dec.trunced(0).to_string());
    /// let dec = FixedDecimal::from_str("0.6").unwrap();
    /// assert_eq!("0", dec.trunced(0).to_string());
    /// let dec = FixedDecimal::from_str("1.5").unwrap();
    /// assert_eq!("1", dec.trunced(0).to_string());
    /// ```
    pub fn trunced(mut self, position: i16) -> Self {
        self.trunc(position);
        self
    }

    /// Rounds this number at a particular digit position, using the specified rounding mode.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::{FixedDecimal, RoundingMode};
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("-3.5").unwrap();
    /// dec.round_with_mode(0, RoundingMode::Floor);
    /// assert_eq!("-4", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("-3.5").unwrap();
    /// dec.round_with_mode(0, RoundingMode::Ceil);
    /// assert_eq!("-3", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("5.455").unwrap();
    /// dec.round_with_mode(-2, RoundingMode::HalfExpand);
    /// assert_eq!("5.46", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("-7.235").unwrap();
    /// dec.round_with_mode(-2, RoundingMode::HalfTrunc);
    /// assert_eq!("-7.23", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("9.75").unwrap();
    /// dec.round_with_mode(-1, RoundingMode::HalfEven);
    /// assert_eq!("9.8", dec.to_string());
    /// ```
    pub fn round_with_mode(&mut self, position: i16, mode: RoundingMode) {
        match mode {
            RoundingMode::Ceil => self.ceil_to_increment_internal(position, NoIncrement),
            RoundingMode::Expand => self.expand_to_increment_internal(position, NoIncrement),
            RoundingMode::Floor => self.floor_to_increment_internal(position, NoIncrement),
            RoundingMode::Trunc => self.trunc_to_increment_internal(position, NoIncrement),
            RoundingMode::HalfCeil => self.half_ceil_to_increment_internal(position, NoIncrement),
            RoundingMode::HalfExpand => {
                self.half_expand_to_increment_internal(position, NoIncrement)
            }
            RoundingMode::HalfFloor => self.half_floor_to_increment_internal(position, NoIncrement),
            RoundingMode::HalfTrunc => self.half_trunc_to_increment_internal(position, NoIncrement),
            RoundingMode::HalfEven => self.half_even_to_increment_internal(position, NoIncrement),
        }
    }

    /// Returns this number rounded at a particular digit position, using the specified rounding mode.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::{FixedDecimal, RoundingMode};
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("-3.5").unwrap();
    /// assert_eq!(
    ///     "-4",
    ///     dec.rounded_with_mode(0, RoundingMode::Floor).to_string()
    /// );
    /// let mut dec = FixedDecimal::from_str("-3.5").unwrap();
    /// assert_eq!(
    ///     "-3",
    ///     dec.rounded_with_mode(0, RoundingMode::Ceil).to_string()
    /// );
    /// let mut dec = FixedDecimal::from_str("5.455").unwrap();
    /// assert_eq!(
    ///     "5.46",
    ///     dec.rounded_with_mode(-2, RoundingMode::HalfExpand)
    ///         .to_string()
    /// );
    /// let mut dec = FixedDecimal::from_str("-7.235").unwrap();
    /// assert_eq!(
    ///     "-7.23",
    ///     dec.rounded_with_mode(-2, RoundingMode::HalfTrunc)
    ///         .to_string()
    /// );
    /// let mut dec = FixedDecimal::from_str("9.75").unwrap();
    /// assert_eq!(
    ///     "9.8",
    ///     dec.rounded_with_mode(-1, RoundingMode::HalfEven)
    ///         .to_string()
    /// );
    /// ```
    pub fn rounded_with_mode(mut self, position: i16, mode: RoundingMode) -> Self {
        self.round_with_mode(position, mode);
        self
    }

    /// Rounds this number at a particular digit position and increment, using the specified rounding mode.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::{FixedDecimal, RoundingIncrement, RoundingMode};
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("-3.5").unwrap();
    /// dec.round_with_mode_and_increment(
    ///     0,
    ///     RoundingMode::Floor,
    ///     RoundingIncrement::MultiplesOf1,
    /// );
    /// assert_eq!("-4", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("-3.59").unwrap();
    /// dec.round_with_mode_and_increment(
    ///     -1,
    ///     RoundingMode::Ceil,
    ///     RoundingIncrement::MultiplesOf2,
    /// );
    /// assert_eq!("-3.4", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("5.455").unwrap();
    /// dec.round_with_mode_and_increment(
    ///     -2,
    ///     RoundingMode::HalfExpand,
    ///     RoundingIncrement::MultiplesOf5,
    /// );
    /// assert_eq!("5.45", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("-7.235").unwrap();
    /// dec.round_with_mode_and_increment(
    ///     -2,
    ///     RoundingMode::HalfTrunc,
    ///     RoundingIncrement::MultiplesOf25,
    /// );
    /// assert_eq!("-7.25", dec.to_string());
    /// let mut dec = FixedDecimal::from_str("9.75").unwrap();
    /// dec.round_with_mode_and_increment(
    ///     -1,
    ///     RoundingMode::HalfEven,
    ///     RoundingIncrement::MultiplesOf5,
    /// );
    /// assert_eq!("10.0", dec.to_string());
    /// ```
    pub fn round_with_mode_and_increment(
        &mut self,
        position: i16,
        mode: RoundingMode,
        increment: RoundingIncrement,
    ) {
        match mode {
            RoundingMode::Ceil => self.ceil_to_increment_internal(position, increment),
            RoundingMode::Expand => self.expand_to_increment_internal(position, increment),
            RoundingMode::Floor => self.floor_to_increment_internal(position, increment),
            RoundingMode::Trunc => self.trunc_to_increment_internal(position, increment),
            RoundingMode::HalfCeil => self.half_ceil_to_increment_internal(position, increment),
            RoundingMode::HalfExpand => self.half_expand_to_increment_internal(position, increment),
            RoundingMode::HalfFloor => self.half_floor_to_increment_internal(position, increment),
            RoundingMode::HalfTrunc => self.half_trunc_to_increment_internal(position, increment),
            RoundingMode::HalfEven => self.half_even_to_increment_internal(position, increment),
        }
    }

    /// Returns this number rounded at a particular digit position and increment, using the specified rounding mode.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::{FixedDecimal, RoundingIncrement, RoundingMode};
    /// # use std::str::FromStr;
    ///
    /// let mut dec = FixedDecimal::from_str("-3.5").unwrap();
    /// assert_eq!(
    ///     "-4",
    ///     dec.rounded_with_mode_and_increment(
    ///         0,
    ///         RoundingMode::Floor,
    ///         RoundingIncrement::MultiplesOf1
    ///     )
    ///     .to_string()
    /// );
    /// let mut dec = FixedDecimal::from_str("-3.59").unwrap();
    /// assert_eq!(
    ///     "-3.4",
    ///     dec.rounded_with_mode_and_increment(
    ///         -1,
    ///         RoundingMode::Ceil,
    ///         RoundingIncrement::MultiplesOf2
    ///     )
    ///     .to_string()
    /// );
    /// let mut dec = FixedDecimal::from_str("5.455").unwrap();
    /// assert_eq!(
    ///     "5.45",
    ///     dec.rounded_with_mode_and_increment(
    ///         -2,
    ///         RoundingMode::HalfExpand,
    ///         RoundingIncrement::MultiplesOf5
    ///     )
    ///     .to_string()
    /// );
    /// let mut dec = FixedDecimal::from_str("-7.235").unwrap();
    /// assert_eq!(
    ///     "-7.25",
    ///     dec.rounded_with_mode_and_increment(
    ///         -2,
    ///         RoundingMode::HalfTrunc,
    ///         RoundingIncrement::MultiplesOf25
    ///     )
    ///     .to_string()
    /// );
    /// let mut dec = FixedDecimal::from_str("9.75").unwrap();
    /// assert_eq!(
    ///     "10.0",
    ///     dec.rounded_with_mode_and_increment(
    ///         -1,
    ///         RoundingMode::HalfEven,
    ///         RoundingIncrement::MultiplesOf5
    ///     )
    ///     .to_string()
    /// );
    /// ```
    pub fn rounded_with_mode_and_increment(
        mut self,
        position: i16,
        mode: RoundingMode,
        increment: RoundingIncrement,
    ) -> Self {
        self.round_with_mode_and_increment(position, mode, increment);
        self
    }

    fn ceil_to_increment_internal<R: IncrementLike>(&mut self, position: i16, increment: R) {
        if self.sign == Sign::Negative {
            self.trunc_to_increment_internal(position, increment);
            return;
        }

        self.expand_to_increment_internal(position, increment);
    }

    fn expand_to_increment_internal<R: IncrementLike>(
        &mut self,
        position: i16,
        inner_increment: R,
    ) {
        /// Modifies `number` to signal that an overflow happened.
        fn overflow(number: &mut FixedDecimal) {
            // TODO(#2297): Decide on behavior here
            number.digits.clear();
            number.magnitude = 0;
            #[cfg(debug_assertions)]
            number.check_invariants();
        }

        let increment = Some(inner_increment);

        // 1. Set upper and lower magnitude
        self.lower_magnitude = cmp::min(position, 0);

        match position.cmp(&i16::MIN) {
            // Don't return if the increment is not one, because we
            // also need to round to the next increment if necessary.
            Ordering::Equal if increment == R::MULTIPLES_OF_1 => {
                // Nothing more to do
                #[cfg(debug_assertions)]
                self.check_invariants();
                return;
            }
            Ordering::Greater => {
                self.upper_magnitude = cmp::max(self.upper_magnitude, position - 1);
            }
            _ => {
                // Ordering::Less is unreachable, and Ordering::Equal needs to apply roundings
                // when the increment is not equal to 1.
                // We don't override `self.upper_magnitude` because `self.upper_magnitude` is
                // always equal or bigger than `i16::MIN`.
            }
        }

        // 2. If the number is already rounded, exit early.
        if self.is_rounded(position, inner_increment) {
            #[cfg(debug_assertions)]
            self.check_invariants();
            return;
        }

        // 3. If the rounding position is *in the middle* of the nonzero digits
        if position <= self.magnitude {
            // 3a. Calculate the number of digits to retain and remove the rest
            let digits_to_retain = crate::ops::i16_abs_sub(self.magnitude, position) + 1;
            self.digits.truncate(digits_to_retain as usize);

            // 3b. Handle the last digit considering the increment.
            let iter = match increment {
                x if x == R::MULTIPLES_OF_1 => {
                    // Can just execute the algorithm normally.

                    self.digits.iter_mut().rev().enumerate()
                }
                x if x == R::MULTIPLES_OF_2 => {
                    let mut iter = self.digits.iter_mut().rev().enumerate();

                    // Must round the last digit to the next increment.
                    let Some((_, digit)) = iter.next() else {
                        debug_assert!(false, "`self.digits` should have at least a digit");
                        return;
                    };

                    // Equivalent to (n + 2 / 2) * 2, which expands to the next
                    // multiple of two
                    *digit = (*digit + 2) & 0xFE;

                    if *digit < 10 {
                        // Early returns if the expansion didn't generate a remainder.
                        #[cfg(debug_assertions)]
                        self.check_invariants();
                        return;
                    }

                    iter
                }
                x if x == R::MULTIPLES_OF_5 => {
                    let mut iter = self.digits.iter_mut().rev().enumerate();

                    // Must round the last digit to the next increment.
                    let Some((_, digit)) = iter.next() else {
                        debug_assert!(false, "`self.digits` should have at least a digit");
                        return;
                    };

                    if *digit < 5 {
                        *digit = 5;

                        // Early return since the expansion didn't generate a remainder.
                        #[cfg(debug_assertions)]
                        self.check_invariants();
                        return;
                    }

                    *digit = 10;

                    iter
                }
                x if x == R::MULTIPLES_OF_25 => {
                    // Extend the digits to have the correct
                    // number of trailing zeroes.
                    self.digits.resize(digits_to_retain as usize, 0);

                    let Some((last_digit, digits)) = self.digits.split_last_mut() else {
                        debug_assert!(false, "`self.digits` should have at least a digit");
                        return;
                    };

                    let Some((second_last_digit, _)) = digits.split_last_mut() else {
                        // The number has no other digits aside from the last.
                        // We need to manually increment the number to 25

                        if self.magnitude == i16::MAX {
                            overflow(self);
                            return;
                        }

                        self.digits.clear();
                        self.digits.extend_from_slice(&[2, 5]);
                        self.magnitude += 1;
                        self.upper_magnitude = cmp::max(self.upper_magnitude, self.magnitude);

                        #[cfg(debug_assertions)]
                        self.check_invariants();
                        return;
                    };

                    let number = *second_last_digit * 10 + *last_digit;

                    if number < 75 {
                        // We can return directly if the number won't expand
                        // to 100.
                        if number < 25 {
                            *second_last_digit = 2;
                            *last_digit = 5;
                        } else if number < 50 {
                            *second_last_digit = 5;
                            self.digits.pop();
                        } else {
                            *second_last_digit = 7;
                            *last_digit = 5;
                        }

                        #[cfg(debug_assertions)]
                        self.check_invariants();
                        return;
                    }

                    // The number reached 100. Continue the algorithm but with the
                    // last two digits removed from the iterator.

                    *second_last_digit = 10;
                    *last_digit = 10;

                    let mut iter = self.digits.iter_mut().rev().enumerate();

                    // Remove the last two items that were already handled.
                    iter.next();
                    iter.next();

                    iter
                }
                _ => {
                    debug_assert!(false, "INCREMENT should be 1, 2, 5, or 25");
                    return;
                }
            };

            // 3b. Increment the rightmost remaining digit since we are rounding up; this might
            // require bubbling the addition to higher magnitudes, like 199 + 1 = 200
            for (zero_count, digit) in iter {
                *digit += 1;
                if *digit < 10 {
                    self.digits.truncate(self.digits.len() - zero_count);
                    #[cfg(debug_assertions)]
                    self.check_invariants();
                    return;
                }
            }

            // 3c. If we get here, the mantissa is fully saturated.
            // Clear the saturated mantissa and replace everything with a single 1.

            if self.magnitude == i16::MAX {
                overflow(self);
                return;
            }

            self.digits.clear();
            self.digits.push(1);

            self.magnitude += 1;
            self.upper_magnitude = cmp::max(self.upper_magnitude, self.magnitude);

            #[cfg(debug_assertions)]
            self.check_invariants();
            return;
        }

        // 4. If the rounding position is *above* the leftmost nonzero digit,
        // set the value to the next increment at the appropriate position.

        // Check if we have enough available spaces to push a `25` at the start.
        if increment == R::MULTIPLES_OF_25 && position == i16::MAX {
            // Need an extra digit to push a 25, which we don't have.
            overflow(self);
            return;
        }

        self.digits.clear();

        // We need to push the next multiple of the increment,
        // and we also need to adjust the magnitude to the
        // new upper magnitude.
        let increment_digits = match increment {
            x if x == R::MULTIPLES_OF_1 => [1].as_slice(),
            x if x == R::MULTIPLES_OF_2 => [2].as_slice(),
            x if x == R::MULTIPLES_OF_5 => [5].as_slice(),
            x if x == R::MULTIPLES_OF_25 => [2, 5].as_slice(),
            _ => {
                debug_assert!(false, "INCREMENT should be 1, 2, 5, or 25");
                return;
            }
        };
        self.digits.extend_from_slice(increment_digits);

        // Could also be derived from `increment_digits.len() - 1`, but it makes
        // the logic a bit harder to follow.
        self.magnitude = if increment == R::MULTIPLES_OF_25 {
            position + 1
        } else {
            position
        };

        self.upper_magnitude = cmp::max(self.upper_magnitude, self.magnitude);

        #[cfg(debug_assertions)]
        self.check_invariants();
    }

    fn floor_to_increment_internal<R: IncrementLike>(&mut self, position: i16, increment: R) {
        if self.sign == Sign::Negative {
            self.expand_to_increment_internal(position, increment);
            return;
        }

        self.trunc_to_increment_internal(position, increment);
    }

    fn trunc_to_increment_internal<R: IncrementLike>(&mut self, position: i16, inner_increment: R) {
        let increment = Some(inner_increment);

        // 1. Set upper and lower magnitude
        self.lower_magnitude = cmp::min(position, 0);

        match position.cmp(&i16::MIN) {
            // Don't return if the increment is not one, because we
            // also need to round to the next increment if necessary.
            Ordering::Equal if increment == R::MULTIPLES_OF_1 => {
                // Nothing more to do
                #[cfg(debug_assertions)]
                self.check_invariants();
                return;
            }
            Ordering::Greater => {
                self.upper_magnitude = cmp::max(self.upper_magnitude, position - 1);
            }
            _ => {
                // Ordering::Less is unreachable, and Ordering::Equal needs to apply roundings
                // when the increment is not equal to 1.
                // We don't override `self.upper_magnitude` because `self.upper_magnitude` is
                // always equal or bigger than `i16::MIN`.
            }
        }

        // 2. If the number is already rounded, exit early.
        if self.is_rounded(position, inner_increment) {
            #[cfg(debug_assertions)]
            self.check_invariants();
            return;
        }

        // 3. If the rounding position is *in the middle* of the nonzero digits
        if position <= self.magnitude {
            // 3a. Calculate the number of digits to retain and remove the rest
            let digits_to_retain = crate::ops::i16_abs_sub(self.magnitude, position) + 1;
            self.digits.truncate(digits_to_retain as usize);

            // 3b. Truncate to the previous multiple.
            match increment {
                x if x == R::MULTIPLES_OF_1 => {
                    // No need to do more work, trailing zeroes are removed below.
                }
                x if x == R::MULTIPLES_OF_2 => {
                    let Some(last_digit) = self.digits.last_mut() else {
                        debug_assert!(false, "`self.digits` should have at least a digit");
                        return;
                    };

                    // Equivalent to (n / 2) * 2, which truncates to the previous
                    // multiple of two
                    *last_digit &= 0xFE;
                }
                x if x == R::MULTIPLES_OF_5 => {
                    let Some(last_digit) = self.digits.last_mut() else {
                        debug_assert!(false, "`self.digits` should have at least a digit");
                        return;
                    };

                    *last_digit = if *last_digit < 5 { 0 } else { 5 };
                }
                x if x == R::MULTIPLES_OF_25 => {
                    // Extend with zeroes to have the correct trailing digits.
                    self.digits.resize(digits_to_retain as usize, 0);

                    let Some((last_digit, digits)) = self.digits.split_last_mut() else {
                        debug_assert!(false, "`self.digits` should have at least a digit");
                        return;
                    };

                    if let Some(second_last_digit) = digits.last_mut() {
                        let number = *second_last_digit * 10 + *last_digit;

                        // Trailing zeroes will be removed below. We can defer
                        // the deletion to there.
                        (*second_last_digit, *last_digit) = if number < 25 {
                            (0, 0)
                        } else if number < 50 {
                            (2, 5)
                        } else if number < 75 {
                            (5, 0)
                        } else {
                            (7, 5)
                        };
                    } else {
                        // The number has no other digits aside from the last,
                        // making it strictly less than 25.
                        *last_digit = 0;
                    };
                }
                _ => {
                    debug_assert!(false, "INCREMENT should be 1, 2, 5, or 25");
                    return;
                }
            }

            // 3c. Handle the case where `digits` has trailing zeroes after
            // truncating to the previous multiple.
            let position_last_nonzero_digit = self
                .digits
                .iter()
                .rposition(|x| *x != 0)
                .map(|x| x + 1)
                .unwrap_or(0);
            self.digits.truncate(position_last_nonzero_digit);

            // 3d. If `digits` had only trailing zeroes after truncating,
            // reset to zero.
            if self.digits.is_empty() {
                self.magnitude = 0;
            }
        } else {
            // 4. If the rounding position is *above* the leftmost nonzero
            // digit, set to zero
            self.digits.clear();
            self.magnitude = 0;
        }

        #[cfg(debug_assertions)]
        self.check_invariants();
    }

    fn half_ceil_to_increment_internal<R: IncrementLike>(&mut self, position: i16, increment: R) {
        if self.sign == Sign::Negative {
            self.half_trunc_to_increment_internal(position, increment);
            return;
        }

        self.half_expand_to_increment_internal(position, increment);
    }

    fn half_even_to_increment_internal<R: IncrementLike>(&mut self, position: i16, increment: R) {
        let should_expand = match self.half_increment_at_magnitude(position, increment) {
            Ordering::Greater => true,
            Ordering::Less => false,
            Ordering::Equal => match Some(increment) {
                x if x == R::MULTIPLES_OF_1 => {
                    // Expand if odd, truncate if even.
                    self.digit_at(position) & 0x01 == 1
                }
                x if x == R::MULTIPLES_OF_2 => {
                    let current_digit = self.digit_at(position);
                    let previous_digit = self.digit_at_previous_position(position);
                    let full = previous_digit * 10 + current_digit;

                    // This essentially expands to the "even" increments,
                    // or the increments that are in the even places on the
                    // rounding range: [0, 4, 8, 12, 16, 20, ...].
                    // Equivalent to `(full / 2) is odd`.
                    //
                    // Examples:
                    // - 37 should truncate, since 37 % 20 = 17, which truncates to 16.
                    //   37 / 2 = 18, which is even.
                    // - 83 should expand, since 83 % 20 = 3, which expands to 4.
                    //   83 / 2 = 41, which is odd.
                    (full >> 1) & 0x01 == 1
                }
                x if x == R::MULTIPLES_OF_5 => {
                    // Expand 7.5 to 10 and truncate 2.5 to 0.
                    self.digit_at(position) == 7
                }
                x if x == R::MULTIPLES_OF_25 => {
                    let current_digit = self.digit_at(position);
                    let prev_digit = self.digit_at_previous_position(position);
                    let full_number = prev_digit * 10 + current_digit;

                    // Expand `37.5` to 50 and `87.5` to 100.
                    // Truncate `12.5` to 0 and `62.5` to 50.
                    full_number == 37 || full_number == 87
                }
                _ => {
                    debug_assert!(false, "INCREMENT should be 1, 2, 5, or 25");
                    return;
                }
            },
        };

        if should_expand {
            self.expand_to_increment_internal(position, increment);
        } else {
            self.trunc_to_increment_internal(position, increment);
        }
    }

    fn half_expand_to_increment_internal<R: IncrementLike>(&mut self, position: i16, increment: R) {
        // Only truncate if the rounding position is strictly less than the half increment.
        // At the half increment, `half_expand` always expands.
        let should_trunc = self.half_increment_at_magnitude(position, increment) == Ordering::Less;

        if should_trunc {
            self.trunc_to_increment_internal(position, increment);
        } else {
            self.expand_to_increment_internal(position, increment);
        }
    }

    fn half_floor_to_increment_internal<R: IncrementLike>(&mut self, position: i16, increment: R) {
        if self.sign == Sign::Negative {
            self.half_expand_to_increment_internal(position, increment);
            return;
        }

        self.half_trunc_to_increment_internal(position, increment);
    }

    fn half_trunc_to_increment_internal<R: IncrementLike>(&mut self, position: i16, increment: R) {
        // Only expand if the rounding position is strictly greater than the half increment.
        // At the half increment, `half_trunc` always truncates.
        let should_expand =
            self.half_increment_at_magnitude(position, increment) == Ordering::Greater;

        if should_expand {
            self.expand_to_increment_internal(position, increment);
        } else {
            self.trunc_to_increment_internal(position, increment);
        }
    }

    /// Concatenate another `FixedDecimal` into the end of this `FixedDecimal`.
    ///
    /// All nonzero digits in `other` must have lower magnitude than nonzero digits in `self`.
    /// If the two decimals represent overlapping ranges of magnitudes, an `Err` is returned,
    /// containing (self, other).
    ///
    /// The magnitude range of `self` will be increased if `other` covers a larger range.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let integer = FixedDecimal::from(123);
    /// let fraction = FixedDecimal::from(456).multiplied_pow10(-3);
    ///
    /// let result = integer.concatenated_end(fraction).expect("nonoverlapping");
    ///
    /// assert_eq!("123.456", result.to_string());
    /// ```
    pub fn concatenated_end(
        mut self,
        other: FixedDecimal,
    ) -> Result<Self, (FixedDecimal, FixedDecimal)> {
        match self.concatenate_end(other) {
            Ok(()) => Ok(self),
            Err(err) => Err((self, err)),
        }
    }

    /// Concatenate another `FixedDecimal` into the end of this `FixedDecimal`.
    ///
    /// All nonzero digits in `other` must have lower magnitude than nonzero digits in `self`.
    /// If the two decimals represent overlapping ranges of magnitudes, an `Err` is returned,
    /// passing ownership of `other` back to the caller.
    ///
    /// The magnitude range of `self` will be increased if `other` covers a larger range.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::FixedDecimal;
    ///
    /// let mut integer = FixedDecimal::from(123);
    /// let fraction = FixedDecimal::from(456).multiplied_pow10(-3);
    ///
    /// integer.concatenate_end(fraction);
    ///
    /// assert_eq!("123.456", integer.to_string());
    /// ```
    pub fn concatenate_end(&mut self, other: FixedDecimal) -> Result<(), FixedDecimal> {
        let self_right = self.nonzero_magnitude_end();
        let other_left = other.nonzero_magnitude_start();
        if self.is_zero() {
            // Operation will succeed. We can move the digits into self.
            self.digits = other.digits;
            self.magnitude = other.magnitude;
        } else if other.is_zero() {
            // No changes to the digits are necessary.
        } else if self_right <= other_left {
            // Illegal: `other` is not to the right of `self`
            return Err(other);
        } else {
            // Append the digits from other to the end of self
            let inner_zeroes = crate::ops::i16_abs_sub(self_right, other_left) as usize - 1;
            self.append_digits(inner_zeroes, &other.digits);
        }
        self.upper_magnitude = cmp::max(self.upper_magnitude, other.upper_magnitude);
        self.lower_magnitude = cmp::min(self.lower_magnitude, other.lower_magnitude);
        #[cfg(debug_assertions)]
        self.check_invariants();
        Ok(())
    }

    /// Appends a slice of digits to the end of `self.digits` with optional inner zeroes.
    ///
    /// This function does not check invariants.
    fn append_digits(&mut self, inner_zeroes: usize, new_digits: &[u8]) {
        let new_len = self.digits.len() + inner_zeroes;
        self.digits.resize_with(new_len, || 0);
        self.digits.extend_from_slice(new_digits);
    }

    /// Assert that the invariants among struct fields are enforced. Returns true if all are okay.
    /// Call this in any method that mutates the struct fields.
    ///
    /// Example: `debug_assert!(self.check_invariants())`
    #[cfg(debug_assertions)]
    #[allow(clippy::indexing_slicing)]
    fn check_invariants(&self) {
        // magnitude invariants:
        debug_assert!(
            self.upper_magnitude >= self.magnitude,
            "Upper magnitude too small {self:?}"
        );
        debug_assert!(
            self.lower_magnitude <= self.magnitude,
            "Lower magnitude too large {self:?}"
        );
        debug_assert!(
            self.upper_magnitude >= 0,
            "Upper magnitude below zero {self:?}"
        );
        debug_assert!(
            self.lower_magnitude <= 0,
            "Lower magnitude above zero {self:?}",
        );

        // digits invariants:
        debug_assert!(
            self.digits.len() <= (self.magnitude as i32 - self.lower_magnitude as i32 + 1) as usize,
            "{self:?}"
        );
        if !self.digits.is_empty() {
            debug_assert_ne!(self.digits[0], 0, "Starts with a zero {self:?}");
            debug_assert_ne!(
                self.digits[self.digits.len() - 1],
                0,
                "Ends with a zero {self:?}",
            );
        } else {
            debug_assert_eq!(self.magnitude, 0);
        }
    }
}

/// Render the `FixedDecimal` as a string of ASCII digits with a possible decimal point.
///
/// # Examples
///
/// ```
/// # use fixed_decimal::FixedDecimal;
/// # use writeable::assert_writeable_eq;
/// #
/// assert_writeable_eq!(FixedDecimal::from(42), "42");
/// ```
impl writeable::Writeable for FixedDecimal {
    fn write_to<W: fmt::Write + ?Sized>(&self, sink: &mut W) -> fmt::Result {
        match self.sign {
            Sign::Negative => sink.write_char('-')?,
            Sign::Positive => sink.write_char('+')?,
            Sign::None => (),
        }
        for m in self.magnitude_range().rev() {
            if m == -1 {
                sink.write_char('.')?;
            }
            let d = self.digit_at(m);
            sink.write_char((b'0' + d) as char)?;
        }
        Ok(())
    }

    fn writeable_length_hint(&self) -> writeable::LengthHint {
        writeable::LengthHint::exact(1)
            + ((self.upper_magnitude as i32 - self.lower_magnitude as i32) as usize)
            + (self.sign != Sign::None) as usize
            + (self.lower_magnitude < 0) as usize
    }
}

writeable::impl_display_with_writeable!(FixedDecimal);

impl FixedDecimal {
    #[inline]
    /// Parses a [`FixedDecimal`].
    pub fn try_from_str(s: &str) -> Result<Self, ParseError> {
        Self::try_from_utf8(s.as_bytes())
    }

    pub fn try_from_utf8(input_str: &[u8]) -> Result<Self, ParseError> {
        // input_str: the input string
        // no_sign_str: the input string when the sign is removed from it
        if input_str.is_empty() {
            return Err(ParseError::Syntax);
        }
        #[allow(clippy::indexing_slicing)] // The string is not empty.
        let sign = match input_str[0] {
            b'-' => Sign::Negative,
            b'+' => Sign::Positive,
            _ => Sign::None,
        };
        #[allow(clippy::indexing_slicing)] // The string is not empty.
        let no_sign_str = if sign == Sign::None {
            input_str
        } else {
            &input_str[1..]
        };
        if no_sign_str.is_empty() {
            return Err(ParseError::Syntax);
        }
        // Compute length of each string once and store it, so if you use that multiple times,
        // you don't compute it multiple times
        // has_dot: shows if your input has dot in it
        // has_exponent: shows if your input has an exponent in it
        // dot_index: gives the index of dot (after removing the sign) -- equal to length of
        // the no_sign_str if there is no dot
        // exponent_index: gives the index of exponent (after removing the sign) -- equal to length of
        // the no_sign_str if there is no dot
        let mut has_dot = false;
        let mut has_exponent = false;
        let mut dot_index = no_sign_str.len();
        let mut exponent_index = no_sign_str.len();
        // The following loop computes has_dot, dot_index, and also checks to see if all
        // characters are digits and if you have at most one dot
        // Note: Input of format 111_123 is detected as syntax error here
        // Note: Input starting or ending with a dot is detected as syntax error here (Ex: .123, 123.)
        for (i, c) in no_sign_str.iter().enumerate() {
            if *c == b'.' {
                if has_dot || has_exponent {
                    // multiple dots or dots after the exponent
                    return Err(ParseError::Syntax);
                }
                dot_index = i;
                has_dot = true;
                // We do support omitting the leading zero,
                // but not trailing decimal points
                if i == no_sign_str.len() - 1 {
                    return Err(ParseError::Syntax);
                }
            } else if *c == b'e' || *c == b'E' {
                if has_exponent {
                    // multiple exponents
                    return Err(ParseError::Syntax);
                }
                exponent_index = i;
                has_exponent = true;
                if i == 0 || i == no_sign_str.len() - 1 {
                    return Err(ParseError::Syntax);
                }
            } else if *c == b'-' {
                // Allow a single minus sign after the exponent
                if has_exponent && exponent_index == i - 1 {
                    continue;
                } else {
                    return Err(ParseError::Syntax);
                }
            } else if *c < b'0' || *c > b'9' {
                return Err(ParseError::Syntax);
            }
        }

        // The string without the exponent (or sign)
        // We do the bulk of the calculation on this string,
        // and extract the exponent at the end
        #[allow(clippy::indexing_slicing)] // exponent_index comes from enumerate
        let no_exponent_str = &no_sign_str[..exponent_index];

        // If there was no dot, truncate the dot index
        if dot_index > exponent_index {
            dot_index = exponent_index;
        }

        // defining the output dec here and set its sign
        let mut dec = Self {
            sign,
            ..Default::default()
        };

        // no_dot_str_len: shows length of the string after removing the dot
        let mut no_dot_str_len = no_exponent_str.len();
        if has_dot {
            no_dot_str_len -= 1;
        }

        // Computing DecimalFixed.upper_magnitude
        // We support strings like `0.x` and `.x`. The upper magnitude
        // is always one less than the position of the dot, except in the case where
        // the 0 is omitted; when dot_index = 0. We use saturating_sub to set
        // magnitude to 0 in that case.
        let temp_upper_magnitude = dot_index.saturating_sub(1);
        if temp_upper_magnitude > i16::MAX as usize {
            return Err(ParseError::Limit);
        }
        dec.upper_magnitude = temp_upper_magnitude as i16;

        // Computing DecimalFixed.lower_magnitude
        let temp_lower_magnitude = no_dot_str_len - dot_index;
        if temp_lower_magnitude > (i16::MIN as u16) as usize {
            return Err(ParseError::Limit);
        }
        dec.lower_magnitude = (temp_lower_magnitude as i16).wrapping_neg();

        // leftmost_digit: index of the first non-zero digit
        // rightmost_digit: index of the first element after the last non-zero digit
        // Example:
        //     input string    leftmost_digit     rightmost_digit
        //     00123000              2                  5
        //     0.0123000             3                  6
        //     001.23000             2                  6
        //     001230.00             2                  5
        // Compute leftmost_digit
        let leftmost_digit = no_exponent_str
            .iter()
            .position(|c| *c != b'.' && *c != b'0');

        // If the input only has zeros (like 000, 00.0, -00.000) we handle the situation here
        // by returning the dec and don't running the rest of the code
        let leftmost_digit = if let Some(leftmost_digit) = leftmost_digit {
            leftmost_digit
        } else {
            return Ok(dec);
        };

        // Else if the input is not all zeros, we compute its magnitude:
        // Note that we can cast with "as" here because lower and upper magnitude have been checked already
        let mut temp_magnitude = ((dot_index as i32) - (leftmost_digit as i32) - 1i32) as i16;
        if dot_index < leftmost_digit {
            temp_magnitude += 1;
        }
        dec.magnitude = temp_magnitude;

        // Compute the index where the rightmost_digit ends
        let rightmost_digit_end = no_exponent_str
            .iter()
            .rposition(|c| *c != b'.' && *c != b'0')
            .map(|p| p + 1)
            .unwrap_or(no_exponent_str.len());

        // digits_str_len: shows the length of digits (Ex. 0012.8900 --> 4)
        let mut digits_str_len = rightmost_digit_end - leftmost_digit;
        if leftmost_digit < dot_index && dot_index < rightmost_digit_end {
            digits_str_len -= 1;
        }

        // Constructing DecimalFixed.digits
        #[allow(clippy::indexing_slicing)]
        // leftmost_digit  and rightmost_digit_end come from Iterator::position and Iterator::rposition.
        let v: SmallVec<[u8; 8]> = no_exponent_str[leftmost_digit..rightmost_digit_end]
            .iter()
            .filter(|c| **c != b'.')
            .map(|c| c - b'0')
            .collect();

        let v_len = v.len();
        debug_assert_eq!(v_len, digits_str_len);
        dec.digits = v;

        // Extract the exponent part
        if has_exponent {
            let mut pow = 0;
            let mut pos_neg = 1;
            #[allow(clippy::indexing_slicing)]
            // exponent_index is exist, then exponent_index + 1 will equal at most no_sign_str.len().
            for digit in &no_sign_str[exponent_index + 1..] {
                if *digit == b'-' {
                    pos_neg = -1;
                    continue;
                }
                pow *= 10;
                pow += (digit - b'0') as i16;
            }

            dec.multiply_pow10(pos_neg * pow);

            // Clean up magnitude after multiplication
            if dec.magnitude > 0 {
                dec.upper_magnitude = dec.magnitude;
            }
            let neg_mag = dec.magnitude - dec.digits.len() as i16 + 1;
            if neg_mag < 0 {
                dec.lower_magnitude = neg_mag;
            }
        }

        Ok(dec)
    }
}

impl FromStr for FixedDecimal {
    type Err = ParseError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(s)
    }
}

/// Specifies the precision of a floating point value when constructing a FixedDecimal.
///
/// IEEE 754 is a representation of a point on the number line. On the other hand, FixedDecimal
/// specifies not only the point on the number line but also the precision of the number to a
/// specific power of 10. This enum augments a floating-point value with the additional
/// information required by FixedDecimal.
#[non_exhaustive]
#[cfg(feature = "ryu")]
#[derive(Debug, Clone, Copy)]
pub enum FloatPrecision {
    /// Specify that the floating point number is integer-valued.
    ///
    /// If the floating point is not actually integer-valued, an error will be returned.
    Integer,

    /// Specify that the floating point number is precise to a specific power of 10.
    /// The number may be rounded or trailing zeros may be added as necessary.
    Magnitude(i16),

    /// Specify that the floating point number is precise to a specific number of significant digits.
    /// The number may be rounded or trailing zeros may be added as necessary.
    ///
    /// The number requested may not be zero
    SignificantDigits(u8),

    /// Specify that the floating point number is precise to the maximum representable by IEEE.
    ///
    /// This results in a FixedDecimal having enough digits to recover the original floating point
    /// value, with no trailing zeros.
    RoundTrip,
}

#[cfg(feature = "ryu")]
impl FixedDecimal {
    /// Constructs a [`FixedDecimal`] from an f64.
    ///
    /// Since f64 values do not carry a notion of their precision, the second argument to this
    /// function specifies the type of precision associated with the f64. For more information,
    /// see [`FloatPrecision`].
    ///
    /// This function uses `ryu`, which is an efficient double-to-string algorithm, but other
    /// implementations may yield higher performance; for more details, see
    /// [icu4x#166](https://github.com/unicode-org/icu4x/issues/166).
    ///
    /// This function can be made available with the `"ryu"` Cargo feature.
    ///
    /// ```rust
    /// use fixed_decimal::{FixedDecimal, FloatPrecision};
    /// use writeable::assert_writeable_eq;
    ///
    /// let decimal =
    ///     FixedDecimal::try_from_f64(-5.1, FloatPrecision::Magnitude(-2))
    ///         .expect("Finite quantity with limited precision");
    /// assert_writeable_eq!(decimal, "-5.10");
    ///
    /// let decimal =
    ///     FixedDecimal::try_from_f64(0.012345678, FloatPrecision::RoundTrip)
    ///         .expect("Finite quantity");
    /// assert_writeable_eq!(decimal, "0.012345678");
    ///
    /// let decimal =
    ///     FixedDecimal::try_from_f64(12345678000., FloatPrecision::Integer)
    ///         .expect("Finite, integer-valued quantity");
    /// assert_writeable_eq!(decimal, "12345678000");
    /// ```
    ///
    /// Negative zero is supported.
    ///
    /// ```rust
    /// use fixed_decimal::{FixedDecimal, FloatPrecision};
    /// use writeable::assert_writeable_eq;
    ///
    /// // IEEE 754 for floating point defines the sign bit separate
    /// // from the mantissa and exponent, allowing for -0.
    /// let negative_zero =
    ///     FixedDecimal::try_from_f64(-0.0, FloatPrecision::Integer)
    ///         .expect("Negative zero");
    /// assert_writeable_eq!(negative_zero, "-0");
    /// ```
    pub fn try_from_f64(float: f64, precision: FloatPrecision) -> Result<Self, LimitError> {
        let mut decimal = Self::new_from_f64_raw(float)?;
        let n_digits = decimal.digits.len();
        // magnitude of the lowest digit in self.digits
        let lowest_magnitude = decimal.magnitude - n_digits as i16 + 1;
        // ryū will usually tack on a `.0` to integers which gets included when parsing.
        // Explicitly remove it before doing anything else
        if lowest_magnitude >= 0 && decimal.lower_magnitude < 0 {
            decimal.lower_magnitude = 0;
        }
        match precision {
            FloatPrecision::RoundTrip => (),
            FloatPrecision::Integer => {
                if lowest_magnitude < 0 {
                    return Err(LimitError);
                }
            }
            FloatPrecision::Magnitude(mag) => {
                decimal.round(mag);
            }
            FloatPrecision::SignificantDigits(sig) => {
                if sig == 0 {
                    return Err(LimitError);
                }

                let position = decimal.magnitude - (sig as i16) + 1;
                let old_magnitude = decimal.magnitude;
                decimal.round(position);

                // This means the significant digits has been increased by 1.
                if decimal.magnitude > old_magnitude {
                    decimal.lower_magnitude = cmp::min(0, position + 1);
                }
            }
        }
        #[cfg(debug_assertions)]
        decimal.check_invariants();
        Ok(decimal)
    }

    /// Internal function for parsing directly from floats using ryū
    fn new_from_f64_raw(float: f64) -> Result<Self, LimitError> {
        if !float.is_finite() {
            return Err(LimitError);
        }
        // note: this does not heap allocate
        let mut buf = ryu::Buffer::new();
        let formatted = buf.format_finite(float);
        Self::from_str(formatted).map_err(|e| match e {
            ParseError::Limit => LimitError,
            ParseError::Syntax => unreachable!("ryu produces correct syntax"),
        })
    }
}

#[cfg(feature = "ryu")]
#[test]
fn test_float() {
    #[derive(Debug)]
    struct TestCase {
        pub input: f64,
        pub precision: FloatPrecision,
        pub expected: &'static str,
    }
    let cases = [
        TestCase {
            input: 1.234567,
            precision: FloatPrecision::RoundTrip,
            expected: "1.234567",
        },
        TestCase {
            input: 888999.,
            precision: FloatPrecision::RoundTrip,
            expected: "888999",
        },
        // HalfExpand tests
        TestCase {
            input: 1.234567,
            precision: FloatPrecision::Magnitude(-2),
            expected: "1.23",
        },
        TestCase {
            input: 1.235567,
            precision: FloatPrecision::Magnitude(-2),
            expected: "1.24",
        },
        TestCase {
            input: 1.2002,
            precision: FloatPrecision::Magnitude(-3),
            expected: "1.200",
        },
        TestCase {
            input: 888999.,
            precision: FloatPrecision::Magnitude(2),
            expected: "889000",
        },
        TestCase {
            input: 888999.,
            precision: FloatPrecision::Magnitude(4),
            expected: "890000",
        },
        TestCase {
            input: 0.9,
            precision: FloatPrecision::Magnitude(0),
            expected: "1",
        },
        TestCase {
            input: 0.9,
            precision: FloatPrecision::Magnitude(2),
            expected: "00",
        },
        TestCase {
            input: 0.009,
            precision: FloatPrecision::Magnitude(-2),
            expected: "0.01",
        },
        TestCase {
            input: 0.009,
            precision: FloatPrecision::Magnitude(-1),
            expected: "0.0",
        },
        TestCase {
            input: 0.009,
            precision: FloatPrecision::Magnitude(0),
            expected: "0",
        },
        TestCase {
            input: 0.0000009,
            precision: FloatPrecision::Magnitude(0),
            expected: "0",
        },
        TestCase {
            input: 0.0000009,
            precision: FloatPrecision::Magnitude(-7),
            expected: "0.0000009",
        },
        TestCase {
            input: 0.0000009,
            precision: FloatPrecision::Magnitude(-6),
            expected: "0.000001",
        },
        TestCase {
            input: 1.234567,
            precision: FloatPrecision::SignificantDigits(1),
            expected: "1",
        },
        TestCase {
            input: 1.234567,
            precision: FloatPrecision::SignificantDigits(2),
            expected: "1.2",
        },
        TestCase {
            input: 1.234567,
            precision: FloatPrecision::SignificantDigits(4),
            expected: "1.235",
        },
        TestCase {
            input: 1.234567,
            precision: FloatPrecision::SignificantDigits(10),
            expected: "1.234567000",
        },
        TestCase {
            input: 888999.,
            precision: FloatPrecision::SignificantDigits(1),
            expected: "900000",
        },
        TestCase {
            input: 888999.,
            precision: FloatPrecision::SignificantDigits(2),
            expected: "890000",
        },
        TestCase {
            input: 888999.,
            precision: FloatPrecision::SignificantDigits(4),
            expected: "889000",
        },
        TestCase {
            input: 988999.,
            precision: FloatPrecision::SignificantDigits(1),
            expected: "1000000",
        },
        TestCase {
            input: 99888.,
            precision: FloatPrecision::SignificantDigits(1),
            expected: "100000",
        },
        TestCase {
            input: 99888.,
            precision: FloatPrecision::SignificantDigits(2),
            expected: "100000",
        },
        TestCase {
            input: 99888.,
            precision: FloatPrecision::SignificantDigits(3),
            expected: "99900",
        },
        TestCase {
            input: 0.0099,
            precision: FloatPrecision::SignificantDigits(1),
            expected: "0.01",
        },
        TestCase {
            input: 9.9888,
            precision: FloatPrecision::SignificantDigits(1),
            expected: "10",
        },
        TestCase {
            input: 9.9888,
            precision: FloatPrecision::SignificantDigits(2),
            expected: "10",
        },
        TestCase {
            input: 99888.0,
            precision: FloatPrecision::SignificantDigits(1),
            expected: "100000",
        },
        TestCase {
            input: 99888.0,
            precision: FloatPrecision::SignificantDigits(2),
            expected: "100000",
        },
        TestCase {
            input: 9.9888,
            precision: FloatPrecision::SignificantDigits(3),
            expected: "9.99",
        },
    ];

    for case in &cases {
        let dec = FixedDecimal::try_from_f64(case.input, case.precision).unwrap();
        writeable::assert_writeable_eq!(dec, case.expected, "{:?}", case);
    }
}

#[test]
fn test_basic() {
    #[derive(Debug)]
    struct TestCase {
        pub input: isize,
        pub delta: i16,
        pub expected: &'static str,
    }
    let cases = [
        TestCase {
            input: 51423,
            delta: 0,
            expected: "51423",
        },
        TestCase {
            input: 51423,
            delta: -2,
            expected: "514.23",
        },
        TestCase {
            input: 51423,
            delta: -5,
            expected: "0.51423",
        },
        TestCase {
            input: 51423,
            delta: -8,
            expected: "0.00051423",
        },
        TestCase {
            input: 51423,
            delta: 3,
            expected: "51423000",
        },
        TestCase {
            input: 0,
            delta: 0,
            expected: "0",
        },
        TestCase {
            input: 0,
            delta: -2,
            expected: "0.00",
        },
        TestCase {
            input: 0,
            delta: 3,
            expected: "0000",
        },
        TestCase {
            input: 500,
            delta: 0,
            expected: "500",
        },
        TestCase {
            input: 500,
            delta: -1,
            expected: "50.0",
        },
        TestCase {
            input: 500,
            delta: -2,
            expected: "5.00",
        },
        TestCase {
            input: 500,
            delta: -3,
            expected: "0.500",
        },
        TestCase {
            input: 500,
            delta: -4,
            expected: "0.0500",
        },
        TestCase {
            input: 500,
            delta: 3,
            expected: "500000",
        },
        TestCase {
            input: -123,
            delta: 0,
            expected: "-123",
        },
        TestCase {
            input: -123,
            delta: -2,
            expected: "-1.23",
        },
        TestCase {
            input: -123,
            delta: -5,
            expected: "-0.00123",
        },
        TestCase {
            input: -123,
            delta: 3,
            expected: "-123000",
        },
    ];
    for cas in &cases {
        let mut dec: FixedDecimal = cas.input.into();
        // println!("{}", cas.input + 0.01);
        dec.multiply_pow10(cas.delta);
        writeable::assert_writeable_eq!(dec, cas.expected, "{:?}", cas);
    }
}

#[test]
fn test_from_str() {
    #[derive(Debug)]
    struct TestCase {
        pub input_str: &'static str,
        /// The output str, `None` for roundtrip
        pub output_str: Option<&'static str>,
        /// [upper magnitude, upper nonzero magnitude, lower nonzero magnitude, lower magnitude]
        pub magnitudes: [i16; 4],
    }
    let cases = [
        TestCase {
            input_str: "-00123400",
            output_str: None,
            magnitudes: [7, 5, 2, 0],
        },
        TestCase {
            input_str: "+00123400",
            output_str: None,
            magnitudes: [7, 5, 2, 0],
        },
        TestCase {
            input_str: "0.0123400",
            output_str: None,
            magnitudes: [0, -2, -5, -7],
        },
        TestCase {
            input_str: "-00.123400",
            output_str: None,
            magnitudes: [1, -1, -4, -6],
        },
        TestCase {
            input_str: "0012.3400",
            output_str: None,
            magnitudes: [3, 1, -2, -4],
        },
        TestCase {
            input_str: "-0012340.0",
            output_str: None,
            magnitudes: [6, 4, 1, -1],
        },
        TestCase {
            input_str: "1234",
            output_str: None,
            magnitudes: [3, 3, 0, 0],
        },
        TestCase {
            input_str: "0.000000001",
            output_str: None,
            magnitudes: [0, -9, -9, -9],
        },
        TestCase {
            input_str: "0.0000000010",
            output_str: None,
            magnitudes: [0, -9, -9, -10],
        },
        TestCase {
            input_str: "1000000",
            output_str: None,
            magnitudes: [6, 6, 6, 0],
        },
        TestCase {
            input_str: "10000001",
            output_str: None,
            magnitudes: [7, 7, 0, 0],
        },
        TestCase {
            input_str: "123",
            output_str: None,
            magnitudes: [2, 2, 0, 0],
        },
        TestCase {
            input_str: "922337203685477580898230948203840239384.9823094820384023938423424",
            output_str: None,
            magnitudes: [38, 38, -25, -25],
        },
        TestCase {
            input_str: "009223372000.003685477580898230948203840239384000",
            output_str: None,
            magnitudes: [11, 9, -33, -36],
        },
        TestCase {
            input_str: "-009223372000.003685477580898230948203840239384000",
            output_str: None,
            magnitudes: [11, 9, -33, -36],
        },
        TestCase {
            input_str: "0",
            output_str: None,
            magnitudes: [0, 0, 0, 0],
        },
        TestCase {
            input_str: "-0",
            output_str: None,
            magnitudes: [0, 0, 0, 0],
        },
        TestCase {
            input_str: "+0",
            output_str: None,
            magnitudes: [0, 0, 0, 0],
        },
        TestCase {
            input_str: "000",
            output_str: None,
            magnitudes: [2, 0, 0, 0],
        },
        TestCase {
            input_str: "-00.0",
            output_str: None,
            magnitudes: [1, 0, 0, -1],
        },
        // no leading 0 parsing
        TestCase {
            input_str: ".0123400",
            output_str: Some("0.0123400"),
            magnitudes: [0, -2, -5, -7],
        },
        TestCase {
            input_str: ".000000001",
            output_str: Some("0.000000001"),
            magnitudes: [0, -9, -9, -9],
        },
        TestCase {
            input_str: "-.123400",
            output_str: Some("-0.123400"),
            magnitudes: [0, -1, -4, -6],
        },
    ];
    for cas in &cases {
        let fd = FixedDecimal::from_str(cas.input_str).unwrap();
        assert_eq!(
            fd.magnitude_range(),
            cas.magnitudes[3]..=cas.magnitudes[0],
            "{cas:?}"
        );
        assert_eq!(fd.nonzero_magnitude_start(), cas.magnitudes[1], "{cas:?}");
        assert_eq!(fd.nonzero_magnitude_end(), cas.magnitudes[2], "{cas:?}");
        let input_str_roundtrip = fd.to_string();
        let output_str = cas.output_str.unwrap_or(cas.input_str);
        assert_eq!(output_str, input_str_roundtrip, "{cas:?}");
    }
}

#[test]
fn test_from_str_scientific() {
    #[derive(Debug)]
    struct TestCase {
        pub input_str: &'static str,
        pub output: &'static str,
    }
    let cases = [
        TestCase {
            input_str: "-5.4e10",
            output: "-54000000000",
        },
        TestCase {
            input_str: "5.4e-2",
            output: "0.054",
        },
        TestCase {
            input_str: "54.1e-2",
            output: "0.541",
        },
        TestCase {
            input_str: "-541e-2",
            output: "-5.41",
        },
        TestCase {
            input_str: "0.009E10",
            output: "90000000",
        },
        TestCase {
            input_str: "-9000E-10",
            output: "-0.0000009",
        },
    ];
    for cas in &cases {
        let input_str_roundtrip = FixedDecimal::from_str(cas.input_str).unwrap().to_string();
        assert_eq!(cas.output, input_str_roundtrip);
    }
}

#[test]
fn test_isize_limits() {
    for num in &[isize::MAX, isize::MIN] {
        let dec: FixedDecimal = (*num).into();
        let dec_str = dec.to_string();
        assert_eq!(num.to_string(), dec_str);
        assert_eq!(dec, FixedDecimal::from_str(&dec_str).unwrap());
        writeable::assert_writeable_eq!(dec, dec_str);
    }
}

#[test]
fn test_ui128_limits() {
    for num in &[i128::MAX, i128::MIN] {
        let dec: FixedDecimal = (*num).into();
        let dec_str = dec.to_string();
        assert_eq!(num.to_string(), dec_str);
        assert_eq!(dec, FixedDecimal::from_str(&dec_str).unwrap());
        writeable::assert_writeable_eq!(dec, dec_str);
    }
    for num in &[u128::MAX, u128::MIN] {
        let dec: FixedDecimal = (*num).into();
        let dec_str = dec.to_string();
        assert_eq!(num.to_string(), dec_str);
        assert_eq!(dec, FixedDecimal::from_str(&dec_str).unwrap());
        writeable::assert_writeable_eq!(dec, dec_str);
    }
}

#[test]
fn test_upper_magnitude_bounds() {
    let mut dec: FixedDecimal = 98765.into();
    assert_eq!(dec.upper_magnitude, 4);
    dec.multiply_pow10(i16::MAX - 4);
    assert_eq!(dec.upper_magnitude, i16::MAX);
    assert_eq!(dec.nonzero_magnitude_start(), i16::MAX);
    let dec_backup = dec.clone();
    dec.multiply_pow10(1);
    assert!(dec.is_zero());
    assert_ne!(dec, dec_backup, "Value should be unchanged on failure");

    // Checking from_str for dec (which is valid)
    let dec_roundtrip = FixedDecimal::from_str(&dec.to_string()).unwrap();
    assert_eq!(dec, dec_roundtrip);
}

#[test]
fn test_lower_magnitude_bounds() {
    let mut dec: FixedDecimal = 98765.into();
    assert_eq!(dec.lower_magnitude, 0);
    dec.multiply_pow10(i16::MIN);
    assert_eq!(dec.lower_magnitude, i16::MIN);
    assert_eq!(dec.nonzero_magnitude_end(), i16::MIN);
    let dec_backup = dec.clone();
    dec.multiply_pow10(-1);
    assert!(dec.is_zero());
    assert_ne!(dec, dec_backup);

    // Checking from_str for dec (which is valid)
    let dec_roundtrip = FixedDecimal::from_str(&dec.to_string()).unwrap();
    assert_eq!(dec, dec_roundtrip);
}

#[test]
fn test_zero_str_bounds() {
    #[derive(Debug)]
    struct TestCase {
        pub zeros_before_dot: usize,
        pub zeros_after_dot: usize,
        pub expected_err: Option<ParseError>,
    }
    let cases = [
        TestCase {
            zeros_before_dot: i16::MAX as usize + 1,
            zeros_after_dot: 0,
            expected_err: None,
        },
        TestCase {
            zeros_before_dot: i16::MAX as usize,
            zeros_after_dot: 0,
            expected_err: None,
        },
        TestCase {
            zeros_before_dot: i16::MAX as usize + 2,
            zeros_after_dot: 0,
            expected_err: Some(ParseError::Limit),
        },
        TestCase {
            zeros_before_dot: 0,
            zeros_after_dot: i16::MAX as usize + 2,
            expected_err: Some(ParseError::Limit),
        },
        TestCase {
            zeros_before_dot: i16::MAX as usize + 1,
            zeros_after_dot: i16::MAX as usize + 1,
            expected_err: None,
        },
        TestCase {
            zeros_before_dot: i16::MAX as usize + 2,
            zeros_after_dot: i16::MAX as usize + 1,
            expected_err: Some(ParseError::Limit),
        },
        TestCase {
            zeros_before_dot: i16::MAX as usize + 1,
            zeros_after_dot: i16::MAX as usize + 2,
            expected_err: Some(ParseError::Limit),
        },
        TestCase {
            zeros_before_dot: i16::MAX as usize,
            zeros_after_dot: i16::MAX as usize + 2,
            expected_err: Some(ParseError::Limit),
        },
        TestCase {
            zeros_before_dot: i16::MAX as usize,
            zeros_after_dot: i16::MAX as usize,
            expected_err: None,
        },
        TestCase {
            zeros_before_dot: i16::MAX as usize + 1,
            zeros_after_dot: i16::MAX as usize,
            expected_err: None,
        },
    ];
    for cas in &cases {
        let mut input_str = format!("{:0fill$}", 0, fill = cas.zeros_before_dot);
        if cas.zeros_after_dot > 0 {
            input_str.push('.');
            input_str.push_str(&format!("{:0fill$}", 0, fill = cas.zeros_after_dot));
        }
        match FixedDecimal::from_str(&input_str) {
            Ok(dec) => {
                assert_eq!(cas.expected_err, None, "{cas:?}");
                assert_eq!(input_str, dec.to_string(), "{cas:?}");
            }
            Err(err) => {
                assert_eq!(cas.expected_err, Some(err), "{cas:?}");
            }
        }
    }
}

#[test]
fn test_syntax_error() {
    #[derive(Debug)]
    struct TestCase {
        pub input_str: &'static str,
        pub expected_err: Option<ParseError>,
    }
    let cases = [
        TestCase {
            input_str: "-12a34",
            expected_err: Some(ParseError::Syntax),
        },
        TestCase {
            input_str: "0.0123√400",
            expected_err: Some(ParseError::Syntax),
        },
        TestCase {
            input_str: "0.012.3400",
            expected_err: Some(ParseError::Syntax),
        },
        TestCase {
            input_str: "-0-0123400",
            expected_err: Some(ParseError::Syntax),
        },
        TestCase {
            input_str: "0-0123400",
            expected_err: Some(ParseError::Syntax),
        },
        TestCase {
            input_str: "-0.00123400",
            expected_err: None,
        },
        TestCase {
            input_str: "00123400.",
            expected_err: Some(ParseError::Syntax),
        },
        TestCase {
            input_str: "00123400.0",
            expected_err: None,
        },
        TestCase {
            input_str: "123_456",
            expected_err: Some(ParseError::Syntax),
        },
        TestCase {
            input_str: "",
            expected_err: Some(ParseError::Syntax),
        },
        TestCase {
            input_str: "-",
            expected_err: Some(ParseError::Syntax),
        },
        TestCase {
            input_str: "+",
            expected_err: Some(ParseError::Syntax),
        },
        TestCase {
            input_str: "-1",
            expected_err: None,
        },
    ];
    for cas in &cases {
        match FixedDecimal::from_str(cas.input_str) {
            Ok(dec) => {
                assert_eq!(cas.expected_err, None, "{cas:?}");
                assert_eq!(cas.input_str, dec.to_string(), "{cas:?}");
            }
            Err(err) => {
                assert_eq!(cas.expected_err, Some(err), "{cas:?}");
            }
        }
    }
}

#[test]
fn test_pad() {
    let mut dec = FixedDecimal::from_str("-0.42").unwrap();
    assert_eq!("-0.42", dec.to_string());

    dec.pad_start(1);
    assert_eq!("-0.42", dec.to_string());

    dec.pad_start(4);
    assert_eq!("-0000.42", dec.to_string());

    dec.pad_start(2);
    assert_eq!("-00.42", dec.to_string());
}

#[test]
fn test_sign_display() {
    use SignDisplay::*;
    let positive_nonzero = FixedDecimal::from(163);
    let negative_nonzero = FixedDecimal::from(-163);
    let positive_zero = FixedDecimal::from(0);
    let negative_zero = FixedDecimal::from(0).with_sign(Sign::Negative);
    assert_eq!(
        "163",
        positive_nonzero.clone().with_sign_display(Auto).to_string()
    );
    assert_eq!(
        "-163",
        negative_nonzero.clone().with_sign_display(Auto).to_string()
    );
    assert_eq!(
        "0",
        positive_zero.clone().with_sign_display(Auto).to_string()
    );
    assert_eq!(
        "-0",
        negative_zero.clone().with_sign_display(Auto).to_string()
    );
    assert_eq!(
        "+163",
        positive_nonzero
            .clone()
            .with_sign_display(Always)
            .to_string()
    );
    assert_eq!(
        "-163",
        negative_nonzero
            .clone()
            .with_sign_display(Always)
            .to_string()
    );
    assert_eq!(
        "+0",
        positive_zero.clone().with_sign_display(Always).to_string()
    );
    assert_eq!(
        "-0",
        negative_zero.clone().with_sign_display(Always).to_string()
    );
    assert_eq!(
        "163",
        positive_nonzero
            .clone()
            .with_sign_display(Never)
            .to_string()
    );
    assert_eq!(
        "163",
        negative_nonzero
            .clone()
            .with_sign_display(Never)
            .to_string()
    );
    assert_eq!(
        "0",
        positive_zero.clone().with_sign_display(Never).to_string()
    );
    assert_eq!(
        "0",
        negative_zero.clone().with_sign_display(Never).to_string()
    );
    assert_eq!(
        "+163",
        positive_nonzero
            .clone()
            .with_sign_display(ExceptZero)
            .to_string()
    );
    assert_eq!(
        "-163",
        negative_nonzero
            .clone()
            .with_sign_display(ExceptZero)
            .to_string()
    );
    assert_eq!(
        "0",
        positive_zero
            .clone()
            .with_sign_display(ExceptZero)
            .to_string()
    );
    assert_eq!(
        "0",
        negative_zero
            .clone()
            .with_sign_display(ExceptZero)
            .to_string()
    );
    assert_eq!(
        "163",
        positive_nonzero.with_sign_display(Negative).to_string()
    );
    assert_eq!(
        "-163",
        negative_nonzero.with_sign_display(Negative).to_string()
    );
    assert_eq!("0", positive_zero.with_sign_display(Negative).to_string());
    assert_eq!("0", negative_zero.with_sign_display(Negative).to_string());
}

#[test]
fn test_set_max_position() {
    let mut dec = FixedDecimal::from(1000);
    assert_eq!("1000", dec.to_string());

    dec.set_max_position(2);
    assert_eq!("00", dec.to_string());

    dec.set_max_position(0);
    assert_eq!("0", dec.to_string());

    dec.set_max_position(3);
    assert_eq!("000", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.456").unwrap();
    assert_eq!("0.456", dec.to_string());

    dec.set_max_position(0);
    assert_eq!("0.456", dec.to_string());

    dec.set_max_position(-1);
    assert_eq!("0.056", dec.to_string());

    dec.set_max_position(-2);
    assert_eq!("0.006", dec.to_string());

    dec.set_max_position(-3);
    assert_eq!("0.000", dec.to_string());

    dec.set_max_position(-4);
    assert_eq!("0.0000", dec.to_string());

    let mut dec = FixedDecimal::from_str("100.01").unwrap();
    dec.set_max_position(1);
    assert_eq!("0.01", dec.to_string());
}

#[test]
fn test_pad_start_bounds() {
    let mut dec = FixedDecimal::from_str("299792.458").unwrap();
    let max_integer_digits = i16::MAX as usize + 1;

    dec.pad_start(i16::MAX - 1);
    assert_eq!(
        max_integer_digits - 2,
        dec.to_string().split_once('.').unwrap().0.len()
    );

    dec.pad_start(i16::MAX);
    assert_eq!(
        max_integer_digits - 1,
        dec.to_string().split_once('.').unwrap().0.len()
    );
}

#[test]
fn test_pad_end_bounds() {
    let mut dec = FixedDecimal::from_str("299792.458").unwrap();
    let max_fractional_digits = -(i16::MIN as isize) as usize;

    dec.pad_end(i16::MIN + 1);
    assert_eq!(
        max_fractional_digits - 1,
        dec.to_string().split_once('.').unwrap().1.len()
    );

    dec.pad_end(i16::MIN);
    assert_eq!(
        max_fractional_digits,
        dec.to_string().split_once('.').unwrap().1.len()
    );
}

#[test]
fn test_rounding() {
    pub(crate) use std::str::FromStr;

    // Test Ceil
    let mut dec = FixedDecimal::from_str("3.234").unwrap();
    dec.ceil(0);
    assert_eq!("4", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.222").unwrap();
    dec.ceil(-1);
    assert_eq!("2.3", dec.to_string());

    let mut dec = FixedDecimal::from_str("22.222").unwrap();
    dec.ceil(-2);
    assert_eq!("22.23", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.ceil(-2);
    assert_eq!("100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.ceil(-5);
    assert_eq!("99.99900", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.ceil(-5);
    assert_eq!("-99.99900", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.ceil(-2);
    assert_eq!("-99.99", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.ceil(4);
    assert_eq!("10000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.ceil(4);
    assert_eq!("-0000", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.ceil(-1);
    assert_eq!("0.1", dec.to_string());

    let mut dec = FixedDecimal::from_str("-0.009").unwrap();
    dec.ceil(-1);
    assert_eq!("-0.0", dec.to_string());

    // Test Half Ceil
    let mut dec = FixedDecimal::from_str("3.234").unwrap();
    dec.round_with_mode(0, RoundingMode::HalfCeil);
    assert_eq!("3", dec.to_string());

    let mut dec = FixedDecimal::from_str("3.534").unwrap();
    dec.round_with_mode(0, RoundingMode::HalfCeil);
    assert_eq!("4", dec.to_string());

    let mut dec = FixedDecimal::from_str("3.934").unwrap();
    dec.round_with_mode(0, RoundingMode::HalfCeil);
    assert_eq!("4", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.222").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfCeil);
    assert_eq!("2.2", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.44").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfCeil);
    assert_eq!("2.4", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.45").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfCeil);
    assert_eq!("2.5", dec.to_string());

    let mut dec = FixedDecimal::from_str("-2.44").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfCeil);
    assert_eq!("-2.4", dec.to_string());

    let mut dec = FixedDecimal::from_str("-2.45").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfCeil);
    assert_eq!("-2.4", dec.to_string());

    let mut dec = FixedDecimal::from_str("22.222").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfCeil);
    assert_eq!("22.22", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfCeil);
    assert_eq!("100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.round_with_mode(-5, RoundingMode::HalfCeil);
    assert_eq!("99.99900", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode(-5, RoundingMode::HalfCeil);
    assert_eq!("-99.99900", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfCeil);
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.round_with_mode(4, RoundingMode::HalfCeil);
    assert_eq!("0000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode(4, RoundingMode::HalfCeil);
    assert_eq!("-0000", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfCeil);
    assert_eq!("0.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("-0.009").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfCeil);
    assert_eq!("-0.0", dec.to_string());

    // Test Floor
    let mut dec = FixedDecimal::from_str("3.234").unwrap();
    dec.floor(0);
    assert_eq!("3", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.222").unwrap();
    dec.floor(-1);
    assert_eq!("2.2", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.floor(-2);
    assert_eq!("99.99", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.floor(-10);
    assert_eq!("99.9990000000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.floor(-10);
    assert_eq!("-99.9990000000", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.floor(10);
    assert_eq!("0000000000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.floor(10);
    assert_eq!("-10000000000", dec.to_string());

    // Test Half Floor
    let mut dec = FixedDecimal::from_str("3.234").unwrap();
    dec.round_with_mode(0, RoundingMode::HalfFloor);
    assert_eq!("3", dec.to_string());

    let mut dec = FixedDecimal::from_str("3.534").unwrap();
    dec.round_with_mode(0, RoundingMode::HalfFloor);
    assert_eq!("4", dec.to_string());

    let mut dec = FixedDecimal::from_str("3.934").unwrap();
    dec.round_with_mode(0, RoundingMode::HalfFloor);
    assert_eq!("4", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.222").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfFloor);
    assert_eq!("2.2", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.44").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfFloor);
    assert_eq!("2.4", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.45").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfFloor);
    assert_eq!("2.4", dec.to_string());

    let mut dec = FixedDecimal::from_str("-2.44").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfFloor);
    assert_eq!("-2.4", dec.to_string());

    let mut dec = FixedDecimal::from_str("-2.45").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfFloor);
    assert_eq!("-2.5", dec.to_string());

    let mut dec = FixedDecimal::from_str("22.222").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfFloor);
    assert_eq!("22.22", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfFloor);
    assert_eq!("100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.round_with_mode(-5, RoundingMode::HalfFloor);
    assert_eq!("99.99900", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode(-5, RoundingMode::HalfFloor);
    assert_eq!("-99.99900", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfFloor);
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.round_with_mode(4, RoundingMode::HalfFloor);
    assert_eq!("0000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode(4, RoundingMode::HalfFloor);
    assert_eq!("-0000", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfFloor);
    assert_eq!("0.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("-0.009").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfFloor);
    assert_eq!("-0.0", dec.to_string());

    // Test Truncate Right
    let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    dec.trunc(-5);
    assert_eq!("4235.97000", dec.to_string());

    dec.trunc(-1);
    assert_eq!("4235.9", dec.to_string());

    dec.trunc(0);
    assert_eq!("4235", dec.to_string());

    dec.trunc(1);
    assert_eq!("4230", dec.to_string());

    dec.trunc(5);
    assert_eq!("00000", dec.to_string());

    dec.trunc(2);
    assert_eq!("00000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.trunc(-2);
    assert_eq!("-99.99", dec.to_string());

    let mut dec = FixedDecimal::from_str("1234.56").unwrap();
    dec.trunc(-1);
    assert_eq!("1234.5", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.trunc(-1);
    assert_eq!("0.0", dec.to_string());

    // Test trunced
    let dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    assert_eq!("4235.97000", dec.clone().trunced(-5).to_string());

    assert_eq!("4230", dec.clone().trunced(1).to_string());

    assert_eq!("4200", dec.clone().trunced(2).to_string());

    assert_eq!("00000", dec.trunced(5).to_string());

    //Test expand
    let mut dec = FixedDecimal::from_str("3.234").unwrap();
    dec.expand(0);
    assert_eq!("4", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.222").unwrap();
    dec.expand(-1);
    assert_eq!("2.3", dec.to_string());

    let mut dec = FixedDecimal::from_str("22.222").unwrap();
    dec.expand(-2);
    assert_eq!("22.23", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.expand(-2);
    assert_eq!("100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.expand(-5);
    assert_eq!("99.99900", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.expand(-5);
    assert_eq!("-99.99900", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.expand(-2);
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.expand(4);
    assert_eq!("10000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.expand(4);
    assert_eq!("-10000", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.expand(-1);
    assert_eq!("0.1", dec.to_string());

    let mut dec = FixedDecimal::from_str("-0.009").unwrap();
    dec.expand(-1);
    assert_eq!("-0.1", dec.to_string());

    let mut dec = FixedDecimal::from_str("3.954").unwrap();
    dec.expand(0);
    assert_eq!("4", dec.to_string());

    // Test half_expand
    let mut dec = FixedDecimal::from_str("3.234").unwrap();
    dec.round_with_mode(0, RoundingMode::HalfExpand);
    assert_eq!("3", dec.to_string());

    let mut dec = FixedDecimal::from_str("3.534").unwrap();
    dec.round_with_mode(0, RoundingMode::HalfExpand);
    assert_eq!("4", dec.to_string());

    let mut dec = FixedDecimal::from_str("3.934").unwrap();
    dec.round_with_mode(0, RoundingMode::HalfExpand);
    assert_eq!("4", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.222").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfExpand);
    assert_eq!("2.2", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.44").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfExpand);
    assert_eq!("2.4", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.45").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfExpand);
    assert_eq!("2.5", dec.to_string());

    let mut dec = FixedDecimal::from_str("-2.44").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfExpand);
    assert_eq!("-2.4", dec.to_string());

    let mut dec = FixedDecimal::from_str("-2.45").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfExpand);
    assert_eq!("-2.5", dec.to_string());

    let mut dec = FixedDecimal::from_str("22.222").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfExpand);
    assert_eq!("22.22", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfExpand);
    assert_eq!("100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.round_with_mode(-5, RoundingMode::HalfExpand);
    assert_eq!("99.99900", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode(-5, RoundingMode::HalfExpand);
    assert_eq!("-99.99900", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfExpand);
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("99.999").unwrap();
    dec.round_with_mode(4, RoundingMode::HalfExpand);
    assert_eq!("0000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode(4, RoundingMode::HalfExpand);
    assert_eq!("-0000", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfExpand);
    assert_eq!("0.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("-0.009").unwrap();
    dec.round_with_mode(-1, RoundingMode::HalfExpand);
    assert_eq!("-0.0", dec.to_string());

    // Test specific cases
    let mut dec = FixedDecimal::from_str("1.108").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfEven);
    assert_eq!("1.11", dec.to_string());

    let mut dec = FixedDecimal::from_str("1.108").unwrap();
    dec.expand(-2);
    assert_eq!("1.11", dec.to_string());

    let mut dec = FixedDecimal::from_str("1.108").unwrap();
    dec.trunc(-2);
    assert_eq!("1.10", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.78536913177").unwrap();
    dec.round_with_mode(-2, RoundingMode::HalfEven);
    assert_eq!("2.79", dec.to_string());
}

#[test]
fn test_concatenate() {
    #[derive(Debug)]
    struct TestCase {
        pub input_1: &'static str,
        pub input_2: &'static str,
        pub expected: Option<&'static str>,
    }
    let cases = [
        TestCase {
            input_1: "123",
            input_2: "0.456",
            expected: Some("123.456"),
        },
        TestCase {
            input_1: "0.456",
            input_2: "123",
            expected: None,
        },
        TestCase {
            input_1: "123",
            input_2: "0.0456",
            expected: Some("123.0456"),
        },
        TestCase {
            input_1: "0.0456",
            input_2: "123",
            expected: None,
        },
        TestCase {
            input_1: "100",
            input_2: "0.456",
            expected: Some("100.456"),
        },
        TestCase {
            input_1: "0.456",
            input_2: "100",
            expected: None,
        },
        TestCase {
            input_1: "100",
            input_2: "0.001",
            expected: Some("100.001"),
        },
        TestCase {
            input_1: "0.001",
            input_2: "100",
            expected: None,
        },
        TestCase {
            input_1: "123000",
            input_2: "456",
            expected: Some("123456"),
        },
        TestCase {
            input_1: "456",
            input_2: "123000",
            expected: None,
        },
        TestCase {
            input_1: "5",
            input_2: "5",
            expected: None,
        },
        TestCase {
            input_1: "120",
            input_2: "25",
            expected: None,
        },
        TestCase {
            input_1: "1.1",
            input_2: "0.2",
            expected: None,
        },
        TestCase {
            input_1: "0",
            input_2: "222",
            expected: Some("222"),
        },
        TestCase {
            input_1: "222",
            input_2: "0",
            expected: Some("222"),
        },
        TestCase {
            input_1: "0",
            input_2: "0",
            expected: Some("0"),
        },
        TestCase {
            input_1: "000",
            input_2: "0",
            expected: Some("000"),
        },
        TestCase {
            input_1: "0.00",
            input_2: "0",
            expected: Some("0.00"),
        },
    ];
    for cas in &cases {
        let fd1 = FixedDecimal::from_str(cas.input_1).unwrap();
        let fd2 = FixedDecimal::from_str(cas.input_2).unwrap();
        match fd1.concatenated_end(fd2) {
            Ok(fd) => {
                assert_eq!(cas.expected, Some(fd.to_string().as_str()), "{cas:?}");
            }
            Err(_) => {
                assert!(cas.expected.is_none(), "{cas:?}");
            }
        }
    }
}

#[test]
fn test_rounding_increment() {
    // Test Truncate Right
    let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    dec.round_with_mode_and_increment(-2, RoundingMode::Trunc, RoundingIncrement::MultiplesOf2);
    assert_eq!("4235.96", dec.to_string());

    dec.round_with_mode_and_increment(-1, RoundingMode::Trunc, RoundingIncrement::MultiplesOf5);
    assert_eq!("4235.5", dec.to_string());

    dec.round_with_mode_and_increment(0, RoundingMode::Trunc, RoundingIncrement::MultiplesOf25);
    assert_eq!("4225", dec.to_string());

    dec.round_with_mode_and_increment(5, RoundingMode::Trunc, RoundingIncrement::MultiplesOf5);
    assert_eq!("00000", dec.to_string());

    dec.round_with_mode_and_increment(2, RoundingMode::Trunc, RoundingIncrement::MultiplesOf2);
    assert_eq!("00000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Trunc, RoundingIncrement::MultiplesOf25);
    assert_eq!("-99.75", dec.to_string());

    let mut dec = FixedDecimal::from_str("1234.56").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::Trunc, RoundingIncrement::MultiplesOf2);
    assert_eq!("1234.4", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::Trunc, RoundingIncrement::MultiplesOf5);
    assert_eq!("0.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.60").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Trunc, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.40").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Trunc, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.25", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.7000000099").unwrap();
    dec.round_with_mode_and_increment(-3, RoundingMode::Trunc, RoundingIncrement::MultiplesOf2);
    assert_eq!("0.700", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Trunc, RoundingIncrement::MultiplesOf25);
    assert_eq!("0", dec.to_string());

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Trunc,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(6).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Trunc,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(5).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(70).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Trunc,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(50).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::Trunc,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(6).multiplied_pow10(i16::MAX), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::Trunc,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(5).multiplied_pow10(i16::MAX), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::Trunc,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(0).multiplied_pow10(i16::MAX), dec);

    // Test Expand
    let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    dec.round_with_mode_and_increment(-2, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("4235.98", dec.to_string());

    dec.round_with_mode_and_increment(-1, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("4236.0", dec.to_string());

    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf25);
    assert_eq!("4250", dec.to_string());

    dec.round_with_mode_and_increment(5, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("500000", dec.to_string());

    dec.round_with_mode_and_increment(2, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("500000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Expand, RoundingIncrement::MultiplesOf25);
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("1234.56").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("1234.6", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("0.5", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.60").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Expand, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.75", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.40").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Expand, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.7000000099").unwrap();
    dec.round_with_mode_and_increment(-3, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("0.702", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf25);
    assert_eq!("25", dec.to_string());

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Expand,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(8).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Expand,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(10).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(70).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Expand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(75).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::Expand,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(8).multiplied_pow10(i16::MAX), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::Expand,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(0).multiplied_pow10(i16::MAX), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::Expand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(0).multiplied_pow10(i16::MAX), dec);

    // Test Half Truncate Right
    let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    dec.round_with_mode_and_increment(-2, RoundingMode::HalfTrunc, RoundingIncrement::MultiplesOf2);
    assert_eq!("4235.96", dec.to_string());

    dec.round_with_mode_and_increment(-1, RoundingMode::HalfTrunc, RoundingIncrement::MultiplesOf5);
    assert_eq!("4236.0", dec.to_string());

    dec.round_with_mode_and_increment(0, RoundingMode::HalfTrunc, RoundingIncrement::MultiplesOf25);
    assert_eq!("4225", dec.to_string());

    dec.round_with_mode_and_increment(5, RoundingMode::HalfTrunc, RoundingIncrement::MultiplesOf5);
    assert_eq!("00000", dec.to_string());

    dec.round_with_mode_and_increment(2, RoundingMode::HalfTrunc, RoundingIncrement::MultiplesOf2);
    assert_eq!("00000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfTrunc,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("1234.56").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::HalfTrunc, RoundingIncrement::MultiplesOf2);
    assert_eq!("1234.6", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::HalfTrunc, RoundingIncrement::MultiplesOf5);
    assert_eq!("0.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.60").unwrap();
    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfTrunc,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.40").unwrap();
    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfTrunc,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.7000000099").unwrap();
    dec.round_with_mode_and_increment(-3, RoundingMode::HalfTrunc, RoundingIncrement::MultiplesOf2);
    assert_eq!("0.700", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::HalfTrunc, RoundingIncrement::MultiplesOf25);
    assert_eq!("0", dec.to_string());

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfTrunc,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(6).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfTrunc,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(10).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(70).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfTrunc,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(75).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::HalfTrunc,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(6).multiplied_pow10(i16::MAX), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::HalfTrunc,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(5).multiplied_pow10(i16::MAX), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::HalfTrunc,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(0).multiplied_pow10(i16::MAX), dec);

    // Test Half Expand
    let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!("4235.98", dec.to_string());

    dec.round_with_mode_and_increment(
        -1,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!("4236.0", dec.to_string());

    dec.round_with_mode_and_increment(
        0,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("4225", dec.to_string());

    dec.round_with_mode_and_increment(5, RoundingMode::HalfExpand, RoundingIncrement::MultiplesOf5);
    assert_eq!("00000", dec.to_string());

    dec.round_with_mode_and_increment(2, RoundingMode::HalfExpand, RoundingIncrement::MultiplesOf2);
    assert_eq!("00000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("1234.56").unwrap();
    dec.round_with_mode_and_increment(
        -1,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!("1234.6", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode_and_increment(
        -1,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!("0.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.60").unwrap();
    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.40").unwrap();
    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.7000000099").unwrap();
    dec.round_with_mode_and_increment(
        -3,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!("0.700", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(
        0,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("0", dec.to_string());

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(8).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(10).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(70).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(75).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(8).multiplied_pow10(i16::MAX), dec);

    // Test Ceil
    let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    dec.round_with_mode_and_increment(-2, RoundingMode::Ceil, RoundingIncrement::MultiplesOf2);
    assert_eq!("4235.98", dec.to_string());

    dec.round_with_mode_and_increment(-1, RoundingMode::Ceil, RoundingIncrement::MultiplesOf5);
    assert_eq!("4236.0", dec.to_string());

    dec.round_with_mode_and_increment(0, RoundingMode::Ceil, RoundingIncrement::MultiplesOf25);
    assert_eq!("4250", dec.to_string());

    dec.round_with_mode_and_increment(5, RoundingMode::Ceil, RoundingIncrement::MultiplesOf5);
    assert_eq!("500000", dec.to_string());

    dec.round_with_mode_and_increment(2, RoundingMode::Ceil, RoundingIncrement::MultiplesOf2);
    assert_eq!("500000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Ceil, RoundingIncrement::MultiplesOf25);
    assert_eq!("-99.75", dec.to_string());

    let mut dec = FixedDecimal::from_str("1234.56").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::Ceil, RoundingIncrement::MultiplesOf2);
    assert_eq!("1234.6", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::Ceil, RoundingIncrement::MultiplesOf5);
    assert_eq!("0.5", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.60").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Ceil, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.75", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.40").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Ceil, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.7000000099").unwrap();
    dec.round_with_mode_and_increment(-3, RoundingMode::Ceil, RoundingIncrement::MultiplesOf2);
    assert_eq!("0.702", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Ceil, RoundingIncrement::MultiplesOf25);
    assert_eq!("25", dec.to_string());

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Ceil,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(8).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Ceil,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(10).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(70).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Ceil,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(75).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::Ceil,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(8).multiplied_pow10(i16::MAX), dec);

    // Test Half Ceil
    let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    dec.round_with_mode_and_increment(-2, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf2);
    assert_eq!("4235.98", dec.to_string());

    dec.round_with_mode_and_increment(-1, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf5);
    assert_eq!("4236.0", dec.to_string());

    dec.round_with_mode_and_increment(0, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf25);
    assert_eq!("4225", dec.to_string());

    dec.round_with_mode_and_increment(5, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf5);
    assert_eq!("00000", dec.to_string());

    dec.round_with_mode_and_increment(2, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf2);
    assert_eq!("00000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf25);
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("1234.56").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf2);
    assert_eq!("1234.6", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf5);
    assert_eq!("0.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.60").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.40").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.7000000099").unwrap();
    dec.round_with_mode_and_increment(-3, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf2);
    assert_eq!("0.700", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf25);
    assert_eq!("0", dec.to_string());

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfCeil,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(8).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfCeil,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(10).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(70).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfCeil,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(75).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::HalfCeil,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(8).multiplied_pow10(i16::MAX), dec);

    // Test Floor
    let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    dec.round_with_mode_and_increment(-2, RoundingMode::Floor, RoundingIncrement::MultiplesOf2);
    assert_eq!("4235.96", dec.to_string());

    dec.round_with_mode_and_increment(-1, RoundingMode::Floor, RoundingIncrement::MultiplesOf5);
    assert_eq!("4235.5", dec.to_string());

    dec.round_with_mode_and_increment(0, RoundingMode::Floor, RoundingIncrement::MultiplesOf25);
    assert_eq!("4225", dec.to_string());

    dec.round_with_mode_and_increment(5, RoundingMode::Floor, RoundingIncrement::MultiplesOf5);
    assert_eq!("00000", dec.to_string());

    dec.round_with_mode_and_increment(2, RoundingMode::Floor, RoundingIncrement::MultiplesOf2);
    assert_eq!("00000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Floor, RoundingIncrement::MultiplesOf25);
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("1234.56").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::Floor, RoundingIncrement::MultiplesOf2);
    assert_eq!("1234.4", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::Floor, RoundingIncrement::MultiplesOf5);
    assert_eq!("0.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.60").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Floor, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.40").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Floor, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.25", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.7000000099").unwrap();
    dec.round_with_mode_and_increment(-3, RoundingMode::Floor, RoundingIncrement::MultiplesOf2);
    assert_eq!("0.700", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Floor, RoundingIncrement::MultiplesOf25);
    assert_eq!("0", dec.to_string());

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Floor,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(6).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Floor,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(5).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(70).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::Floor,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(50).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::Floor,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(6).multiplied_pow10(i16::MAX), dec);

    // Test Half Floor
    let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    dec.round_with_mode_and_increment(-2, RoundingMode::HalfFloor, RoundingIncrement::MultiplesOf2);
    assert_eq!("4235.96", dec.to_string());

    dec.round_with_mode_and_increment(-1, RoundingMode::HalfFloor, RoundingIncrement::MultiplesOf5);
    assert_eq!("4236.0", dec.to_string());

    dec.round_with_mode_and_increment(0, RoundingMode::HalfFloor, RoundingIncrement::MultiplesOf25);
    assert_eq!("4225", dec.to_string());

    dec.round_with_mode_and_increment(5, RoundingMode::HalfFloor, RoundingIncrement::MultiplesOf5);
    assert_eq!("00000", dec.to_string());

    dec.round_with_mode_and_increment(2, RoundingMode::HalfFloor, RoundingIncrement::MultiplesOf2);
    assert_eq!("00000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfFloor,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("1234.56").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::HalfFloor, RoundingIncrement::MultiplesOf2);
    assert_eq!("1234.6", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::HalfFloor, RoundingIncrement::MultiplesOf5);
    assert_eq!("0.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.60").unwrap();
    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfFloor,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.40").unwrap();
    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfFloor,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.7000000099").unwrap();
    dec.round_with_mode_and_increment(-3, RoundingMode::HalfFloor, RoundingIncrement::MultiplesOf2);
    assert_eq!("0.700", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::HalfFloor, RoundingIncrement::MultiplesOf25);
    assert_eq!("0", dec.to_string());

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfFloor,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(6).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfFloor,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(10).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(70).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfFloor,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(75).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::HalfFloor,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(6).multiplied_pow10(i16::MAX), dec);

    // Test Half Even
    let mut dec = FixedDecimal::from(4235970).multiplied_pow10(-3);
    assert_eq!("4235.970", dec.to_string());

    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("4235.96", dec.to_string());

    dec.round_with_mode_and_increment(-1, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf5);
    assert_eq!("4236.0", dec.to_string());

    dec.round_with_mode_and_increment(0, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf25);
    assert_eq!("4225", dec.to_string());

    dec.round_with_mode_and_increment(5, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf5);
    assert_eq!("00000", dec.to_string());

    dec.round_with_mode_and_increment(2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("00000", dec.to_string());

    let mut dec = FixedDecimal::from_str("-99.999").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf25);
    assert_eq!("-100.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("1234.56").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("1234.6", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.009").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf5);
    assert_eq!("0.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.60").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.40").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.7000000099").unwrap();
    dec.round_with_mode_and_increment(-3, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("0.700", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf25);
    assert_eq!("0", dec.to_string());

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfEven,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(8).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfEven,
        RoundingIncrement::MultiplesOf5,
    );
    assert_eq!(FixedDecimal::from(10).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(70).multiplied_pow10(i16::MIN);
    dec.round_with_mode_and_increment(
        i16::MIN,
        RoundingMode::HalfEven,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(75).multiplied_pow10(i16::MIN), dec);

    let mut dec = FixedDecimal::from(7).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::HalfEven,
        RoundingIncrement::MultiplesOf2,
    );
    assert_eq!(FixedDecimal::from(8).multiplied_pow10(i16::MAX), dec);

    // Test specific cases
    let mut dec = FixedDecimal::from_str("1.108").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("1.12", dec.to_string());

    let mut dec = FixedDecimal::from_str("1.108").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("1.15", dec.to_string());

    let mut dec = FixedDecimal::from_str("1.108").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Expand, RoundingIncrement::MultiplesOf25);
    assert_eq!("1.25", dec.to_string());

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MAX - 1);
    dec.round_with_mode_and_increment(
        i16::MAX - 1,
        RoundingMode::Expand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(25).multiplied_pow10(i16::MAX - 1), dec);

    let mut dec = FixedDecimal::from(9).multiplied_pow10(i16::MAX);
    dec.round_with_mode_and_increment(
        i16::MAX,
        RoundingMode::Expand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!(FixedDecimal::from(0).multiplied_pow10(i16::MAX), dec);

    let mut dec = FixedDecimal::from_str("0").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("0", dec.to_string());

    let mut dec = FixedDecimal::from_str("0").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("0", dec.to_string());

    let mut dec = FixedDecimal::from_str("0").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf25);
    assert_eq!("0", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("2", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("5", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf25);
    assert_eq!("25", dec.to_string());

    let mut dec = FixedDecimal::from_str("1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("2", dec.to_string());

    let mut dec = FixedDecimal::from_str("1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("5", dec.to_string());

    let mut dec = FixedDecimal::from_str("1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf25);
    assert_eq!("25", dec.to_string());

    let mut dec = FixedDecimal::from_str("2").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("2", dec.to_string());

    let mut dec = FixedDecimal::from_str("2").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("5", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("4", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("5", dec.to_string());

    let mut dec = FixedDecimal::from_str("4").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("4", dec.to_string());

    let mut dec = FixedDecimal::from_str("4").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("5", dec.to_string());

    let mut dec = FixedDecimal::from_str("4.1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("6", dec.to_string());

    let mut dec = FixedDecimal::from_str("4.1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("5", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("6", dec.to_string());

    let mut dec = FixedDecimal::from_str("5").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("5", dec.to_string());

    let mut dec = FixedDecimal::from_str("5.1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("6", dec.to_string());

    let mut dec = FixedDecimal::from_str("5.1").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("10", dec.to_string());

    let mut dec = FixedDecimal::from_str("6").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf2);
    assert_eq!("6", dec.to_string());

    let mut dec = FixedDecimal::from_str("6").unwrap();
    dec.round_with_mode_and_increment(0, RoundingMode::Expand, RoundingIncrement::MultiplesOf5);
    assert_eq!("10", dec.to_string());

    let mut dec = FixedDecimal::from_str("0.50").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::Expand, RoundingIncrement::MultiplesOf25);
    assert_eq!("0.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("1.1025").unwrap();
    dec.round_with_mode_and_increment(-3, RoundingMode::HalfTrunc, RoundingIncrement::MultiplesOf5);
    assert_eq!("1.100", dec.to_string());

    let mut dec = FixedDecimal::from_str("1.10125").unwrap();
    dec.round_with_mode_and_increment(
        -4,
        RoundingMode::HalfExpand,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("1.1025", dec.to_string());

    let mut dec = FixedDecimal::from_str("-1.25").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf5);
    assert_eq!("-1.0", dec.to_string());

    let mut dec = FixedDecimal::from_str("-1.251").unwrap();
    dec.round_with_mode_and_increment(-1, RoundingMode::HalfCeil, RoundingIncrement::MultiplesOf5);
    assert_eq!("-1.5", dec.to_string());

    let mut dec = FixedDecimal::from_str("-1.125").unwrap();
    dec.round_with_mode_and_increment(
        -2,
        RoundingMode::HalfFloor,
        RoundingIncrement::MultiplesOf25,
    );
    assert_eq!("-1.25", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.71").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("2.72", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.73").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("2.72", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.75").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("2.76", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.77").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("2.76", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.79").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("2.80", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.41").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("2.40", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.43").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("2.44", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.45").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("2.44", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.47").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("2.48", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.49").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf2);
    assert_eq!("2.48", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.725").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf5);
    assert_eq!("2.70", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.775").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf5);
    assert_eq!("2.80", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.875").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf25);
    assert_eq!("3.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.375").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf25);
    assert_eq!("2.50", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.125").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf25);
    assert_eq!("2.00", dec.to_string());

    let mut dec = FixedDecimal::from_str("2.625").unwrap();
    dec.round_with_mode_and_increment(-2, RoundingMode::HalfEven, RoundingIncrement::MultiplesOf25);
    assert_eq!("2.50", dec.to_string());
}
