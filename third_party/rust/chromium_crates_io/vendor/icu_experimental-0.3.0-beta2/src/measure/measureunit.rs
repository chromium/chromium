// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use smallvec::SmallVec;

use super::provider::single_unit::SingleUnit;

// TODO NOTE: the MeasureUnitParser takes the trie and the ConverterFactory takes the full payload and an instance of MeasureUnitParser.
/// The [`MeasureUnit`] struct represents a processed CLDR compound unit.
/// Examples include:
///  1. `meter-per-second`
///  2. `square-meter`
///  3. `liter-per-100-kilometer`
///  4. `portion-per-1e9`
///  5. `square-meter` (Note: a single unit is a special case of a compound unit containing only one single unit.)
///
/// To construct a [`MeasureUnit`] from a CLDR unit identifier, use the [`crate::measure::parser::MeasureUnitParser`].
#[derive(Debug)]
pub struct MeasureUnit {
    /// Contains the processed units.
    pub(crate) single_units: SmallVec<[SingleUnit; 8]>,

    /// Represents the constant denominator of this measure unit.
    ///
    /// Examples:
    ///   - For the unit `meter-per-second`, the constant denominator is `0`, because there is no denominator.
    ///   - For the unit `liter-per-100-kilometer`, the constant denominator is `100`.
    ///   - For the unit `portion-per-1e9`, the constant denominator is `1_000_000_000`.
    ///
    /// NOTE:
    ///   If the constant denominator is not set, the value defaults to `0`.
    pub(crate) constant_denominator: u64,
}

impl MeasureUnit {
    /// Returns a reference to the single units contained within this measure unit.
    pub fn get_single_units(&self) -> &SmallVec<[SingleUnit; 8]> {
        &self.single_units
    }

    /// Returns the constant denominator of this measure unit.
    ///
    /// NOTE:
    ///   If the constant denominator is not set, a value of `0` is returned.
    pub fn get_constant_denominator(&self) -> u64 {
        self.constant_denominator
    }
}
