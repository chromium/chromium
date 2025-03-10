// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::si_prefix::SiPrefix;

/// Represents a single unit in a measure unit.
/// For example, the MeasureUnit `kilometer-per-square-second` contains two single units:
///    1. `kilometer` with power 1 and prefix 3 with base 10.
///    2. `second` with power -2 and prefix power equal to 0.
#[zerovec::make_ule(SingleUnitULE)]
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Default)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::measure::provider::single_unit))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct SingleUnit {
    /// The power of the unit.
    pub power: i8,

    /// The si base of the unit.
    pub si_prefix: SiPrefix,

    /// The id of the unit.
    pub unit_id: u16,
}
