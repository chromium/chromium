// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use icu_plurals::PluralCategory;
use icu_provider::prelude::*;
use zerovec::ZeroMap;

icu_provider::data_marker!(
    /// `ShortCurrencyCompactV1`
    ShortCurrencyCompactV1,
    ShortCurrencyCompact<'static>
);

/// Currency Compact  data struct.
#[derive(Debug, Clone, Default, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::compact))]
#[yoke(prove_covariance_manually)]
pub struct ShortCurrencyCompact<'data> {
    // TODO: this map should include a `DoublePattern` as a value.
    /// Contains the compact patterns for a compact currency format based on the plural rules.
    /// NOTE:
    ///       A map keyed on log10 of the CLDR `type` attribute.
    ///       For example:
    ///         `"1000-count-one": "¤0K"`
    ///                 -> key1 = 3, key2 = CompactCount::One, value = "¤0K"
    ///         `"1000-count-one-alt-alphaNextToNumber": "¤ 0K"`
    ///                 -> key1 = 3, key2 = CompactCount::OneAlt, value = "¤ 0K"
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub compact_patterns: ZeroMap<'data, (i8, CompactCount), str>,
}

icu_provider::data_struct!(ShortCurrencyCompact<'_>, #[cfg(feature = "datagen")]);

#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::cucompact))]
#[repr(u8)]
pub enum CompactCount {
    Standard(PluralCategory),
    AlphaNextToNumber(PluralCategory),
}
