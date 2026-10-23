// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::units::convertible::Convertible;

/// A converter for converting between two single or compound units.
/// For example:
///     1 - `meter` to `foot`
///     2 - `mile-per-gallon` to `liter-per-100-kilometer`.
///     3 - `celsius` to `fahrenheit`.
///
/// NOTE:
///     This converter does not support conversions between mixed units,
///     for example, from "meter" to "foot-and-inch".
#[derive(Debug, Clone)]
pub struct UnitsConverter<N>(pub(crate) UnitsConverterInner<N>)
where
    N: Convertible;

impl<N> UnitsConverter<N>
where
    N: Convertible,
{
    /// Converts the given value from the input unit to the output unit.
    pub fn convert(&self, value: N) -> N::Result {
        match &self.0 {
            UnitsConverterInner::Proportional { factor } => value.mul(factor),
            UnitsConverterInner::Reciprocal { factor } => value.reciprocal_mul(factor),
            UnitsConverterInner::Offset { factor, offset } => value.mul_add(factor, offset),
        }
    }
}

/// Enum containing all the of converters: Proportional, Reciprocal, and Offset converters as follows:
///    1 - Proportional: Converts between two units that are proportionally related (e.g. `meter` to `foot`).
///    2 - Reciprocal: Converts between two units that are reciprocal (e.g. `mile-per-gallon` to `liter-per-100-kilometer`).
///    3 - Offset: Converts between two units that require an offset (e.g. `celsius` to `fahrenheit`).
#[derive(Debug, Clone)]
pub(crate) enum UnitsConverterInner<N>
where
    N: Convertible,
{
    /// `ProportionalConverter` is responsible for converting between two units that are proportionally related.
    /// For example: 1- `meter` to `foot`.
    ///              2- `square-meter` to `square-foot`.
    ///
    /// However, it cannot convert between two units that are not proportionally related,
    /// such as `celsius` to `fahrenheit` and `mile-per-gallon` to `liter-per-100-kilometer`.
    ///
    /// Also, it cannot convert between two units that are not single, such as `meter` to `foot-and-inch`.
    Proportional { factor: N::Factor },
    /// A converter for converting between two units that are reciprocal.
    /// For example:
    ///    1 - `meter-per-second` to `second-per-meter`.
    ///    2 - `mile-per-gallon` to `liter-per-100-kilometer`.
    Reciprocal { factor: N::Factor },
    /// A converter for converting between two units that require an offset.
    Offset {
        factor: N::Factor,
        offset: N::Addend,
    },
}
