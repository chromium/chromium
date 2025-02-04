// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] Data provider struct definitions for this ICU4X component.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

mod lstm;
pub use lstm::*;

use crate::WordType;
use icu_collections::codepointtrie::CodePointTrie;
use icu_provider::prelude::*;
use zerovec::ZeroVec;

#[cfg(feature = "compiled_data")]
#[derive(Debug)]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub struct Baked;

#[cfg(feature = "compiled_data")]
#[allow(unused_imports)]
const _: () = {
    use icu_segmenter_data::*;
    pub mod icu {
        pub use crate as segmenter;
        pub use icu_collections as collections;
        pub use icu_segmenter_data::icu_locale as locale;
    }
    make_provider!(Baked);
    impl_dictionary_for_word_only_auto_v1_marker!(Baked);
    impl_dictionary_for_word_line_extended_v1_marker!(Baked);
    impl_grapheme_cluster_break_data_v2_marker!(Baked);
    impl_line_break_data_v2_marker!(Baked);
    #[cfg(feature = "lstm")]
    impl_lstm_for_word_line_auto_v1_marker!(Baked);
    impl_sentence_break_data_override_v1_marker!(Baked);
    impl_sentence_break_data_v2_marker!(Baked);
    impl_word_break_data_override_v1_marker!(Baked);
    impl_word_break_data_v2_marker!(Baked);
};

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] = &[
    DictionaryForWordLineExtendedV1Marker::INFO,
    DictionaryForWordOnlyAutoV1Marker::INFO,
    GraphemeClusterBreakDataV2Marker::INFO,
    LineBreakDataV2Marker::INFO,
    LstmForWordLineAutoV1Marker::INFO,
    SentenceBreakDataOverrideV1Marker::INFO,
    SentenceBreakDataV2Marker::INFO,
    WordBreakDataOverrideV1Marker::INFO,
    WordBreakDataV2Marker::INFO,
];

/// Pre-processed Unicode data in the form of tables to be used for rule-based breaking.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(LineBreakDataV2Marker, "segmenter/line@2", singleton),
    marker(WordBreakDataV2Marker, "segmenter/word@2", singleton),
    marker(GraphemeClusterBreakDataV2Marker, "segmenter/grapheme@2", singleton),
    marker(SentenceBreakDataV2Marker, "segmenter/sentence@2", singleton)
)]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_segmenter::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct RuleBreakDataV2<'data> {
    /// Property table.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub property_table: CodePointTrie<'data, u8>,

    /// Break state table.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub break_state_table: ZeroVec<'data, BreakState>,

    /// Word type table. Only used for word segmenter.
    #[cfg_attr(feature = "serde", serde(borrow, rename = "rule_status_table"))]
    pub word_type_table: ZeroVec<'data, WordType>,

    /// Number of properties; should be the square root of the length of [`Self::break_state_table`].
    pub property_count: u8,

    /// The index of the last simple state for [`Self::break_state_table`]. (A simple state has no
    /// `left` nor `right` in SegmenterProperty).
    pub last_codepoint_property: u8,

    /// The index of SOT (start of text) state for [`Self::break_state_table`].
    pub sot_property: u8,

    /// The index of EOT (end of text) state [`Self::break_state_table`].
    pub eot_property: u8,

    /// The index of "SA" state (or 127 if the complex language isn't handled) for
    /// [`Self::break_state_table`].
    pub complex_property: u8,
}

/// char16trie data for dictionary break
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(
        DictionaryForWordOnlyAutoV1Marker,
        "segmenter/dictionary/w_auto@1",
        attributes_domain = "segmenter"
    ),
    marker(
        DictionaryForWordLineExtendedV1Marker,
        "segmenter/dictionary/wl_ext@1",
        attributes_domain = "segmenter"
    )
)]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_segmenter::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct UCharDictionaryBreakDataV1<'data> {
    /// Dictionary data of char16trie.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub trie_data: ZeroVec<'data, u16>,
}

pub(crate) struct UCharDictionaryBreakDataV1Marker;

impl DynamicDataMarker for UCharDictionaryBreakDataV1Marker {
    type DataStruct = UCharDictionaryBreakDataV1<'static>;
}

/// codepoint trie data that the difference by specific locale
#[icu_provider::data_struct(
    marker(SentenceBreakDataOverrideV1Marker, "segmenter/sentence/override@1",),
    marker(WordBreakDataOverrideV1Marker, "segmenter/word/override@1")
)]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize,databake::Bake),
    databake(path = icu_segmenter::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct RuleBreakDataOverrideV1<'data> {
    /// The difference of property table for special locale.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub property_table_override: CodePointTrie<'data, u8>,
}

#[derive(Clone, Copy, PartialEq, Debug)]
#[cfg_attr(feature = "datagen", derive(databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_segmenter::provider))]
/// Break state
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub enum BreakState {
    /// Break
    Break,
    /// Keep rule
    Keep,
    /// Non-matching rule
    NoMatch,
    /// We have to look ahead one more character.
    Intermediate(u8),
    /// Index of a state.
    Index(u8),
}

#[cfg(feature = "datagen")]
impl serde::Serialize for BreakState {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        // would be nice to use the derive serde for JSON, but can't break serialization
        if serializer.is_human_readable() {
            i8::from_le_bytes([zerovec::ule::AsULE::to_unaligned(*self)]).serialize(serializer)
        } else {
            zerovec::ule::AsULE::to_unaligned(*self).serialize(serializer)
        }
    }
}

#[cfg(feature = "serde")]
impl<'de> serde::Deserialize<'de> for BreakState {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            Ok(zerovec::ule::AsULE::from_unaligned(
                i8::deserialize(deserializer)?.to_le_bytes()[0],
            ))
        } else {
            u8::deserialize(deserializer).map(zerovec::ule::AsULE::from_unaligned)
        }
    }
}

impl zerovec::ule::AsULE for BreakState {
    type ULE = u8;

    fn to_unaligned(self) -> Self::ULE {
        match self {
            BreakState::Break => 253,
            BreakState::Keep => 255,
            BreakState::NoMatch => 254,
            BreakState::Intermediate(i) => i + 120,
            BreakState::Index(i) => i,
        }
    }

    fn from_unaligned(unaligned: Self::ULE) -> Self {
        match unaligned {
            253 => BreakState::Break,
            255 => BreakState::Keep,
            254 => BreakState::NoMatch,
            i if (120..253).contains(&i) => BreakState::Intermediate(i - 120),
            i => BreakState::Index(i),
        }
    }
}

#[cfg(feature = "datagen")]
impl serde::Serialize for WordType {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        if serializer.is_human_readable() {
            (*self as u8).serialize(serializer)
        } else {
            unreachable!("only used as ULE")
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::Bake for WordType {
    fn bake(&self, _crate_env: &databake::CrateEnv) -> databake::TokenStream {
        unreachable!("only used as ULE")
    }
}

#[cfg(feature = "serde")]
impl<'de> serde::Deserialize<'de> for WordType {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            use serde::de::Error;
            match u8::deserialize(deserializer) {
                Ok(0) => Ok(WordType::None),
                Ok(1) => Ok(WordType::Number),
                Ok(2) => Ok(WordType::Letter),
                Ok(_) => Err(D::Error::custom("invalid value")),
                Err(e) => Err(e),
            }
        } else {
            unreachable!("only used as ULE")
        }
    }
}
