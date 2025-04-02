// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] Property names-related data for this component
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

use crate::TimeZone;
use icu_provider::prelude::*;
use zerotrie::ZeroAsciiIgnoreCaseTrie;
use zerovec::{VarZeroVec, ZeroVec};

/// [`IanaToBcp47Map`]'s trie cannot handle differently-cased prefixes, like `Mexico/BajaSur`` and `MET`.
///
/// Therefore, any ID that is not of the shape `{region}/{city}` gets prefixed with this character
/// inside the trie.
///
/// During lookup, if the input is not of the shape `{region}/{city}`, the trie cursor has to be advanced over
/// this byte.
pub const NON_REGION_CITY_PREFIX: u8 = b'_';

icu_provider::data_marker!(
    /// See [`IanaToBcp47Map`]
    ///
    /// This marker uses a checksum to ensure consistency with [`TimeZoneIanaNamesV1`].
    TimeZoneIanaMapV1,
    "time/zone/iana/map/v1",
    IanaToBcp47Map<'static>,
    is_singleton = true,
    has_checksum = true,
);

icu_provider::data_marker!(
    /// See [`Bcp47ToIanaMap`]
    ///
    /// This marker uses a checksum to ensure consistency with [`TimeZoneIanaMapV1`].
    TimeZoneIanaNamesV1,
    "time/zone/iana/names/v1",
    IanaNames<'static>,
    is_singleton = true,
    has_checksum = true,
);

/// A mapping from normal-case IANA time zone identifiers to BCP-47 time zone identifiers.
///
/// Multiple IANA time zone IDs can map to the same BCP-47 time zone ID.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Clone, PartialEq, zerofrom::ZeroFrom, yoke::Yokeable)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_time::provider::iana))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct IanaToBcp47Map<'data> {
    /// A map from normal-case IANA time zone identifiers to indexes of BCP-47 time zone
    /// identifiers along with a canonical flag. The IANA identifiers are normal-case.
    ///
    /// The `usize` values stored in the trie have the following form:
    ///
    /// - Lowest bit: 1 if canonical, 0 if not canonical
    /// - All remaining bits: index into `bcp47_ids`
    ///
    /// For example, in CLDR 44, `"Africa/Abidjan"` has value 221, which means it is canonical
    /// (low bit is 1 == odd number) and the index into `bcp47_ids` is 110 (221 >> 1).
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub map: ZeroAsciiIgnoreCaseTrie<ZeroVec<'data, u8>>,
    /// A list of BCP-47 time zone identifiers, sorted by canonical IANA ID.
    #[cfg_attr(feature = "serde", serde(borrow))]
    // Note: this is 9739B as `ZeroVec<TimeZone>` (`ZeroVec<TinyStr8>`)
    // and 9335B as `VarZeroVec<str>`
    pub bcp47_ids: ZeroVec<'data, TimeZone>,
}

icu_provider::data_struct!(
    IanaToBcp47Map<'_>,
    #[cfg(feature = "datagen")]
);

/// A mapping from IANA time zone identifiers to BCP-47 time zone identifiers.
///
/// The BCP-47 time zone ID maps to the default IANA time zone ID according to the CLDR data.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Clone, PartialEq, zerofrom::ZeroFrom, yoke::Yokeable)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_time::provider::iana))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct IanaNames<'data> {
    /// The list of all normalized IANA identifiers.
    ///
    /// The first `bcp47_ids.len()` identifiers are canonical for the
    /// the BCP-47 IDs in [`IanaToBcp47Map::bcp47_ids`] at the same index.
    ///
    /// The remaining non-canonical identifiers are sorted in ascending lowercase order.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub normalized_iana_ids: VarZeroVec<'data, str>,
}

icu_provider::data_struct!(
    IanaNames<'_>,
    #[cfg(feature = "datagen")]
);
