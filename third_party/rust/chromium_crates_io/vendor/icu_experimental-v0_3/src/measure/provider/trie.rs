// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use icu_provider::prelude::*;
use zerotrie::ZeroTrieSimpleAscii;
use zerovec::ZeroVec;

#[cfg(feature = "compiled_data")]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub use crate::provider::Baked;

icu_provider::data_marker!(
    /// `UnitsTrieV1`
    UnitsTrieV1,
    UnitsTrie<'static>,
    is_singleton = true,
);

/// This type encapsulates all the constant data required for unit conversions.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, PartialEq, Debug, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::measure::provider::trie))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct UnitsTrie<'data> {
    /// Maps from a unit name (e.g., "foot" or "meter") to its corresponding unit ID.
    /// This ID represents the index of this unit in the `UnitsInfo` struct and can be used to retrieve the conversion information.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub trie: ZeroTrieSimpleAscii<ZeroVec<'data, u8>>,
}

icu_provider::data_struct!(UnitsTrie<'_>, #[cfg(feature = "datagen")]);
