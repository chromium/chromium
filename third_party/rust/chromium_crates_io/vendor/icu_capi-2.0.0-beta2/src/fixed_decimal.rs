// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use ffi::FixedDecimalSignedRoundingMode;

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::errors::ffi::{FixedDecimalLimitError, FixedDecimalParseError};

    use writeable::Writeable;

    #[diplomat::opaque]
    #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
    #[diplomat::rust_link(fixed_decimal::Decimal, Typedef, hidden)]
    pub struct Decimal(pub fixed_decimal::Decimal);

    /// The sign of a FixedDecimal, as shown in formatting.
    #[diplomat::rust_link(fixed_decimal::Sign, Enum)]
    #[diplomat::enum_convert(fixed_decimal::Sign, needs_wildcard)]
    pub enum FixedDecimalSign {
        /// No sign (implicitly positive, e.g., 1729).
        None,
        /// A negative sign, e.g., -1729.
        Negative,
        /// An explicit positive sign, e.g., +1729.
        Positive,
    }

    /// ECMA-402 compatible sign display preference.
    #[diplomat::rust_link(fixed_decimal::SignDisplay, Enum)]
    #[diplomat::enum_convert(fixed_decimal::SignDisplay, needs_wildcard)]
    pub enum FixedDecimalSignDisplay {
        Auto,
        Never,
        Always,
        ExceptZero,
        Negative,
    }

    /// Increment used in a rounding operation.
    #[diplomat::rust_link(fixed_decimal::RoundingIncrement, Enum)]
    #[diplomat::enum_convert(fixed_decimal::RoundingIncrement, needs_wildcard)]
    pub enum FixedDecimalRoundingIncrement {
        MultiplesOf1,
        MultiplesOf2,
        MultiplesOf5,
        MultiplesOf25,
    }

    /// Mode used in a rounding operation for signed numbers.
    #[diplomat::rust_link(fixed_decimal::SignedRoundingMode, Enum)]
    pub enum FixedDecimalSignedRoundingMode {
        Expand,
        Trunc,
        HalfExpand,
        HalfTrunc,
        HalfEven,
        Ceil,
        Floor,
        HalfCeil,
        HalfFloor,
    }

    impl Decimal {
        /// Construct an [`Decimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(dart, disable)]
        #[diplomat::attr(js, rename = "from_number")]
        #[diplomat::attr(supports = method_overloading, rename = "from")]
        #[diplomat::attr(auto, named_constructor)]
        pub fn from_int32(v: i32) -> Box<Decimal> {
            Box::new(Decimal(fixed_decimal::Decimal::from(v)))
        }

        /// Construct an [`Decimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(dart, disable)]
        #[diplomat::attr(js, disable)]
        #[diplomat::attr(supports = method_overloading, rename = "from")]
        #[diplomat::attr(auto, named_constructor)]
        pub fn from_uint32(v: u32) -> Box<Decimal> {
            Box::new(Decimal(fixed_decimal::Decimal::from(v)))
        }

        /// Construct an [`Decimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(dart, rename = "from_int")]
        #[diplomat::attr(js, rename = "from_big_int")]
        #[diplomat::attr(supports = method_overloading, rename = "from")]
        #[diplomat::attr(auto, named_constructor)]
        pub fn from_int64(v: i64) -> Box<Decimal> {
            Box::new(Decimal(fixed_decimal::Decimal::from(v)))
        }

        /// Construct an [`Decimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(any(dart, js), disable)]
        #[diplomat::attr(supports = method_overloading, rename = "from")]
        #[diplomat::attr(auto, named_constructor)]
        pub fn from_uint64(v: u64) -> Box<Decimal> {
            Box::new(Decimal(fixed_decimal::Decimal::from(v)))
        }

        /// Construct an [`Decimal`] from an integer-valued float
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(any(dart, js), disable)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_double_with_integer_precision(
            f: f64,
        ) -> Result<Box<Decimal>, FixedDecimalLimitError> {
            let precision = fixed_decimal::DoublePrecision::Integer;
            Ok(Box::new(Decimal(fixed_decimal::Decimal::try_from_f64(
                f, precision,
            )?)))
        }

        /// Construct an [`Decimal`] from an float, with a given power of 10 for the lower magnitude
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(js, rename = "from_number_with_lower_magnitude")]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn from_double_with_lower_magnitude(
            f: f64,
            magnitude: i16,
        ) -> Result<Box<Decimal>, FixedDecimalLimitError> {
            let precision = fixed_decimal::DoublePrecision::Magnitude(magnitude);
            Ok(Box::new(Decimal(fixed_decimal::Decimal::try_from_f64(
                f, precision,
            )?)))
        }

        /// Construct an [`Decimal`] from an float, for a given number of significant digits
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(js, rename = "from_number_with_significant_digits")]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_double_with_significant_digits(
            f: f64,
            digits: u8,
        ) -> Result<Box<Decimal>, FixedDecimalLimitError> {
            let precision = fixed_decimal::DoublePrecision::SignificantDigits(digits);
            Ok(Box::new(Decimal(fixed_decimal::Decimal::try_from_f64(
                f, precision,
            )?)))
        }

        /// Construct an [`Decimal`] from an float, with enough digits to recover
        /// the original floating point in IEEE 754 without needing trailing zeros
        #[diplomat::rust_link(fixed_decimal::Decimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(js, rename = "from_number_with_round_trip_precision")]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_double_with_round_trip_precision(
            f: f64,
        ) -> Result<Box<Decimal>, FixedDecimalLimitError> {
            let precision = fixed_decimal::DoublePrecision::RoundTrip;
            Ok(Box::new(Decimal(fixed_decimal::Decimal::try_from_f64(
                f, precision,
            )?)))
        }

        /// Construct an [`Decimal`] from a string.
        #[diplomat::rust_link(fixed_decimal::Decimal::try_from_str, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::Decimal::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(fixed_decimal::Decimal::from_str, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_string(v: &DiplomatStr) -> Result<Box<Decimal>, FixedDecimalParseError> {
            Ok(Box::new(Decimal(fixed_decimal::Decimal::try_from_utf8(v)?)))
        }

        #[diplomat::rust_link(fixed_decimal::UnsignedDecimal::digit_at, FnInStruct)]
        pub fn digit_at(&self, magnitude: i16) -> u8 {
            self.0.absolute.digit_at(magnitude)
        }

        #[diplomat::rust_link(fixed_decimal::UnsignedDecimal::magnitude_range, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn magnitude_start(&self) -> i16 {
            *self.0.absolute.magnitude_range().start()
        }

        #[diplomat::rust_link(fixed_decimal::UnsignedDecimal::magnitude_range, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn magnitude_end(&self) -> i16 {
            *self.0.absolute.magnitude_range().end()
        }

        #[diplomat::rust_link(fixed_decimal::UnsignedDecimal::nonzero_magnitude_start, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn nonzero_magnitude_start(&self) -> i16 {
            self.0.absolute.nonzero_magnitude_start()
        }

        #[diplomat::rust_link(fixed_decimal::UnsignedDecimal::nonzero_magnitude_end, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn nonzero_magnitude_end(&self) -> i16 {
            self.0.absolute.nonzero_magnitude_end()
        }

        #[diplomat::rust_link(fixed_decimal::UnsignedDecimal::is_zero, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn is_zero(&self) -> bool {
            self.0.absolute.is_zero()
        }

        /// Multiply the [`Decimal`] by a given power of ten.
        #[diplomat::rust_link(fixed_decimal::Decimal::multiply_pow10, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::Decimal::multiplied_pow10, FnInStruct, hidden)]
        pub fn multiply_pow10(&mut self, power: i16) {
            self.0.multiply_pow10(power)
        }

        #[diplomat::rust_link(fixed_decimal::Decimal::sign, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn sign(&self) -> FixedDecimalSign {
            self.0.sign().into()
        }

        /// Set the sign of the [`Decimal`].
        #[diplomat::rust_link(fixed_decimal::Decimal::set_sign, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::Decimal::with_sign, FnInStruct, hidden)]
        #[diplomat::attr(auto, setter = "sign")]
        pub fn set_sign(&mut self, sign: FixedDecimalSign) {
            self.0.set_sign(sign.into())
        }

        #[diplomat::rust_link(fixed_decimal::Decimal::apply_sign_display, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::Decimal::with_sign_display, FnInStruct, hidden)]
        pub fn apply_sign_display(&mut self, sign_display: FixedDecimalSignDisplay) {
            self.0.apply_sign_display(sign_display.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trim_start, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trimmed_start, FnInStruct, hidden)]
        pub fn trim_start(&mut self) {
            self.0.absolute.trim_start()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trim_end, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trimmed_end, FnInStruct, hidden)]
        pub fn trim_end(&mut self) {
            self.0.absolute.trim_end()
        }

        #[diplomat::rust_link(fixed_decimal::UnsignedDecimal::trim_end_if_integer, FnInStruct)]
        #[diplomat::rust_link(
            fixed_decimal::UnsignedDecimal::trimmed_end_if_integer,
            FnInStruct,
            hidden
        )]
        pub fn trim_end_if_integer(&mut self) {
            self.0.absolute.trim_end_if_integer()
        }

        /// Zero-pad the [`Decimal`] on the left to a particular position
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::pad_start, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::padded_start, FnInStruct, hidden)]
        pub fn pad_start(&mut self, position: i16) {
            self.0.absolute.pad_start(position)
        }

        /// Zero-pad the [`Decimal`] on the right to a particular position
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::pad_end, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::padded_end, FnInStruct, hidden)]
        pub fn pad_end(&mut self, position: i16) {
            self.0.absolute.pad_end(position)
        }

        /// Truncate the [`Decimal`] on the left to a particular position, deleting digits if necessary. This is useful for, e.g. abbreviating years
        /// ("2022" -> "22")
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::set_max_position, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::with_max_position, FnInStruct, hidden)]
        pub fn set_max_position(&mut self, position: i16) {
            self.0.absolute.set_max_position(position)
        }

        /// Round the number at a particular digit position.
        ///
        /// This uses half to even rounding, which resolves ties by selecting the nearest
        /// even integer to the original value.
        #[diplomat::rust_link(fixed_decimal::Decimal::round, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::Decimal::rounded, FnInStruct, hidden)]
        pub fn round(&mut self, position: i16) {
            self.0.round(position)
        }

        #[diplomat::rust_link(fixed_decimal::Decimal::ceil, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::Decimal::ceiled, FnInStruct, hidden)]
        pub fn ceil(&mut self, position: i16) {
            self.0.ceil(position)
        }

        #[diplomat::rust_link(fixed_decimal::Decimal::expand, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::Decimal::expanded, FnInStruct, hidden)]
        pub fn expand(&mut self, position: i16) {
            self.0.expand(position)
        }

        #[diplomat::rust_link(fixed_decimal::Decimal::floor, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::Decimal::floored, FnInStruct, hidden)]
        pub fn floor(&mut self, position: i16) {
            self.0.floor(position)
        }

        #[diplomat::rust_link(fixed_decimal::Decimal::trunc, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::Decimal::trunced, FnInStruct, hidden)]
        pub fn trunc(&mut self, position: i16) {
            self.0.trunc(position)
        }

        #[diplomat::rust_link(fixed_decimal::Decimal::round_with_mode, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::Decimal::rounded_with_mode, FnInStruct, hidden)]
        pub fn round_with_mode(&mut self, position: i16, mode: FixedDecimalSignedRoundingMode) {
            self.0.round_with_mode(position, mode.into())
        }

        #[diplomat::rust_link(
            fixed_decimal::FixedDecimal::round_with_mode_and_increment,
            FnInStruct
        )]
        #[diplomat::rust_link(
            fixed_decimal::FixedDecimal::rounded_with_mode_and_increment,
            FnInStruct,
            hidden
        )]
        pub fn round_with_mode_and_increment(
            &mut self,
            position: i16,
            mode: FixedDecimalSignedRoundingMode,
            increment: FixedDecimalRoundingIncrement,
        ) {
            self.0
                .round_with_mode_and_increment(position, mode.into(), increment.into())
        }

        /// Concatenates `other` to the end of `self`.
        ///
        /// If successful, `other` will be set to 0 and a successful status is returned.
        ///
        /// If not successful, `other` will be unchanged and an error is returned.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::concatenate_end, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::concatenated_end, FnInStruct, hidden)]
        pub fn concatenate_end(&mut self, other: &mut Decimal) -> Result<(), ()> {
            let x = core::mem::take(&mut other.0);
            self.0.absolute.concatenate_end(x.absolute).map_err(|y| {
                other.0.absolute = y;
            })
        }

        /// Format the [`Decimal`] as a string.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::write_to, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::to_string, FnInStruct, hidden)]
        #[diplomat::attr(auto, stringifier)]
        pub fn to_string(&self, to: &mut diplomat_runtime::DiplomatWrite) {
            let _ = self.0.write_to(to);
        }
    }
}

impl From<FixedDecimalSignedRoundingMode> for fixed_decimal::SignedRoundingMode {
    fn from(mode: FixedDecimalSignedRoundingMode) -> Self {
        match mode {
            FixedDecimalSignedRoundingMode::Expand => fixed_decimal::SignedRoundingMode::Unsigned(
                fixed_decimal::UnsignedRoundingMode::Expand,
            ),
            FixedDecimalSignedRoundingMode::Trunc => fixed_decimal::SignedRoundingMode::Unsigned(
                fixed_decimal::UnsignedRoundingMode::Trunc,
            ),
            FixedDecimalSignedRoundingMode::HalfExpand => {
                fixed_decimal::SignedRoundingMode::Unsigned(
                    fixed_decimal::UnsignedRoundingMode::HalfExpand,
                )
            }
            FixedDecimalSignedRoundingMode::HalfTrunc => {
                fixed_decimal::SignedRoundingMode::Unsigned(
                    fixed_decimal::UnsignedRoundingMode::HalfTrunc,
                )
            }
            FixedDecimalSignedRoundingMode::HalfEven => {
                fixed_decimal::SignedRoundingMode::Unsigned(
                    fixed_decimal::UnsignedRoundingMode::HalfEven,
                )
            }
            FixedDecimalSignedRoundingMode::Ceil => fixed_decimal::SignedRoundingMode::Ceil,
            FixedDecimalSignedRoundingMode::Floor => fixed_decimal::SignedRoundingMode::Floor,
            FixedDecimalSignedRoundingMode::HalfCeil => fixed_decimal::SignedRoundingMode::HalfCeil,
            FixedDecimalSignedRoundingMode::HalfFloor => {
                fixed_decimal::SignedRoundingMode::HalfFloor
            }
        }
    }
}
