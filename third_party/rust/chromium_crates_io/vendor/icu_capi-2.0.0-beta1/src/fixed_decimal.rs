// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::errors::ffi::{FixedDecimalLimitError, FixedDecimalParseError};

    use writeable::Writeable;

    #[diplomat::opaque]
    #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
    pub struct FixedDecimal(pub fixed_decimal::FixedDecimal);

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

    /// Mode used in a rounding operation.
    #[diplomat::rust_link(fixed_decimal::RoundingMode, Enum)]
    #[diplomat::enum_convert(fixed_decimal::RoundingMode, needs_wildcard)]
    pub enum FixedDecimalRoundingMode {
        Ceil,
        Expand,
        Floor,
        Trunc,
        HalfCeil,
        HalfExpand,
        HalfFloor,
        HalfTrunc,
        HalfEven,
    }

    impl FixedDecimal {
        /// Construct an [`FixedDecimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(dart, disable)]
        #[diplomat::attr(js, rename = "from_number")]
        #[diplomat::attr(supports = method_overloading, rename = "from")]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_int32(v: i32) -> Box<FixedDecimal> {
            Box::new(FixedDecimal(fixed_decimal::FixedDecimal::from(v)))
        }

        /// Construct an [`FixedDecimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(dart, disable)]
        #[diplomat::attr(js, disable)]
        #[diplomat::attr(supports = method_overloading, rename = "from")]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_uint32(v: u32) -> Box<FixedDecimal> {
            Box::new(FixedDecimal(fixed_decimal::FixedDecimal::from(v)))
        }

        /// Construct an [`FixedDecimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(dart, rename = "from_int")]
        #[diplomat::attr(js, rename = "from_big_int")]
        #[diplomat::attr(supports = method_overloading, rename = "from")]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_int64(v: i64) -> Box<FixedDecimal> {
            Box::new(FixedDecimal(fixed_decimal::FixedDecimal::from(v)))
        }

        /// Construct an [`FixedDecimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(any(dart, js), disable)]
        #[diplomat::attr(supports = method_overloading, rename = "from")]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_uint64(v: u64) -> Box<FixedDecimal> {
            Box::new(FixedDecimal(fixed_decimal::FixedDecimal::from(v)))
        }

        /// Construct an [`FixedDecimal`] from an integer-valued float
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(any(dart, js), disable)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_double_with_integer_precision(
            f: f64,
        ) -> Result<Box<FixedDecimal>, FixedDecimalLimitError> {
            let precision = fixed_decimal::DoublePrecision::Integer;
            Ok(Box::new(FixedDecimal(
                fixed_decimal::FixedDecimal::try_from_f64(f, precision)?,
            )))
        }

        /// Construct an [`FixedDecimal`] from an float, with a given power of 10 for the lower magnitude
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(js, rename = "from_number_with_lower_magnitude")]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn from_double_with_lower_magnitude(
            f: f64,
            magnitude: i16,
        ) -> Result<Box<FixedDecimal>, FixedDecimalLimitError> {
            let precision = fixed_decimal::DoublePrecision::Magnitude(magnitude);
            Ok(Box::new(FixedDecimal(
                fixed_decimal::FixedDecimal::try_from_f64(f, precision)?,
            )))
        }

        /// Construct an [`FixedDecimal`] from an float, for a given number of significant digits
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(js, rename = "from_number_with_significant_digits")]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_double_with_significant_digits(
            f: f64,
            digits: u8,
        ) -> Result<Box<FixedDecimal>, FixedDecimalLimitError> {
            let precision = fixed_decimal::DoublePrecision::SignificantDigits(digits);
            Ok(Box::new(FixedDecimal(
                fixed_decimal::FixedDecimal::try_from_f64(f, precision)?,
            )))
        }

        /// Construct an [`FixedDecimal`] from an float, with enough digits to recover
        /// the original floating point in IEEE 754 without needing trailing zeros
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(js, rename = "from_number_with_round_trip_precision")]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_double_with_round_trip_precision(
            f: f64,
        ) -> Result<Box<FixedDecimal>, FixedDecimalLimitError> {
            let precision = fixed_decimal::DoublePrecision::RoundTrip;
            Ok(Box::new(FixedDecimal(
                fixed_decimal::FixedDecimal::try_from_f64(f, precision)?,
            )))
        }

        /// Construct an [`FixedDecimal`] from a string.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_str, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::from_str, FnInStruct, hidden)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_string(v: &DiplomatStr) -> Result<Box<FixedDecimal>, FixedDecimalParseError> {
            Ok(Box::new(FixedDecimal(
                fixed_decimal::FixedDecimal::try_from_utf8(v)?,
            )))
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::digit_at, FnInStruct)]
        pub fn digit_at(&self, magnitude: i16) -> u8 {
            self.0.digit_at(magnitude)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::magnitude_range, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn magnitude_start(&self) -> i16 {
            *self.0.magnitude_range().start()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::magnitude_range, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn magnitude_end(&self) -> i16 {
            *self.0.magnitude_range().end()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::nonzero_magnitude_start, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn nonzero_magnitude_start(&self) -> i16 {
            self.0.nonzero_magnitude_start()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::nonzero_magnitude_end, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn nonzero_magnitude_end(&self) -> i16 {
            self.0.nonzero_magnitude_end()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::is_zero, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn is_zero(&self) -> bool {
            self.0.is_zero()
        }

        /// Multiply the [`FixedDecimal`] by a given power of ten.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::multiply_pow10, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::multiplied_pow10, FnInStruct, hidden)]
        pub fn multiply_pow10(&mut self, power: i16) {
            self.0.multiply_pow10(power)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::sign, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn sign(&self) -> FixedDecimalSign {
            self.0.sign().into()
        }

        /// Set the sign of the [`FixedDecimal`].
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::set_sign, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::with_sign, FnInStruct, hidden)]
        #[diplomat::attr(auto, setter = "sign")]
        pub fn set_sign(&mut self, sign: FixedDecimalSign) {
            self.0.set_sign(sign.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::apply_sign_display, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::with_sign_display, FnInStruct, hidden)]
        pub fn apply_sign_display(&mut self, sign_display: FixedDecimalSignDisplay) {
            self.0.apply_sign_display(sign_display.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trim_start, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trimmed_start, FnInStruct, hidden)]
        pub fn trim_start(&mut self) {
            self.0.trim_start()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trim_end, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trimmed_end, FnInStruct, hidden)]
        pub fn trim_end(&mut self) {
            self.0.trim_end()
        }

        /// Zero-pad the [`FixedDecimal`] on the left to a particular position
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::pad_start, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::padded_start, FnInStruct, hidden)]
        pub fn pad_start(&mut self, position: i16) {
            self.0.pad_start(position)
        }

        /// Zero-pad the [`FixedDecimal`] on the right to a particular position
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::pad_end, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::padded_end, FnInStruct, hidden)]
        pub fn pad_end(&mut self, position: i16) {
            self.0.pad_end(position)
        }

        /// Truncate the [`FixedDecimal`] on the left to a particular position, deleting digits if necessary. This is useful for, e.g. abbreviating years
        /// ("2022" -> "22")
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::set_max_position, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::with_max_position, FnInStruct, hidden)]
        pub fn set_max_position(&mut self, position: i16) {
            self.0.set_max_position(position)
        }

        /// Round the number at a particular digit position.
        ///
        /// This uses half to even rounding, which resolves ties by selecting the nearest
        /// even integer to the original value.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::round, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::rounded, FnInStruct, hidden)]
        pub fn round(&mut self, position: i16) {
            self.0.round(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::ceil, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::ceiled, FnInStruct, hidden)]
        pub fn ceil(&mut self, position: i16) {
            self.0.ceil(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::expand, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::expanded, FnInStruct, hidden)]
        pub fn expand(&mut self, position: i16) {
            self.0.expand(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::floor, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::floored, FnInStruct, hidden)]
        pub fn floor(&mut self, position: i16) {
            self.0.floor(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trunc, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trunced, FnInStruct, hidden)]
        pub fn trunc(&mut self, position: i16) {
            self.0.trunc(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::round_with_mode, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::rounded_with_mode, FnInStruct, hidden)]
        pub fn round_with_mode(&mut self, position: i16, mode: FixedDecimalRoundingMode) {
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
            mode: FixedDecimalRoundingMode,
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
        pub fn concatenate_end(&mut self, other: &mut FixedDecimal) -> Result<(), ()> {
            let x = core::mem::take(&mut other.0);
            self.0.concatenate_end(x).map_err(|y| {
                other.0 = y;
            })
        }

        /// Format the [`FixedDecimal`] as a string.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::write_to, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::to_string, FnInStruct, hidden)]
        #[diplomat::attr(auto, stringifier)]
        pub fn to_string(&self, to: &mut diplomat_runtime::DiplomatWrite) {
            let _ = self.0.write_to(to);
        }
    }
}
