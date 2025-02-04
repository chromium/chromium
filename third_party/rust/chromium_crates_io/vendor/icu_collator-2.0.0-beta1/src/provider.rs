// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// The reordering algorithms in this file are adapted from ICU4C and,
// therefore, are subject to the ICU license as described in LICENSE.

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

use icu_collections::char16trie::Char16TrieIterator;
use icu_collections::codepointtrie::CodePointTrie;
use icu_provider::prelude::*;
use zerovec::ule::AsULE;
use zerovec::ZeroVec;
use zerovec::{zeroslice, ZeroSlice};

use crate::elements::CollationElement;
use crate::elements::CollationElement32;
use crate::elements::Tag;
use crate::elements::EMPTY_U16;
use crate::elements::FFFD_CE;
use crate::elements::FFFD_CE32;
use crate::elements::FFFD_CE32_VALUE;
use crate::elements::FFFD_CE_VALUE;
use crate::elements::NO_CE_PRIMARY;

use super::CaseFirst;
use super::MaxVariable;

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
    use icu_collator_data::*;
    pub mod icu {
        pub use crate as collator;
        pub use icu_collator_data::icu_locale as locale;
        pub use icu_collections as collections;
    }
    make_provider!(Baked);
    impl_collation_root_v1_marker!(Baked);
    impl_collation_tailoring_v1_marker!(Baked);
    impl_collation_diacritics_v1_marker!(Baked);
    impl_collation_jamo_v1_marker!(Baked);
    impl_collation_metadata_v1_marker!(Baked);
    impl_collation_special_primaries_v1_marker!(Baked);
    impl_collation_reordering_v1_marker!(Baked);
};

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] = &[
    CollationRootV1Marker::INFO,
    CollationTailoringV1Marker::INFO,
    CollationDiacriticsV1Marker::INFO,
    CollationJamoV1Marker::INFO,
    CollationMetadataV1Marker::INFO,
    CollationReorderingV1Marker::INFO,
    CollationSpecialPrimariesV1Marker::INFO,
];

const SINGLE_U32: &ZeroSlice<u32> =
    zeroslice!(u32; <u32 as AsULE>::ULE::from_unsigned; [FFFD_CE32_VALUE]);
const SINGLE_U64: &ZeroSlice<u64> =
    zeroslice!(u64; <u64 as AsULE>::ULE::from_unsigned; [FFFD_CE_VALUE]);

fn data_ce_to_primary(data_ce: u64, c: char) -> u32 {
    // Collation::getThreeBytePrimaryForOffsetData
    let p = (data_ce >> 32) as u32; // three-byte primary pppppp00
    let lower32 = data_ce as u32 as i32; // base code point b & step s: bbbbbbss (bit 7: isCompressible)
    let mut offset = ((u32::from(c) as i32) - (lower32 >> 8)) * (lower32 & 0x7F); // delta * increment
    let is_compressible = (lower32 & 0x80) != 0;
    // Collation::incThreeBytePrimaryByOffset
    offset += (((p >> 8) & 0xFF) as i32) - 2;
    let mut primary = (((offset % 254) + 2) as u32) << 8;
    offset /= 254;
    // Same with the second byte,
    // but reserve the PRIMARY_COMPRESSION_LOW_BYTE and high byte if necessary.
    if is_compressible {
        offset += (((p >> 16) & 0xFF) as i32) - 4;
        primary |= (((offset % 251) + 4) as u32) << 16;
        offset /= 251;
    } else {
        offset += (((p >> 16) & 0xFF) as i32) - 2;
        primary |= (((offset % 254) + 2) as u32) << 16;
        offset /= 254;
    }
    primary | ((p & 0xFF000000) + ((offset as u32) << 24))
}

/// The main collation data either for the root or for a tailoring
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(CollationRootV1Marker, "collator/root@1", singleton),
    marker(
        CollationTailoringV1Marker,
        "collator/tailoring@1",
        fallback_by = "script",
        attributes_domain = "collator",
    )
)]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_collator::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct CollationDataV1<'data> {
    /// Mapping from `char` to `CollationElement32` (represented
    /// as its `u32` bits).
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub trie: CodePointTrie<'data, u32>,
    /// `CollationElement`s used in expansions and offset CE32s
    /// (represented as their `u64` bits)
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub ces: ZeroVec<'data, u64>,
    /// `CollationElement32`s used in expansions and as defaults
    /// for digits when the numeric mode is not in use
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub ce32s: ZeroVec<'data, u32>,
    /// Defaults and tries for prefix and contraction matching
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub contexts: ZeroVec<'data, u16>,
}

impl<'data> CollationDataV1<'data> {
    pub(crate) fn ce32_for_char(&self, c: char) -> CollationElement32 {
        CollationElement32::new(self.trie.get32(c as u32))
    }
    pub(crate) fn get_ce32(&'data self, index: usize) -> CollationElement32 {
        CollationElement32::new(if let Some(u) = self.ce32s.get(index) {
            u
        } else {
            // GIGO case
            debug_assert!(false);
            FFFD_CE32_VALUE
        })
    }
    pub(crate) fn get_ce32s(&'data self, index: usize, len: usize) -> &'data ZeroSlice<u32> {
        if len > 0 {
            if let Some(slice) = self.ce32s.get_subslice(index..index + len) {
                return slice;
            }
        }
        // GIGO case
        debug_assert!(false);
        SINGLE_U32
    }
    pub(crate) fn get_ces(&'data self, index: usize, len: usize) -> &'data ZeroSlice<u64> {
        if len > 0 {
            if let Some(slice) = self.ces.get_subslice(index..index + len) {
                return slice;
            }
        }
        // GIGO case
        debug_assert!(false);
        SINGLE_U64
    }
    fn get_default_and_trie_impl(
        &'data self,
        index: usize,
    ) -> (CollationElement32, &'data ZeroSlice<u16>) {
        if let Some(slice) = self.contexts.get_subslice(index..self.contexts.len()) {
            #[allow(clippy::unwrap_used)]
            if slice.len() >= 2 {
                // `unwrap` must succeed due to the length check above.
                let first = slice.get(0).unwrap();
                let second = slice.get(1).unwrap();
                let trie = slice.get_subslice(2..slice.len()).unwrap();
                return (
                    CollationElement32::new((u32::from(first) << 16) | u32::from(second)),
                    trie,
                );
            }
        }
        // GIGO case
        debug_assert!(false);
        (FFFD_CE32, EMPTY_U16)
    }
    pub(crate) fn get_default_and_trie(
        &'data self,
        index: usize,
    ) -> (CollationElement32, Char16TrieIterator<'data>) {
        let (ce32, trie) = self.get_default_and_trie_impl(index);
        (ce32, Char16TrieIterator::new(trie))
    }
    pub(crate) fn get_default(&'data self, index: usize) -> CollationElement32 {
        let (ce32, _) = self.get_default_and_trie_impl(index);
        ce32
    }
    pub(crate) fn ce_from_offset_ce32(
        &self,
        c: char,
        ce32: CollationElement32,
    ) -> CollationElement {
        debug_assert!(ce32.tag() == Tag::Offset);
        if let Some(data_ce) = self.ces.get(ce32.index()) {
            CollationElement::new_from_primary(data_ce_to_primary(data_ce, c))
        } else {
            // GIGO case
            debug_assert!(false);
            FFFD_CE
        }
    }
}

/// Secondary weights for the start of the Combining Diacritics block.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(
    CollationDiacriticsV1Marker,
    "collator/dia@1",
    fallback_by = "script",
    attributes_domain = "collator",
))]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_collator::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct CollationDiacriticsV1<'data> {
    /// Secondary weights for characters starting from U+0300 up
    /// to but not including U+034F. May be shorter than that;
    /// zero-length when a tailoring opts out of using this
    /// feature altogether.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub secondaries: ZeroVec<'data, u16>,
}

/// `CollationElement32`s for the Hangul Jamo Unicode Block
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(CollationJamoV1Marker, "collator/jamo@1", singleton))]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_collator::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct CollationJamoV1<'data> {
    /// `CollationElement32`s (as `u32`s) for the Hangul Jamo Unicode Block.
    /// The length must be equal to the size of the block (256).
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub ce32s: ZeroVec<'data, u32>,
}

/// Script reordering data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(
    CollationReorderingV1Marker,
    "collator/reord@1",
    fallback_by = "script",
    attributes_domain = "collator",
))]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_collator::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct CollationReorderingV1<'data> {
    /// Limit of last reordered range. 0 if no reordering or no split bytes.
    ///
    /// Comment from ICU4C's `collationsettings.h`
    pub min_high_no_reorder: u32,
    /// 256-byte table for reordering permutation of primary lead
    /// bytes; NULL if no reordering. A 0 entry at a non-zero index means
    /// that the primary lead byte is "split" (there are different offsets
    /// for primaries that share that lead byte) and the reordering offset
    /// must be determined via the reorderRanges.
    ///
    /// Comment from ICU4C's `collationsettings.h`
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub reorder_table: ZeroVec<'data, u8>, // len always 256
    /// Primary-weight ranges for script reordering, to be used by
    /// reorder(p) for split-reordered primary lead bytes.
    ///
    /// Each entry is a (limit, offset) pair. The upper 16 bits of the
    /// entry are the upper 16 bits of the exclusive primary limit of
    /// a range. Primaries between the previous limit and this one have
    /// their lead bytes modified by the signed offset (-0xff..+0xff)
    /// stored in the lower 16 bits.
    ///
    /// CollationData::makeReorderRanges() writes a full list where the
    /// first range (at least for terminators and separators) has a 0
    /// offset. The last range has a non-zero offset. minHighNoReorder
    /// is set to the limit of that last range.
    ///
    /// In the settings object, the initial ranges before the first
    /// split lead byte are omitted for efficiency; they are handled
    /// by reorder(p) via the reorderTable. If there are no
    /// split-reordered lead bytes, then no ranges are needed.
    ///
    /// Comment from ICU4C's `collationsettings.h`; names refer to
    /// ICU4C.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub reorder_ranges: ZeroVec<'data, u32>,
}

impl CollationReorderingV1<'_> {
    pub(crate) fn reorder(&self, primary: u32) -> u32 {
        if let Some(b) = self.reorder_table.get((primary >> 24) as usize) {
            if b != 0 || primary <= NO_CE_PRIMARY {
                (u32::from(b) << 24) | (primary & 0x00FFFFFF)
            } else {
                self.reorder_ex(primary)
            }
        } else {
            // GIGO case
            debug_assert!(false);
            primary
        }
    }

    fn reorder_ex(&self, primary: u32) -> u32 {
        if primary >= self.min_high_no_reorder {
            return primary;
        }
        let q = primary | 0xFFFF;
        for &range in self.reorder_ranges.as_ule_slice().iter() {
            let r = u32::from_unaligned(range);
            if q < r {
                return primary.wrapping_add(r << 24);
            }
        }
        // GIGO case
        debug_assert!(false);
        primary
    }
}

/// Each non-alias collation that the data provider knows
/// about explicitly has an data entry at least for this
/// struct.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(
    CollationMetadataV1Marker,
    "collator/meta@1",
    fallback_by = "script",
    attributes_domain = "collator",
))]
#[derive(Debug, PartialEq, Clone, Copy)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_collator::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct CollationMetadataV1 {
    /// See the mask constants in the `impl` block for the
    /// bit layout. The other bits are ignored: They could
    /// be from the future if their semantics such that
    /// old code may ignore them.
    ///
    /// Note: At present, it's bogus for the bit for "upper
    /// first" to be set if "case first" isn't also set.
    /// However, the methods handle this case gracefully,
    /// so there is no need for invariant validation.
    pub bits: u32,
}

impl CollationMetadataV1 {
    const MAX_VARIABLE_MASK: u32 = 0b11;
    const TAILORED_MASK: u32 = 1 << 3;
    const TAILORED_DIACRITICS_MASK: u32 = 1 << 4;
    const REORDERING_MASK: u32 = 1 << 5;
    const LITHUANIAN_DOT_ABOVE_MASK: u32 = 1 << 6;
    const BACWARD_SECOND_LEVEL_MASK: u32 = 1 << 7;
    const ALTERNATE_SHIFTED_MASK: u32 = 1 << 8;
    const CASE_FIRST_MASK: u32 = 1 << 9;
    const UPPER_FIRST_MASK: u32 = 1 << 10;

    #[inline(always)]
    pub(crate) fn max_variable(&self) -> MaxVariable {
        // Safe, because the possible numeric values for `MaxVariable` are from 0 to 3, inclusive,
        // and we take the two low bits.
        unsafe { core::mem::transmute((self.bits & CollationMetadataV1::MAX_VARIABLE_MASK) as u8) }
    }

    #[inline(always)]
    pub(crate) fn tailored(&self) -> bool {
        self.bits & CollationMetadataV1::TAILORED_MASK != 0
    }

    /// Vietnamese and Ewe
    #[inline(always)]
    pub(crate) fn tailored_diacritics(&self) -> bool {
        self.bits & CollationMetadataV1::TAILORED_DIACRITICS_MASK != 0
    }

    /// Lithuanian
    #[inline(always)]
    pub(crate) fn lithuanian_dot_above(&self) -> bool {
        self.bits & CollationMetadataV1::LITHUANIAN_DOT_ABOVE_MASK != 0
    }

    /// Canadian French
    #[inline(always)]
    pub(crate) fn backward_second_level(&self) -> bool {
        self.bits & CollationMetadataV1::BACWARD_SECOND_LEVEL_MASK != 0
    }

    #[inline(always)]
    pub(crate) fn reordering(&self) -> bool {
        self.bits & CollationMetadataV1::REORDERING_MASK != 0
    }

    /// Thai
    #[inline(always)]
    pub(crate) fn alternate_shifted(&self) -> bool {
        self.bits & CollationMetadataV1::ALTERNATE_SHIFTED_MASK != 0
    }

    #[inline(always)]
    pub(crate) fn case_first(&self) -> CaseFirst {
        if self.bits & CollationMetadataV1::CASE_FIRST_MASK != 0 {
            if self.bits & CollationMetadataV1::UPPER_FIRST_MASK != 0 {
                CaseFirst::UpperFirst
            } else {
                CaseFirst::LowerFirst
            }
        } else {
            CaseFirst::Off
        }
    }
}

/// Special primaries associated with the root collation
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(
    CollationSpecialPrimariesV1Marker,
    "collator/prim@1",
    singleton
))]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_collator::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct CollationSpecialPrimariesV1<'data> {
    /// The primaries corresponding to `MaxVariable`
    /// character classes packed so that each fits in
    /// 16 bits. Length must match the number of enum
    /// variants in `MaxVariable`, currently 4.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub last_primaries: ZeroVec<'data, u16>,
    /// The high 8 bits of the numeric primary
    pub numeric_primary: u8,
}

impl CollationSpecialPrimariesV1<'_> {
    #[allow(clippy::unwrap_used)]
    pub(crate) fn last_primary_for_group(&self, max_variable: MaxVariable) -> u32 {
        // `unwrap` is OK, because `Collator::try_new` validates the length.
        //
        // Minus one to generate the right lower 16 bits from the high 16 bits.
        // See parse.cpp in genrb and getLastPrimaryForGroup in ICU4C.
        (u32::from(self.last_primaries.get(max_variable as usize).unwrap()) << 16) - 1
    }
}
