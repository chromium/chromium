// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data for reverse folding

use icu_provider::prelude::*;
use potential_utf::PotentialUtf8;
use zerovec::ZeroMap;

/// Reverse case folding data. Maps from multi-character strings back
/// to code-points that fold to those strings.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider))]
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[yoke(prove_covariance_manually)]
pub struct CaseMapUnfold<'data> {
    #[cfg_attr(feature = "serde", serde(borrow))]
    /// The actual map. Maps from strings to a list of codepoints, stored as a contiguous UTF-8 string
    pub map: ZeroMap<'data, PotentialUtf8, str>,
}

icu_provider::data_struct!(
    CaseMapUnfold<'_>,
    #[cfg(feature = "datagen")]
);

impl CaseMapUnfold<'_> {
    // Given a string, returns another string representing the set of characters
    // that case fold to that string.
    pub(crate) fn get(&self, key: &str) -> Option<&str> {
        self.map.get(PotentialUtf8::from_str(key))
    }
}
