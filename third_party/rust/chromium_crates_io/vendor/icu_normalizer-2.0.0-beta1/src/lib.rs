// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, feature = "std")), no_std)]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
        clippy::exhaustive_structs,
        clippy::exhaustive_enums,
        missing_debug_implementations,
    )
)]
#![warn(missing_docs)]

//! Normalizing text into Unicode Normalization Forms.
//!
//! This module is published as its own crate ([`icu_normalizer`](https://docs.rs/icu_normalizer/latest/icu_normalizer/))
//! and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
//!
//! # Implementation notes
//!
//! The normalizer operates on a lazy iterator over Unicode scalar values (Rust `char`) internally
//! and iterating over guaranteed-valid UTF-8, potentially-invalid UTF-8, and potentially-invalid
//! UTF-16 is a step that doesn’t leak into the normalizer internals. Ill-formed byte sequences are
//! treated as U+FFFD.
//!
//! The normalizer data layout is not based on the ICU4C design at all. Instead, the normalization
//! data layout is a clean-slate design optimized for the concept of fusing the NFD decomposition
//! into the collator. That is, the decomposing normalizer is a by-product of the collator-motivated
//! data layout.
//!
//! Notably, the decomposition data structure is optimized for a starter decomposing to itself,
//! which is the most common case, and for a starter decomposing to a starter and a non-starter
//! on the Basic Multilingual Plane. Notably, in this case, the collator makes use of the
//! knowledge that the second character of such a decomposition is a non-starter. Therefore,
//! decomposition into two starters is handled by generic fallback path that looks the
//! decomposition from an array by offset and length instead of baking a BMP starter pair directly
//! into a trie value.
//!
//! The decompositions into non-starters are hard-coded. At present in Unicode, these appear
//! to be special cases falling into three categories:
//!
//! 1. Deprecated combining marks.
//! 2. Particular Tibetan vowel sings.
//! 3. NFKD only: half-width kana voicing marks.
//!
//! Hopefully Unicode never adds more decompositions into non-starters (other than a character
//! decomposing to itself), but if it does, a code update is needed instead of a mere data update.
//!
//! The composing normalizer builds on the decomposing normalizer by performing the canonical
//! composition post-processing per spec. As an optimization, though, the composing normalizer
//! attempts to pass through already-normalized text consisting of starters that never combine
//! backwards and that map to themselves if followed by a character whose decomposition starts
//! with a starter that never combines backwards.
//!
//! As a difference with ICU4C, the composing normalizer has only the simplest possible
//! passthrough (only one inversion list lookup per character in the best case) and the full
//! decompose-then-canonically-compose behavior, whereas ICU4C has other paths between these
//! extremes. The ICU4X collator doesn't make use of the FCD concept at all in order to avoid
//! doing the work of checking whether the FCD condition holds.

extern crate alloc;

// We don't depend on icu_properties to minimize deps, but we want to be able
// to ensure we're using the right CCC values
macro_rules! ccc {
    ($name:ident, $num:expr) => {{
        const X: CanonicalCombiningClass = {
            #[cfg(feature = "icu_properties")]
            if icu_properties::props::CanonicalCombiningClass::$name.0 != $num {
                panic!("icu_normalizer has incorrect ccc values")
            }
            CanonicalCombiningClass($num)
        };
        X
    }};
}

pub mod properties;
pub mod provider;
pub mod uts46;

use crate::provider::CanonicalCompositionsV1;
use crate::provider::CanonicalDecompositionDataV1Marker;
use crate::provider::CompatibilityDecompositionSupplementV1Marker;
use crate::provider::DecompositionDataV1;
use crate::provider::Uts46DecompositionSupplementV1Marker;
use alloc::string::String;
use alloc::vec::Vec;
use core::char::REPLACEMENT_CHARACTER;
use core::str::from_utf8_unchecked;
use icu_collections::char16trie::Char16Trie;
use icu_collections::char16trie::Char16TrieIterator;
use icu_collections::char16trie::TrieResult;
use icu_collections::codepointtrie::CodePointTrie;
#[cfg(feature = "icu_properties")]
use icu_properties::props::CanonicalCombiningClass;
use icu_provider::prelude::*;
use provider::CanonicalCompositionsV1Marker;
use provider::CanonicalDecompositionTablesV1Marker;
use provider::CompatibilityDecompositionTablesV1Marker;
use provider::DecompositionSupplementV1;
use provider::DecompositionTablesV1;
use smallvec::SmallVec;
use utf16_iter::Utf16CharsEx;
use utf8_iter::Utf8CharsEx;
use write16::Write16;
use zerovec::{zeroslice, ZeroSlice};

/// This type exists as a shim for icu_properties CanonicalCombiningClass when the crate is disabled
/// It should not be exposed to users.
#[cfg(not(feature = "icu_properties"))]
#[derive(Copy, Clone, Eq, PartialEq, PartialOrd, Ord)]
struct CanonicalCombiningClass(pub(crate) u8);

const CCC_NOT_REORDERED: CanonicalCombiningClass = ccc!(NotReordered, 0);
const CCC_ABOVE: CanonicalCombiningClass = ccc!(Above, 230);

#[derive(Debug)]
enum SupplementPayloadHolder {
    Compatibility(DataPayload<CompatibilityDecompositionSupplementV1Marker>),
    Uts46(DataPayload<Uts46DecompositionSupplementV1Marker>),
}

impl SupplementPayloadHolder {
    fn get(&self) -> &DecompositionSupplementV1 {
        match self {
            SupplementPayloadHolder::Compatibility(d) => d.get(),
            SupplementPayloadHolder::Uts46(d) => d.get(),
        }
    }
}

/// Treatment of the ignorable marker (0xFFFFFFFF) in data.
#[derive(Debug, PartialEq, Eq)]
enum IgnorableBehavior {
    /// 0xFFFFFFFF in data is not supported.
    Unsupported,
    /// Ignorables are ignored.
    Ignored,
    /// Ignorables are treated as singleton decompositions
    /// to the REPLACEMENT CHARACTER.
    ReplacementCharacter,
}

/// Number of iterations allowed on the fast path before flushing.
/// Since a typical UTF-16 iteration advances over a 2-byte BMP
/// character, this means two memory pages.
/// Intel Core i7-4770 had the best results between 2 and 4 pages
/// when testing powers of two. Apple M1 didn't seem to care
/// about 1, 2, 4, or 8 pages.
///
/// Curiously, the `str` case does not appear to benefit from
/// similar flushing, though the tested monomorphization never
/// passes an error through from `Write`.
const UTF16_FAST_PATH_FLUSH_THRESHOLD: usize = 4096;

/// Marker for UTS 46 ignorables.
const IGNORABLE_MARKER: u32 = 0xFFFFFFFF;

/// Marker for starters that decompose to themselves but may
/// combine backwards under canonical composition.
/// (Main trie only; not used in the supplementary trie.)
const BACKWARD_COMBINING_STARTER_MARKER: u32 = 1;

/// Magic marker trie value for characters whose decomposition
/// starts with a non-starter. The actual decomposition is
/// hard-coded.
const SPECIAL_NON_STARTER_DECOMPOSITION_MARKER: u32 = 2;

/// `u16` version of the previous marker value.
const SPECIAL_NON_STARTER_DECOMPOSITION_MARKER_U16: u16 = 2;

/// Marker that a complex decomposition isn't round-trippable
/// under re-composition.
///
/// TODO: When taking a data format break, swap this and
/// `BACKWARD_COMBINING_STARTER_DECOMPOSITION_MARKER` around
/// to make backward-combiningness use the same bit in all
/// cases.
const NON_ROUND_TRIP_MARKER: u16 = 0b1;

/// Marker that a complex decomposition starts with a starter
/// that can combine backwards.
const BACKWARD_COMBINING_STARTER_DECOMPOSITION_MARKER: u16 = 0b10;

/// Values above this are treated as a BMP character.
const HIGHEST_MARKER: u16 = NON_ROUND_TRIP_MARKER | BACKWARD_COMBINING_STARTER_DECOMPOSITION_MARKER;

/// Checks if a trie value carries a (non-zero) canonical
/// combining class.
fn trie_value_has_ccc(trie_value: u32) -> bool {
    (trie_value & 0xFFFFFF00) == 0xD800
}

/// Checks if the trie signifies a special non-starter decomposition.
fn trie_value_indicates_special_non_starter_decomposition(trie_value: u32) -> bool {
    trie_value == SPECIAL_NON_STARTER_DECOMPOSITION_MARKER
}

/// Checks if a trie value signifies a character whose decomposition
/// starts with a non-starter.
fn decomposition_starts_with_non_starter(trie_value: u32) -> bool {
    trie_value_has_ccc(trie_value)
        || trie_value_indicates_special_non_starter_decomposition(trie_value)
}

/// Extracts a canonical combining class (possibly zero) from a trie value.
///
/// # Panics
///
/// The trie value must not be one that signifies a special non-starter
/// decomposition. (Debug-only)
fn ccc_from_trie_value(trie_value: u32) -> CanonicalCombiningClass {
    if trie_value_has_ccc(trie_value) {
        CanonicalCombiningClass(trie_value as u8)
    } else {
        debug_assert_ne!(trie_value, SPECIAL_NON_STARTER_DECOMPOSITION_MARKER);
        CCC_NOT_REORDERED
    }
}

/// The tail (everything after the first character) of the NFKD form U+FDFA
/// as 16-bit units.
static FDFA_NFKD: [u16; 17] = [
    0x644, 0x649, 0x20, 0x627, 0x644, 0x644, 0x647, 0x20, 0x639, 0x644, 0x64A, 0x647, 0x20, 0x648,
    0x633, 0x644, 0x645,
];

/// Marker value for U+FDFA in NFKD
const FDFA_MARKER: u16 = 3;

// These constants originate from page 143 of Unicode 14.0
/// Syllable base
const HANGUL_S_BASE: u32 = 0xAC00;
/// Lead jamo base
const HANGUL_L_BASE: u32 = 0x1100;
/// Vowel jamo base
const HANGUL_V_BASE: u32 = 0x1161;
/// Trail jamo base (deliberately off by one to account for the absence of a trail)
const HANGUL_T_BASE: u32 = 0x11A7;
/// Lead jamo count
const HANGUL_L_COUNT: u32 = 19;
/// Vowel jamo count
const HANGUL_V_COUNT: u32 = 21;
/// Trail jamo count (deliberately off by one to account for the absence of a trail)
const HANGUL_T_COUNT: u32 = 28;
/// Vowel jamo count times trail jamo count
const HANGUL_N_COUNT: u32 = 588;
/// Syllable count
const HANGUL_S_COUNT: u32 = 11172;

/// One past the conjoining jamo block
const HANGUL_JAMO_LIMIT: u32 = 0x1200;

/// If `opt` is `Some`, unwrap it. If `None`, panic if debug assertions
/// are enabled and return `default` if debug assertions are not enabled.
///
/// Use this only if the only reason why `opt` could be `None` is bogus
/// data from the provider.
#[inline(always)]
fn unwrap_or_gigo<T>(opt: Option<T>, default: T) -> T {
    if let Some(val) = opt {
        val
    } else {
        // GIGO case
        debug_assert!(false);
        default
    }
}

/// Convert a `u32` _obtained from data provider data_ to `char`.
#[inline(always)]
fn char_from_u32(u: u32) -> char {
    unwrap_or_gigo(core::char::from_u32(u), REPLACEMENT_CHARACTER)
}

/// Convert a `u16` _obtained from data provider data_ to `char`.
#[inline(always)]
fn char_from_u16(u: u16) -> char {
    char_from_u32(u32::from(u))
}

const EMPTY_U16: &ZeroSlice<u16> = zeroslice![];

const EMPTY_CHAR: &ZeroSlice<char> = zeroslice![];

#[inline(always)]
fn in_inclusive_range(c: char, start: char, end: char) -> bool {
    u32::from(c).wrapping_sub(u32::from(start)) <= (u32::from(end) - u32::from(start))
}

#[inline(always)]
fn in_inclusive_range32(u: u32, start: u32, end: u32) -> bool {
    u.wrapping_sub(start) <= (end - start)
}

#[inline(always)]
fn in_inclusive_range16(u: u16, start: u16, end: u16) -> bool {
    u.wrapping_sub(start) <= (end - start)
}

/// Performs canonical composition (including Hangul) on a pair of
/// characters or returns `None` if these characters don't compose.
/// Composition exclusions are taken into account.
#[inline]
fn compose(iter: Char16TrieIterator, starter: char, second: char) -> Option<char> {
    let v = u32::from(second).wrapping_sub(HANGUL_V_BASE);
    if v >= HANGUL_JAMO_LIMIT - HANGUL_V_BASE {
        return compose_non_hangul(iter, starter, second);
    }
    if v < HANGUL_V_COUNT {
        let l = u32::from(starter).wrapping_sub(HANGUL_L_BASE);
        if l < HANGUL_L_COUNT {
            let lv = l * HANGUL_N_COUNT + v * HANGUL_T_COUNT;
            // Safe, because the inputs are known to be in range.
            return Some(unsafe { char::from_u32_unchecked(HANGUL_S_BASE + lv) });
        }
        return None;
    }
    if in_inclusive_range(second, '\u{11A8}', '\u{11C2}') {
        let lv = u32::from(starter).wrapping_sub(HANGUL_S_BASE);
        if lv < HANGUL_S_COUNT && lv % HANGUL_T_COUNT == 0 {
            let lvt = lv + (u32::from(second) - HANGUL_T_BASE);
            // Safe, because the inputs are known to be in range.
            return Some(unsafe { char::from_u32_unchecked(HANGUL_S_BASE + lvt) });
        }
    }
    None
}

/// Performs (non-Hangul) canonical composition on a pair of characters
/// or returns `None` if these characters don't compose. Composition
/// exclusions are taken into account.
fn compose_non_hangul(mut iter: Char16TrieIterator, starter: char, second: char) -> Option<char> {
    // To make the trie smaller, the pairs are stored second character first.
    // Given how this method is used in ways where it's known that `second`
    // is or isn't a starter. We could potentially split the trie into two
    // tries depending on whether `second` is a starter.
    match iter.next(second) {
        TrieResult::NoMatch => None,
        TrieResult::NoValue => match iter.next(starter) {
            TrieResult::NoMatch => None,
            TrieResult::FinalValue(i) => {
                if let Some(c) = char::from_u32(i as u32) {
                    Some(c)
                } else {
                    // GIGO case
                    debug_assert!(false);
                    None
                }
            }
            TrieResult::NoValue | TrieResult::Intermediate(_) => {
                // GIGO case
                debug_assert!(false);
                None
            }
        },
        TrieResult::FinalValue(_) | TrieResult::Intermediate(_) => {
            // GIGO case
            debug_assert!(false);
            None
        }
    }
}

/// Struct for holding together a character and the value
/// looked up for it from the NFD trie in a more explicit
/// way than an anonymous pair.
/// Also holds a flag about the supplementary-trie provenance.
#[derive(Debug, PartialEq, Eq)]
struct CharacterAndTrieValue {
    character: char,
    trie_val: u32,
    from_supplement: bool,
}

impl CharacterAndTrieValue {
    #[inline(always)]
    pub fn new(c: char, trie_value: u32) -> Self {
        CharacterAndTrieValue {
            character: c,
            trie_val: trie_value,
            from_supplement: false,
        }
    }
    #[inline(always)]
    pub fn new_from_supplement(c: char, trie_value: u32) -> Self {
        CharacterAndTrieValue {
            character: c,
            trie_val: trie_value,
            from_supplement: true,
        }
    }
    #[inline(always)]
    pub fn starter_and_decomposes_to_self(&self) -> bool {
        if self.trie_val > BACKWARD_COMBINING_STARTER_MARKER {
            return false;
        }
        // Hangul syllables get 0 as their trie value
        u32::from(self.character).wrapping_sub(HANGUL_S_BASE) >= HANGUL_S_COUNT
    }
    #[inline(always)]
    pub fn can_combine_backwards(&self) -> bool {
        decomposition_starts_with_non_starter(self.trie_val)
            || self.trie_val == BACKWARD_COMBINING_STARTER_MARKER
            || (((self.trie_val as u16) & !1) == BACKWARD_COMBINING_STARTER_DECOMPOSITION_MARKER && (self.trie_val >> 16) != 0) // Combine with the previous condition when taking a data format break
            || in_inclusive_range32(self.trie_val, 0x1161, 0x11C2)
    }
    #[inline(always)]
    pub fn potential_passthrough(&self) -> bool {
        self.potential_passthrough_impl(BACKWARD_COMBINING_STARTER_MARKER)
    }
    #[inline(always)]
    pub fn potential_passthrough_and_cannot_combine_backwards(&self) -> bool {
        self.potential_passthrough_impl(0)
    }
    #[inline(always)]
    fn potential_passthrough_impl(&self, bound: u32) -> bool {
        // This methods looks badly branchy, but most characters
        // take the first return.
        if self.trie_val <= bound {
            return true;
        }
        if self.from_supplement {
            return false;
        }
        let trail_or_complex = (self.trie_val >> 16) as u16;
        if trail_or_complex == 0 {
            return false;
        }
        let lead = self.trie_val as u16;
        if lead == 0 {
            return true;
        }
        if lead <= HIGHEST_MARKER {
            return false;
        }
        if (trail_or_complex & 0x7F) == 0x3C
            && in_inclusive_range16(trail_or_complex, 0x0900, 0x0BFF)
        {
            // Nukta
            return false;
        }
        if in_inclusive_range(self.character, '\u{FB1D}', '\u{FB4E}') {
            // Hebrew presentation forms
            return false;
        }
        if in_inclusive_range(self.character, '\u{1F71}', '\u{1FFB}') {
            // Polytonic Greek with oxia
            return false;
        }
        // To avoid more branchiness, 4 characters that decompose to
        // a BMP starter followed by a BMP non-starter are excluded
        // from being encoded directly into the trie value and are
        // handled as complex decompositions instead. These are:
        // U+0F76 TIBETAN VOWEL SIGN VOCALIC R
        // U+0F78 TIBETAN VOWEL SIGN VOCALIC L
        // U+212B ANGSTROM SIGN
        // U+2ADC FORKING
        true
    }
}

/// Pack a `char` and a `CanonicalCombiningClass` in
/// 32 bits (the former in the lower 24 bits and the
/// latter in the high 8 bits). The latter can be
/// initialized to 0xFF upon creation, in which case
/// it can be actually set later by calling
/// `set_ccc_from_trie_if_not_already_set`. This is
/// a micro optimization to avoid the Canonical
/// Combining Class trie lookup when there is only
/// one combining character in a sequence. This type
/// is intentionally non-`Copy` to get compiler help
/// in making sure that the class is set on the
/// instance on which it is intended to be set
/// and not on a temporary copy.
///
/// Note that 0xFF is won't be assigned to an actual
/// canonical combining class per definition D104
/// in The Unicode Standard.
//
// NOTE: The Pernosco debugger has special knowledge
// of this struct. Please do not change the bit layout
// or the crate-module-qualified name of this struct
// without coordination.
#[derive(Debug)]
struct CharacterAndClass(u32);

impl CharacterAndClass {
    pub fn new(c: char, ccc: CanonicalCombiningClass) -> Self {
        CharacterAndClass(u32::from(c) | (u32::from(ccc.0) << 24))
    }
    pub fn new_with_placeholder(c: char) -> Self {
        CharacterAndClass(u32::from(c) | ((0xFF) << 24))
    }
    pub fn new_with_trie_value(c_tv: CharacterAndTrieValue) -> Self {
        Self::new(c_tv.character, ccc_from_trie_value(c_tv.trie_val))
    }
    pub fn new_starter(c: char) -> Self {
        CharacterAndClass(u32::from(c))
    }
    /// This method must exist for Pernosco to apply its special rendering.
    /// Also, this must not be dead code!
    pub fn character(&self) -> char {
        // Safe, because the low 24 bits came from a `char`
        // originally.
        unsafe { char::from_u32_unchecked(self.0 & 0xFFFFFF) }
    }
    /// This method must exist for Pernosco to apply its special rendering.
    pub fn ccc(&self) -> CanonicalCombiningClass {
        CanonicalCombiningClass((self.0 >> 24) as u8)
    }

    pub fn character_and_ccc(&self) -> (char, CanonicalCombiningClass) {
        (self.character(), self.ccc())
    }
    pub fn set_ccc_from_trie_if_not_already_set(&mut self, trie: &CodePointTrie<u32>) {
        if self.0 >> 24 != 0xFF {
            return;
        }
        let scalar = self.0 & 0xFFFFFF;
        self.0 = ((ccc_from_trie_value(trie.get32_u32(scalar)).0 as u32) << 24) | scalar;
    }
}

// This function exists as a borrow check helper.
#[inline(always)]
fn sort_slice_by_ccc(slice: &mut [CharacterAndClass], trie: &CodePointTrie<u32>) {
    // We don't look up the canonical combining class for starters
    // of for single combining characters between starters. When
    // there's more than one combining character between starters,
    // we look up the canonical combining class for each character
    // exactly once.
    if slice.len() < 2 {
        return;
    }
    slice
        .iter_mut()
        .for_each(|cc| cc.set_ccc_from_trie_if_not_already_set(trie));
    slice.sort_by_key(|cc| cc.ccc());
}

/// An iterator adaptor that turns an `Iterator` over `char` into
/// a lazily-decomposed `char` sequence.
#[derive(Debug)]
pub struct Decomposition<'data, I>
where
    I: Iterator<Item = char>,
{
    delegate: I,
    buffer: SmallVec<[CharacterAndClass; 17]>, // Enough to hold NFKD for U+FDFA
    /// The index of the next item to be read from `buffer`.
    /// The purpose if this index is to avoid having to move
    /// the rest upon every read.
    buffer_pos: usize,
    // At the start of `next()` if not `None`, this is a pending unnormalized
    // starter. When `Decomposition` appears alone, this is never a non-starter.
    // However, when `Decomposition` appears inside a `Composition`, this
    // may become a non-starter before `decomposing_next()` is called.
    pending: Option<CharacterAndTrieValue>, // None at end of stream
    trie: &'data CodePointTrie<'data, u32>,
    supplementary_trie: Option<&'data CodePointTrie<'data, u32>>,
    scalars16: &'data ZeroSlice<u16>,
    scalars24: &'data ZeroSlice<char>,
    supplementary_scalars16: &'data ZeroSlice<u16>,
    supplementary_scalars24: &'data ZeroSlice<char>,
    half_width_voicing_marks_become_non_starters: bool,
    /// The lowest character for which either of the following does
    /// not hold:
    /// 1. Decomposes to self.
    /// 2. Decomposition starts with a non-starter
    decomposition_passthrough_bound: u32, // never above 0xC0
    ignorable_behavior: IgnorableBehavior, // Arguably should be a type parameter
}

impl<'data, I> Decomposition<'data, I>
where
    I: Iterator<Item = char>,
{
    /// Constructs a decomposing iterator adapter from a delegate
    /// iterator and references to the necessary data, without
    /// supplementary data.
    ///
    /// Use `DecomposingNormalizer::normalize_iter()` instead unless
    /// there's a good reason to use this constructor directly.
    ///
    /// Public but hidden in order to be able to use this from the
    /// collator.
    #[doc(hidden)] // used in collator
    pub fn new(
        delegate: I,
        decompositions: &'data DecompositionDataV1,
        tables: &'data DecompositionTablesV1,
    ) -> Self {
        Self::new_with_supplements(
            delegate,
            decompositions,
            None,
            tables,
            None,
            0xC0,
            IgnorableBehavior::Unsupported,
        )
    }

    /// Constructs a decomposing iterator adapter from a delegate
    /// iterator and references to the necessary data, including
    /// supplementary data.
    ///
    /// Use `DecomposingNormalizer::normalize_iter()` instead unless
    /// there's a good reason to use this constructor directly.
    fn new_with_supplements(
        delegate: I,
        decompositions: &'data DecompositionDataV1,
        supplementary_decompositions: Option<&'data DecompositionSupplementV1>,
        tables: &'data DecompositionTablesV1,
        supplementary_tables: Option<&'data DecompositionTablesV1>,
        decomposition_passthrough_bound: u8,
        ignorable_behavior: IgnorableBehavior,
    ) -> Self {
        let half_width_voicing_marks_become_non_starters =
            if let Some(supplementary) = supplementary_decompositions {
                supplementary.half_width_voicing_marks_become_non_starters()
            } else {
                false
            };
        let mut ret = Decomposition::<I> {
            delegate,
            buffer: SmallVec::new(), // Normalized
            buffer_pos: 0,
            // Initialize with a placeholder starter in case
            // the real stream starts with a non-starter.
            pending: Some(CharacterAndTrieValue::new('\u{FFFF}', 0)),
            trie: &decompositions.trie,
            supplementary_trie: supplementary_decompositions.map(|s| &s.trie),
            scalars16: &tables.scalars16,
            scalars24: &tables.scalars24,
            supplementary_scalars16: if let Some(supplementary) = supplementary_tables {
                &supplementary.scalars16
            } else {
                EMPTY_U16
            },
            supplementary_scalars24: if let Some(supplementary) = supplementary_tables {
                &supplementary.scalars24
            } else {
                EMPTY_CHAR
            },
            half_width_voicing_marks_become_non_starters,
            decomposition_passthrough_bound: u32::from(decomposition_passthrough_bound),
            ignorable_behavior,
        };
        let _ = ret.next(); // Remove the U+FFFF placeholder
        ret
    }

    fn push_decomposition16(
        &mut self,
        low: u16,
        offset: usize,
        slice16: &ZeroSlice<u16>,
    ) -> (char, usize) {
        let len = usize::from(low >> 13) + 2;
        let (starter, tail) = slice16
            .get_subslice(offset..offset + len)
            .and_then(|slice| slice.split_first())
            .map_or_else(
                || {
                    // GIGO case
                    debug_assert!(false);
                    (REPLACEMENT_CHARACTER, EMPTY_U16)
                },
                |(first, trail)| (char_from_u16(first), trail),
            );
        if low & 0x1000 != 0 {
            // All the rest are combining
            self.buffer.extend(
                tail.iter()
                    .map(|u| CharacterAndClass::new_with_placeholder(char_from_u16(u))),
            );
            (starter, 0)
        } else {
            let mut i = 0;
            let mut combining_start = 0;
            for u in tail.iter() {
                let ch = char_from_u16(u);
                let trie_value = self.trie.get(ch);
                self.buffer.push(CharacterAndClass::new_with_trie_value(
                    CharacterAndTrieValue::new(ch, trie_value),
                ));
                i += 1;
                // Half-width kana and iota subscript don't occur in the tails
                // of these multicharacter decompositions.
                if !decomposition_starts_with_non_starter(trie_value) {
                    combining_start = i;
                }
            }
            (starter, combining_start)
        }
    }

    fn push_decomposition32(
        &mut self,
        low: u16,
        offset: usize,
        slice32: &ZeroSlice<char>,
    ) -> (char, usize) {
        let len = usize::from(low >> 13) + 1;
        let (starter, tail) = slice32
            .get_subslice(offset..offset + len)
            .and_then(|slice| slice.split_first())
            .unwrap_or_else(|| {
                // GIGO case
                debug_assert!(false);
                (REPLACEMENT_CHARACTER, EMPTY_CHAR)
            });
        if low & 0x1000 != 0 {
            // All the rest are combining
            self.buffer
                .extend(tail.iter().map(CharacterAndClass::new_with_placeholder));
            (starter, 0)
        } else {
            let mut i = 0;
            let mut combining_start = 0;
            for ch in tail.iter() {
                let trie_value = self.trie.get(ch);
                self.buffer.push(CharacterAndClass::new_with_trie_value(
                    CharacterAndTrieValue::new(ch, trie_value),
                ));
                i += 1;
                // Half-width kana and iota subscript don't occur in the tails
                // of these multicharacter decompositions.
                if !decomposition_starts_with_non_starter(trie_value) {
                    combining_start = i;
                }
            }
            (starter, combining_start)
        }
    }

    #[inline(always)]
    fn attach_trie_value(&self, c: char) -> CharacterAndTrieValue {
        if let Some(supplementary) = self.supplementary_trie {
            if let Some(value) = self.attach_supplementary_trie_value(c, supplementary) {
                return value;
            }
        }

        CharacterAndTrieValue::new(c, self.trie.get(c))
    }

    #[inline(never)]
    fn attach_supplementary_trie_value(
        &self,
        c: char,
        supplementary: &CodePointTrie<u32>,
    ) -> Option<CharacterAndTrieValue> {
        let voicing_mark = u32::from(c).wrapping_sub(0xFF9E);
        if voicing_mark <= 1 && self.half_width_voicing_marks_become_non_starters {
            return Some(CharacterAndTrieValue::new(
                if voicing_mark == 0 {
                    '\u{3099}'
                } else {
                    '\u{309A}'
                },
                0xD800 | ccc!(KanaVoicing, 8).0 as u32,
            ));
        }
        let trie_value = supplementary.get32(u32::from(c));
        if trie_value != 0 {
            return Some(CharacterAndTrieValue::new_from_supplement(c, trie_value));
        }
        None
    }

    fn delegate_next_no_pending(&mut self) -> Option<CharacterAndTrieValue> {
        debug_assert!(self.pending.is_none());
        loop {
            let c = self.delegate.next()?;

            // TODO(#2384): Measure if this check is actually an optimization even in the
            // non-supplementary case of if this should go inside the supplementary
            // `if` below.
            if u32::from(c) < self.decomposition_passthrough_bound {
                return Some(CharacterAndTrieValue::new(c, 0));
            }

            if let Some(supplementary) = self.supplementary_trie {
                if let Some(value) = self.attach_supplementary_trie_value(c, supplementary) {
                    if value.trie_val == IGNORABLE_MARKER {
                        match self.ignorable_behavior {
                            IgnorableBehavior::Unsupported => {
                                debug_assert!(false);
                            }
                            IgnorableBehavior::ReplacementCharacter => {
                                return Some(CharacterAndTrieValue::new(
                                    c,
                                    u32::from(REPLACEMENT_CHARACTER),
                                ));
                            }
                            IgnorableBehavior::Ignored => {
                                // Else ignore this character by reading the next one from the delegate.
                                continue;
                            }
                        }
                    }
                    return Some(value);
                }
            }
            let trie_val = self.trie.get(c);
            debug_assert_ne!(trie_val, IGNORABLE_MARKER);
            return Some(CharacterAndTrieValue::new(c, trie_val));
        }
    }

    fn delegate_next(&mut self) -> Option<CharacterAndTrieValue> {
        if let Some(pending) = self.pending.take() {
            // Only happens as part of `Composition` and as part of
            // the contiguous-buffer methods of `DecomposingNormalizer`.
            // I.e. does not happen as part of standalone iterator
            // usage of `Decomposition`.
            Some(pending)
        } else {
            self.delegate_next_no_pending()
        }
    }

    fn decomposing_next(&mut self, c_and_trie_val: CharacterAndTrieValue) -> char {
        let (starter, combining_start) = {
            let c = c_and_trie_val.character;
            let hangul_offset = u32::from(c).wrapping_sub(HANGUL_S_BASE); // SIndex in the spec
            if hangul_offset >= HANGUL_S_COUNT {
                let decomposition = c_and_trie_val.trie_val;
                if decomposition <= BACKWARD_COMBINING_STARTER_MARKER {
                    // The character is its own decomposition
                    (c, 0)
                } else {
                    let trail_or_complex = (decomposition >> 16) as u16;
                    let lead = decomposition as u16;
                    if lead > HIGHEST_MARKER && trail_or_complex != 0 {
                        // Decomposition into two BMP characters: starter and non-starter
                        let starter = char_from_u16(lead);
                        let combining = char_from_u16(trail_or_complex);
                        self.buffer
                            .push(CharacterAndClass::new_with_placeholder(combining));
                        (starter, 0)
                    } else if trail_or_complex == 0 {
                        if lead != FDFA_MARKER {
                            debug_assert_ne!(
                                lead, SPECIAL_NON_STARTER_DECOMPOSITION_MARKER_U16,
                                "Should not reach this point with non-starter marker"
                            );
                            // Decomposition into one BMP character
                            let starter = char_from_u16(lead);
                            (starter, 0)
                        } else {
                            // Special case for the NFKD form of U+FDFA.
                            self.buffer.extend(FDFA_NFKD.map(|u| {
                                // Safe, because `FDFA_NFKD` is known not to contain
                                // surrogates.
                                CharacterAndClass::new_starter(unsafe {
                                    core::char::from_u32_unchecked(u32::from(u))
                                })
                            }));
                            ('\u{0635}', 17)
                        }
                    } else {
                        // Complex decomposition
                        // Format for 16-bit value:
                        // 15..13: length minus two for 16-bit case and length minus one for
                        //         the 32-bit case. Length 8 needs to fit in three bits in
                        //         the 16-bit case, and this way the value is future-proofed
                        //         up to 9 in the 16-bit case. Zero is unused and length one
                        //         in the 16-bit case goes directly into the trie.
                        //     12: 1 if all trailing characters are guaranteed non-starters,
                        //         0 if no guarantees about non-starterness.
                        //         Note: The bit choice is this way around to allow for
                        //         dynamically falling back to not having this but instead
                        //         having one more bit for length by merely choosing
                        //         different masks.
                        //  11..0: Start offset in storage. The offset is to the logical
                        //         sequence of scalars16, scalars32, supplementary_scalars16,
                        //         supplementary_scalars32.
                        let offset = usize::from(trail_or_complex & 0xFFF);
                        if offset < self.scalars16.len() {
                            self.push_decomposition16(trail_or_complex, offset, self.scalars16)
                        } else if offset < self.scalars16.len() + self.scalars24.len() {
                            self.push_decomposition32(
                                trail_or_complex,
                                offset - self.scalars16.len(),
                                self.scalars24,
                            )
                        } else if offset
                            < self.scalars16.len()
                                + self.scalars24.len()
                                + self.supplementary_scalars16.len()
                        {
                            self.push_decomposition16(
                                trail_or_complex,
                                offset - (self.scalars16.len() + self.scalars24.len()),
                                self.supplementary_scalars16,
                            )
                        } else {
                            self.push_decomposition32(
                                trail_or_complex,
                                offset
                                    - (self.scalars16.len()
                                        + self.scalars24.len()
                                        + self.supplementary_scalars16.len()),
                                self.supplementary_scalars24,
                            )
                        }
                    }
                }
            } else {
                // Hangul syllable
                // The math here comes from page 144 of Unicode 14.0
                let l = hangul_offset / HANGUL_N_COUNT;
                let v = (hangul_offset % HANGUL_N_COUNT) / HANGUL_T_COUNT;
                let t = hangul_offset % HANGUL_T_COUNT;

                // The unsafe blocks here are OK, because the values stay
                // within the Hangul jamo block and, therefore, the scalar
                // value range by construction.
                self.buffer.push(CharacterAndClass::new_starter(unsafe {
                    core::char::from_u32_unchecked(HANGUL_V_BASE + v)
                }));
                let first = unsafe { core::char::from_u32_unchecked(HANGUL_L_BASE + l) };
                if t != 0 {
                    self.buffer.push(CharacterAndClass::new_starter(unsafe {
                        core::char::from_u32_unchecked(HANGUL_T_BASE + t)
                    }));
                    (first, 2)
                } else {
                    (first, 1)
                }
            }
        };
        // Either we're inside `Composition` or `self.pending.is_none()`.

        self.gather_and_sort_combining(combining_start);
        starter
    }

    fn gather_and_sort_combining(&mut self, combining_start: usize) {
        // Not a `for` loop to avoid holding a mutable reference to `self` across
        // the loop body.
        while let Some(ch_and_trie_val) = self.delegate_next() {
            if trie_value_has_ccc(ch_and_trie_val.trie_val) {
                self.buffer
                    .push(CharacterAndClass::new_with_trie_value(ch_and_trie_val));
            } else if trie_value_indicates_special_non_starter_decomposition(
                ch_and_trie_val.trie_val,
            ) {
                // The Tibetan special cases are starters that decompose into non-starters.
                let mapped = match ch_and_trie_val.character {
                    '\u{0340}' => {
                        // COMBINING GRAVE TONE MARK
                        CharacterAndClass::new('\u{0300}', CCC_ABOVE)
                    }
                    '\u{0341}' => {
                        // COMBINING ACUTE TONE MARK
                        CharacterAndClass::new('\u{0301}', CCC_ABOVE)
                    }
                    '\u{0343}' => {
                        // COMBINING GREEK KORONIS
                        CharacterAndClass::new('\u{0313}', CCC_ABOVE)
                    }
                    '\u{0344}' => {
                        // COMBINING GREEK DIALYTIKA TONOS
                        self.buffer
                            .push(CharacterAndClass::new('\u{0308}', CCC_ABOVE));
                        CharacterAndClass::new('\u{0301}', CCC_ABOVE)
                    }
                    '\u{0F73}' => {
                        // TIBETAN VOWEL SIGN II
                        self.buffer
                            .push(CharacterAndClass::new('\u{0F71}', ccc!(CCC129, 129)));
                        CharacterAndClass::new('\u{0F72}', ccc!(CCC130, 130))
                    }
                    '\u{0F75}' => {
                        // TIBETAN VOWEL SIGN UU
                        self.buffer
                            .push(CharacterAndClass::new('\u{0F71}', ccc!(CCC129, 129)));
                        CharacterAndClass::new('\u{0F74}', ccc!(CCC132, 132))
                    }
                    '\u{0F81}' => {
                        // TIBETAN VOWEL SIGN REVERSED II
                        self.buffer
                            .push(CharacterAndClass::new('\u{0F71}', ccc!(CCC129, 129)));
                        CharacterAndClass::new('\u{0F80}', ccc!(CCC130, 130))
                    }
                    _ => {
                        // GIGO case
                        debug_assert!(false);
                        CharacterAndClass::new_with_placeholder(REPLACEMENT_CHARACTER)
                    }
                };
                self.buffer.push(mapped);
            } else {
                self.pending = Some(ch_and_trie_val);
                break;
            }
        }
        // Slicing succeeds by construction; we've always ensured that `combining_start`
        // is in permissible range.
        #[allow(clippy::indexing_slicing)]
        sort_slice_by_ccc(&mut self.buffer[combining_start..], self.trie);
    }
}

impl<I> Iterator for Decomposition<'_, I>
where
    I: Iterator<Item = char>,
{
    type Item = char;

    fn next(&mut self) -> Option<char> {
        if let Some(ret) = self.buffer.get(self.buffer_pos).map(|c| c.character()) {
            self.buffer_pos += 1;
            if self.buffer_pos == self.buffer.len() {
                self.buffer.clear();
                self.buffer_pos = 0;
            }
            return Some(ret);
        }
        debug_assert_eq!(self.buffer_pos, 0);
        let c_and_trie_val = self.pending.take()?;
        Some(self.decomposing_next(c_and_trie_val))
    }
}

/// An iterator adaptor that turns an `Iterator` over `char` into
/// a lazily-decomposed and then canonically composed `char` sequence.
#[derive(Debug)]
pub struct Composition<'data, I>
where
    I: Iterator<Item = char>,
{
    /// The decomposing part of the normalizer than operates before
    /// the canonical composition is performed on its output.
    decomposition: Decomposition<'data, I>,
    /// Non-Hangul canonical composition data.
    canonical_compositions: Char16Trie<'data>,
    /// To make `next()` yield in cases where there's a non-composing
    /// starter in the decomposition buffer, we put it here to let it
    /// wait for the next `next()` call (or a jump forward within the
    /// `next()` call).
    unprocessed_starter: Option<char>,
    /// The lowest character for which any one of the following does
    /// not hold:
    /// 1. Roundtrips via decomposition and recomposition.
    /// 2. Decomposition starts with a non-starter
    /// 3. Is not a backward-combining starter
    composition_passthrough_bound: u32,
}

impl<'data, I> Composition<'data, I>
where
    I: Iterator<Item = char>,
{
    fn new(
        decomposition: Decomposition<'data, I>,
        canonical_compositions: Char16Trie<'data>,
        composition_passthrough_bound: u16,
    ) -> Self {
        Self {
            decomposition,
            canonical_compositions,
            unprocessed_starter: None,
            composition_passthrough_bound: u32::from(composition_passthrough_bound),
        }
    }

    /// Performs canonical composition (including Hangul) on a pair of
    /// characters or returns `None` if these characters don't compose.
    /// Composition exclusions are taken into account.
    #[inline(always)]
    pub fn compose(&self, starter: char, second: char) -> Option<char> {
        compose(self.canonical_compositions.iter(), starter, second)
    }

    /// Performs (non-Hangul) canonical composition on a pair of characters
    /// or returns `None` if these characters don't compose. Composition
    /// exclusions are taken into account.
    #[inline(always)]
    fn compose_non_hangul(&self, starter: char, second: char) -> Option<char> {
        compose_non_hangul(self.canonical_compositions.iter(), starter, second)
    }
}

impl<I> Iterator for Composition<'_, I>
where
    I: Iterator<Item = char>,
{
    type Item = char;

    #[inline]
    fn next(&mut self) -> Option<char> {
        let mut undecomposed_starter = CharacterAndTrieValue::new('\u{0}', 0); // The compiler can't figure out that this gets overwritten before use.
        if self.unprocessed_starter.is_none() {
            // The loop is only broken out of as goto forward
            #[allow(clippy::never_loop)]
            loop {
                if let Some((character, ccc)) = self
                    .decomposition
                    .buffer
                    .get(self.decomposition.buffer_pos)
                    .map(|c| c.character_and_ccc())
                {
                    self.decomposition.buffer_pos += 1;
                    if self.decomposition.buffer_pos == self.decomposition.buffer.len() {
                        self.decomposition.buffer.clear();
                        self.decomposition.buffer_pos = 0;
                    }
                    if ccc == CCC_NOT_REORDERED {
                        // Previous decomposition contains a starter. This must
                        // now become the `unprocessed_starter` for it to have
                        // a chance to compose with the upcoming characters.
                        //
                        // E.g. parenthesized Hangul in NFKC comes through here,
                        // but suitable composition exclusion could exercise this
                        // in NFC.
                        self.unprocessed_starter = Some(character);
                        break; // We already have a starter, so skip taking one from `pending`.
                    }
                    return Some(character);
                }
                debug_assert_eq!(self.decomposition.buffer_pos, 0);
                undecomposed_starter = self.decomposition.pending.take()?;
                if u32::from(undecomposed_starter.character) < self.composition_passthrough_bound
                    || undecomposed_starter.potential_passthrough()
                {
                    // TODO(#2385): In the NFC case (moot for NFKC and UTS46), if the upcoming
                    // character is not below `decomposition_passthrough_bound` but is
                    // below `composition_passthrough_bound`, we read from the trie
                    // unnecessarily.
                    if let Some(upcoming) = self.decomposition.delegate_next_no_pending() {
                        let cannot_combine_backwards = u32::from(upcoming.character)
                            < self.composition_passthrough_bound
                            || !upcoming.can_combine_backwards();
                        self.decomposition.pending = Some(upcoming);
                        if cannot_combine_backwards {
                            // Fast-track succeeded!
                            return Some(undecomposed_starter.character);
                        }
                    } else {
                        // End of stream
                        return Some(undecomposed_starter.character);
                    }
                }
                break; // Not actually looping
            }
        }
        let mut starter = '\u{0}'; // The compiler can't figure out this gets overwritten before use.

        // The point of having this boolean is to have only one call site to
        // `self.decomposition.decomposing_next`, which is hopefully beneficial for
        // code size under inlining.
        let mut attempt_composition = false;
        loop {
            if let Some(unprocessed) = self.unprocessed_starter.take() {
                debug_assert_eq!(undecomposed_starter, CharacterAndTrieValue::new('\u{0}', 0));
                debug_assert_eq!(starter, '\u{0}');
                starter = unprocessed;
            } else {
                debug_assert_eq!(self.decomposition.buffer_pos, 0);
                let next_starter = self.decomposition.decomposing_next(undecomposed_starter);
                if !attempt_composition {
                    starter = next_starter;
                } else if let Some(composed) = self.compose(starter, next_starter) {
                    starter = composed;
                } else {
                    // This is our yield point. We'll pick this up above in the
                    // next call to `next()`.
                    self.unprocessed_starter = Some(next_starter);
                    return Some(starter);
                }
            }
            // We first loop by index to avoid moving the contents of `buffer`, but
            // if there's a discontiguous match, we'll start modifying `buffer` instead.
            loop {
                let (character, ccc) = if let Some((character, ccc)) = self
                    .decomposition
                    .buffer
                    .get(self.decomposition.buffer_pos)
                    .map(|c| c.character_and_ccc())
                {
                    (character, ccc)
                } else {
                    self.decomposition.buffer.clear();
                    self.decomposition.buffer_pos = 0;
                    break;
                };
                if let Some(composed) = self.compose(starter, character) {
                    starter = composed;
                    self.decomposition.buffer_pos += 1;
                    continue;
                }
                let mut most_recent_skipped_ccc = ccc;
                {
                    let _ = self
                        .decomposition
                        .buffer
                        .drain(0..self.decomposition.buffer_pos);
                }
                self.decomposition.buffer_pos = 0;
                if most_recent_skipped_ccc == CCC_NOT_REORDERED {
                    // We failed to compose a starter. Discontiguous match not allowed.
                    // We leave the starter in `buffer` for `next()` to find.
                    return Some(starter);
                }
                let mut i = 1; // We have skipped one non-starter.
                while let Some((character, ccc)) = self
                    .decomposition
                    .buffer
                    .get(i)
                    .map(|c| c.character_and_ccc())
                {
                    if ccc == CCC_NOT_REORDERED {
                        // Discontiguous match not allowed.
                        return Some(starter);
                    }
                    debug_assert!(ccc >= most_recent_skipped_ccc);
                    if ccc != most_recent_skipped_ccc {
                        // Using the non-Hangul version as a micro-optimization, since
                        // we already rejected the case where `second` is a starter
                        // above, and conjoining jamo are starters.
                        if let Some(composed) = self.compose_non_hangul(starter, character) {
                            self.decomposition.buffer.remove(i);
                            starter = composed;
                            continue;
                        }
                    }
                    most_recent_skipped_ccc = ccc;
                    i += 1;
                }
                break;
            }

            debug_assert_eq!(self.decomposition.buffer_pos, 0);

            if !self.decomposition.buffer.is_empty() {
                return Some(starter);
            }
            // Now we need to check if composition with an upcoming starter is possible.
            #[allow(clippy::unwrap_used)]
            if self.decomposition.pending.is_some() {
                // We know that `pending_starter` decomposes to start with a starter.
                // Otherwise, it would have been moved to `self.decomposition.buffer`
                // by `self.decomposing_next()`. We do this set lookup here in order
                // to get an opportunity to go back to the fast track.
                // Note that this check has to happen _after_ checking that `pending`
                // holds a character, because this flag isn't defined to be meaningful
                // when `pending` isn't holding a character.
                let pending = self.decomposition.pending.as_ref().unwrap();
                if u32::from(pending.character) < self.composition_passthrough_bound
                    || !pending.can_combine_backwards()
                {
                    // Won't combine backwards anyway.
                    return Some(starter);
                }
                // Consume what we peeked. `unwrap` OK, because we checked `is_some()`
                // above.
                undecomposed_starter = self.decomposition.pending.take().unwrap();
                // The following line is OK, because we're about to loop back
                // to `self.decomposition.decomposing_next(c);`, which will
                // restore the between-`next()`-calls invariant of `pending`
                // before this function returns.
                attempt_composition = true;
                continue;
            }
            // End of input
            return Some(starter);
        }
    }
}

macro_rules! composing_normalize_to {
    ($(#[$meta:meta])*,
     $normalize_to:ident,
     $write:path,
     $slice:ty,
     $prolog:block,
     $always_valid_utf:literal,
     $as_slice:ident,
     $fast:block,
     $text:ident,
     $sink:ident,
     $composition:ident,
     $composition_passthrough_bound:ident,
     $undecomposed_starter:ident,
     $pending_slice:ident,
     $len_utf:ident,
    ) => {
        $(#[$meta])*
        pub fn $normalize_to<W: $write + ?Sized>(
            &self,
            $text: $slice,
            $sink: &mut W,
        ) -> core::fmt::Result {
            $prolog
            let mut $composition = self.normalize_iter($text.chars());
            debug_assert_eq!($composition.decomposition.ignorable_behavior, IgnorableBehavior::Unsupported);
            for cc in $composition.decomposition.buffer.drain(..) {
                $sink.write_char(cc.character())?;
            }

            // Try to get the compiler to hoist the bound to a register.
            let $composition_passthrough_bound = $composition.composition_passthrough_bound;
            'outer: loop {
                debug_assert_eq!($composition.decomposition.buffer_pos, 0);
                let mut $undecomposed_starter =
                    if let Some(pending) = $composition.decomposition.pending.take() {
                        pending
                    } else {
                        return Ok(());
                    };
                // Allowing indexed slicing, because a failure would be a code bug and
                // not a data issue.
                #[allow(clippy::indexing_slicing)]
                if u32::from($undecomposed_starter.character) < $composition_passthrough_bound ||
                    $undecomposed_starter.potential_passthrough()
                {
                    // We don't know if a `REPLACEMENT_CHARACTER` occurred in the slice or
                    // was returned in response to an error by the iterator. Assume the
                    // latter for correctness even though it pessimizes the former.
                    if $always_valid_utf || $undecomposed_starter.character != REPLACEMENT_CHARACTER {
                        let $pending_slice = &$text[$text.len() - $composition.decomposition.delegate.$as_slice().len() - $undecomposed_starter.character.$len_utf()..];
                        // The `$fast` block must either:
                        // 1. Return due to reaching EOF
                        // 2. Leave a starter with its trie value in `$undecomposed_starter`
                        //    and, if there is still more input, leave the next character
                        //    and its trie value in `$composition.decomposition.pending`.
                        $fast
                    }
                }
                // Fast track above, full algorithm below
                let mut starter = $composition
                    .decomposition
                    .decomposing_next($undecomposed_starter);
                'bufferloop: loop {
                    // We first loop by index to avoid moving the contents of `buffer`, but
                    // if there's a discontiguous match, we'll start modifying `buffer` instead.
                    loop {
                        let (character, ccc) = if let Some((character, ccc)) = $composition
                            .decomposition
                            .buffer
                            .get($composition.decomposition.buffer_pos)
                            .map(|c| c.character_and_ccc())
                        {
                            (character, ccc)
                        } else {
                            $composition.decomposition.buffer.clear();
                            $composition.decomposition.buffer_pos = 0;
                            break;
                        };
                        if let Some(composed) = $composition.compose(starter, character) {
                            starter = composed;
                            $composition.decomposition.buffer_pos += 1;
                            continue;
                        }
                        let mut most_recent_skipped_ccc = ccc;
                        if most_recent_skipped_ccc == CCC_NOT_REORDERED {
                            // We failed to compose a starter. Discontiguous match not allowed.
                            // Write the current `starter` we've been composing, make the unmatched
                            // starter in the buffer the new `starter` (we know it's been decomposed)
                            // and process the rest of the buffer with that as the starter.
                            $sink.write_char(starter)?;
                            starter = character;
                            $composition.decomposition.buffer_pos += 1;
                            continue 'bufferloop;
                        } else {
                            {
                                let _ = $composition
                                    .decomposition
                                    .buffer
                                    .drain(0..$composition.decomposition.buffer_pos);
                            }
                            $composition.decomposition.buffer_pos = 0;
                        }
                        let mut i = 1; // We have skipped one non-starter.
                        while let Some((character, ccc)) = $composition
                            .decomposition
                            .buffer
                            .get(i)
                            .map(|c| c.character_and_ccc())
                        {
                            if ccc == CCC_NOT_REORDERED {
                                // Discontiguous match not allowed.
                                $sink.write_char(starter)?;
                                for cc in $composition.decomposition.buffer.drain(..i) {
                                    $sink.write_char(cc.character())?;
                                }
                                starter = character;
                                {
                                    let removed = $composition.decomposition.buffer.remove(0);
                                    debug_assert_eq!(starter, removed.character());
                                }
                                debug_assert_eq!($composition.decomposition.buffer_pos, 0);
                                continue 'bufferloop;
                            }
                            debug_assert!(ccc >= most_recent_skipped_ccc);
                            if ccc != most_recent_skipped_ccc {
                                // Using the non-Hangul version as a micro-optimization, since
                                // we already rejected the case where `second` is a starter
                                // above, and conjoining jamo are starters.
                                if let Some(composed) =
                                    $composition.compose_non_hangul(starter, character)
                                {
                                    $composition.decomposition.buffer.remove(i);
                                    starter = composed;
                                    continue;
                                }
                            }
                            most_recent_skipped_ccc = ccc;
                            i += 1;
                        }
                        break;
                    }
                    debug_assert_eq!($composition.decomposition.buffer_pos, 0);

                    if !$composition.decomposition.buffer.is_empty() {
                        $sink.write_char(starter)?;
                        for cc in $composition.decomposition.buffer.drain(..) {
                            $sink.write_char(cc.character())?;
                        }
                        // We had non-empty buffer, so can't compose with upcoming.
                        continue 'outer;
                    }
                    // Now we need to check if composition with an upcoming starter is possible.
                    if $composition.decomposition.pending.is_some() {
                        // We know that `pending_starter` decomposes to start with a starter.
                        // Otherwise, it would have been moved to `composition.decomposition.buffer`
                        // by `composition.decomposing_next()`. We do this set lookup here in order
                        // to get an opportunity to go back to the fast track.
                        // Note that this check has to happen _after_ checking that `pending`
                        // holds a character, because this flag isn't defined to be meaningful
                        // when `pending` isn't holding a character.
                        let pending = $composition.decomposition.pending.as_ref().unwrap();
                        if u32::from(pending.character) < $composition.composition_passthrough_bound
                            || !pending.can_combine_backwards()
                        {
                            // Won't combine backwards anyway.
                            $sink.write_char(starter)?;
                            continue 'outer;
                        }
                        let pending_starter = $composition.decomposition.pending.take().unwrap();
                        let decomposed = $composition.decomposition.decomposing_next(pending_starter);
                        if let Some(composed) = $composition.compose(starter, decomposed) {
                            starter = composed;
                        } else {
                            $sink.write_char(starter)?;
                            starter = decomposed;
                        }
                        continue 'bufferloop;
                    }
                    // End of input
                    $sink.write_char(starter)?;
                    return Ok(());
                } // 'bufferloop
            }
        }
    };
}

macro_rules! decomposing_normalize_to {
    ($(#[$meta:meta])*,
     $normalize_to:ident,
     $write:path,
     $slice:ty,
     $prolog:block,
     $as_slice:ident,
     $fast:block,
     $text:ident,
     $sink:ident,
     $decomposition:ident,
     $decomposition_passthrough_bound:ident,
     $undecomposed_starter:ident,
     $pending_slice:ident,
     $outer:lifetime, // loop labels use lifetime tokens
    ) => {
        $(#[$meta])*
        pub fn $normalize_to<W: $write + ?Sized>(
            &self,
            $text: $slice,
            $sink: &mut W,
        ) -> core::fmt::Result {
            $prolog

            let mut $decomposition = self.normalize_iter($text.chars());
            debug_assert_eq!($decomposition.ignorable_behavior, IgnorableBehavior::Unsupported);

            // Try to get the compiler to hoist the bound to a register.
            let $decomposition_passthrough_bound = $decomposition.decomposition_passthrough_bound;
            $outer: loop {
                for cc in $decomposition.buffer.drain(..) {
                    $sink.write_char(cc.character())?;
                }
                debug_assert_eq!($decomposition.buffer_pos, 0);
                let mut $undecomposed_starter = if let Some(pending) = $decomposition.pending.take() {
                    pending
                } else {
                    return Ok(());
                };
                // Allowing indexed slicing, because a failure would be a code bug and
                // not a data issue.
                #[allow(clippy::indexing_slicing)]
                if $undecomposed_starter.starter_and_decomposes_to_self() {
                    // Don't bother including `undecomposed_starter` in a contiguous buffer
                    // write: Just write it right away:
                    $sink.write_char($undecomposed_starter.character)?;

                    let $pending_slice = $decomposition.delegate.$as_slice();
                    $fast
                }
                let starter = $decomposition.decomposing_next($undecomposed_starter);
                $sink.write_char(starter)?;
            }
        }
    };
}

macro_rules! normalizer_methods {
    () => {
        /// Normalize a string slice into a `String`.
        pub fn normalize(&self, text: &str) -> String {
            let mut ret = String::new();
            ret.reserve(text.len());
            let _ = self.normalize_to(text, &mut ret);
            ret
        }

        /// Return the index a string slice is normalized up to.
        pub fn is_normalized_up_to(&self, text: &str) -> usize {
            let mut sink = IsNormalizedSinkStr::new(text);
            let _ = self.normalize_to(text, &mut sink);
            text.len() - sink.remaining_len()
        }

        /// Check whether a string slice is normalized.
        pub fn is_normalized(&self, text: &str) -> bool {
            let mut sink = IsNormalizedSinkStr::new(text);
            if self.normalize_to(text, &mut sink).is_err() {
                return false;
            }
            sink.finished()
        }

        /// Normalize a slice of potentially-invalid UTF-16 into a `Vec`.
        ///
        /// Unpaired surrogates are mapped to the REPLACEMENT CHARACTER
        /// before normalizing.
        pub fn normalize_utf16(&self, text: &[u16]) -> Vec<u16> {
            let mut ret = Vec::new();
            let _ = self.normalize_utf16_to(text, &mut ret);
            ret
        }

        /// Return the index a slice of potentially-invalid UTF-16 is normalized up to.
        pub fn is_normalized_utf16_up_to(&self, text: &[u16]) -> usize {
            let mut sink = IsNormalizedSinkUtf16::new(text);
            let _ = self.normalize_utf16_to(text, &mut sink);
            text.len() - sink.remaining_len()
        }

        /// Checks whether a slice of potentially-invalid UTF-16 is normalized.
        ///
        /// Unpaired surrogates are treated as the REPLACEMENT CHARACTER.
        pub fn is_normalized_utf16(&self, text: &[u16]) -> bool {
            let mut sink = IsNormalizedSinkUtf16::new(text);
            if self.normalize_utf16_to(text, &mut sink).is_err() {
                return false;
            }
            sink.finished()
        }

        /// Normalize a slice of potentially-invalid UTF-8 into a `String`.
        ///
        /// Ill-formed byte sequences are mapped to the REPLACEMENT CHARACTER
        /// according to the WHATWG Encoding Standard.
        pub fn normalize_utf8(&self, text: &[u8]) -> String {
            let mut ret = String::new();
            ret.reserve(text.len());
            let _ = self.normalize_utf8_to(text, &mut ret);
            ret
        }

        /// Return the index a slice of potentially-invalid UTF-8 is normalized up to
        pub fn is_normalized_utf8_up_to(&self, text: &[u8]) -> usize {
            let mut sink = IsNormalizedSinkUtf8::new(text);
            let _ = self.normalize_utf8_to(text, &mut sink);
            text.len() - sink.remaining_len()
        }

        /// Check if a slice of potentially-invalid UTF-8 is normalized.
        ///
        /// Ill-formed byte sequences are mapped to the REPLACEMENT CHARACTER
        /// according to the WHATWG Encoding Standard before checking.
        pub fn is_normalized_utf8(&self, text: &[u8]) -> bool {
            let mut sink = IsNormalizedSinkUtf8::new(text);
            if self.normalize_utf8_to(text, &mut sink).is_err() {
                return false;
            }
            sink.finished()
        }
    };
}

/// Borrowed version of a normalizer for performing decomposing normalization.
#[derive(Debug)]
pub struct DecomposingNormalizerBorrowed<'a> {
    decompositions: &'a DecompositionDataV1<'a>,
    supplementary_decompositions: Option<&'a DecompositionSupplementV1<'a>>,
    tables: &'a DecompositionTablesV1<'a>,
    supplementary_tables: Option<&'a DecompositionTablesV1<'a>>,
    decomposition_passthrough_bound: u8, // never above 0xC0
    composition_passthrough_bound: u16,  // never above 0x0300
}

impl DecomposingNormalizerBorrowed<'static> {
    /// Cheaply converts a [`DecomposingNormalizerBorrowed<'static>`] into a [`DecomposingNormalizer`].
    ///
    /// Note: Due to branching and indirection, using [`DecomposingNormalizer`] might inhibit some
    /// compile-time optimizations that are possible with [`DecomposingNormalizerBorrowed`].
    pub const fn static_to_owned(self) -> DecomposingNormalizer {
        DecomposingNormalizer {
            decompositions: DataPayload::from_static_ref(self.decompositions),
            supplementary_decompositions: if let Some(s) = self.supplementary_decompositions {
                // `map` not available in const context
                // TODO: Perhaps get rid of the holder enum, since we're just faking it here anyway.
                Some(SupplementPayloadHolder::Compatibility(
                    DataPayload::from_static_ref(s),
                ))
            } else {
                None
            },
            tables: DataPayload::from_static_ref(self.tables),
            supplementary_tables: if let Some(s) = self.supplementary_tables {
                // `map` not available in const context
                Some(DataPayload::from_static_ref(s))
            } else {
                None
            },
            decomposition_passthrough_bound: self.decomposition_passthrough_bound,
            composition_passthrough_bound: self.composition_passthrough_bound,
        }
    }

    /// NFD constructor using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new_nfd() -> Self {
        const _: () = assert!(
            crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_TABLES_V1_MARKER
                .scalars16
                .const_len()
                + crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_TABLES_V1_MARKER
                    .scalars24
                    .const_len()
                <= 0xFFF,
            "future extension"
        );

        DecomposingNormalizerBorrowed {
            decompositions:
                crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_DATA_V1_MARKER,
            supplementary_decompositions: None,
            tables: crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_TABLES_V1_MARKER,
            supplementary_tables: None,
            decomposition_passthrough_bound: 0xC0,
            composition_passthrough_bound: 0x0300,
        }
    }

    /// NFKD constructor using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new_nfkd() -> Self {
        const _: () = assert!(
            crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_TABLES_V1_MARKER
                .scalars16
                .const_len()
                + crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_TABLES_V1_MARKER
                    .scalars24
                    .const_len()
                + crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_TABLES_V1_MARKER
                    .scalars16
                    .const_len()
                + crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_TABLES_V1_MARKER
                    .scalars24
                    .const_len()
                <= 0xFFF,
            "future extension"
        );

        const _: () = assert!(
            crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_SUPPLEMENT_V1_MARKER
                .passthrough_cap
                <= 0x0300,
            "invalid"
        );

        let decomposition_capped =
            if crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_SUPPLEMENT_V1_MARKER
                .passthrough_cap
                < 0xC0
            {
                crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_SUPPLEMENT_V1_MARKER
                    .passthrough_cap
            } else {
                0xC0
            };
        let composition_capped =
            if crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_SUPPLEMENT_V1_MARKER
                .passthrough_cap
                < 0x0300
            {
                crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_SUPPLEMENT_V1_MARKER
                    .passthrough_cap
            } else {
                0x0300
            };

        DecomposingNormalizerBorrowed {
            decompositions:
                crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_DATA_V1_MARKER,
            supplementary_decompositions: Some(
                crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_SUPPLEMENT_V1_MARKER,
            ),
            tables: crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_TABLES_V1_MARKER,
            supplementary_tables: Some(
                crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_TABLES_V1_MARKER,
            ),
            decomposition_passthrough_bound: decomposition_capped as u8,
            composition_passthrough_bound: composition_capped,
        }
    }

    #[cfg(feature = "compiled_data")]
    pub(crate) const fn new_uts46_decomposed() -> Self {
        const _: () = assert!(
            crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_TABLES_V1_MARKER
                .scalars16
                .const_len()
                + crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_TABLES_V1_MARKER
                    .scalars24
                    .const_len()
                + crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_TABLES_V1_MARKER
                    .scalars16
                    .const_len()
                + crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_TABLES_V1_MARKER
                    .scalars24
                    .const_len()
                <= 0xFFF,
            "future extension"
        );

        const _: () = assert!(
            crate::provider::Baked::SINGLETON_UTS46_DECOMPOSITION_SUPPLEMENT_V1_MARKER
                .passthrough_cap
                <= 0x0300,
            "invalid"
        );

        let decomposition_capped =
            if crate::provider::Baked::SINGLETON_UTS46_DECOMPOSITION_SUPPLEMENT_V1_MARKER
                .passthrough_cap
                < 0xC0
            {
                crate::provider::Baked::SINGLETON_UTS46_DECOMPOSITION_SUPPLEMENT_V1_MARKER
                    .passthrough_cap
            } else {
                0xC0
            };
        let composition_capped =
            if crate::provider::Baked::SINGLETON_UTS46_DECOMPOSITION_SUPPLEMENT_V1_MARKER
                .passthrough_cap
                < 0x0300
            {
                crate::provider::Baked::SINGLETON_UTS46_DECOMPOSITION_SUPPLEMENT_V1_MARKER
                    .passthrough_cap
            } else {
                0x0300
            };

        DecomposingNormalizerBorrowed {
            decompositions:
                crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_DATA_V1_MARKER,
            supplementary_decompositions: Some(
                crate::provider::Baked::SINGLETON_UTS46_DECOMPOSITION_SUPPLEMENT_V1_MARKER,
            ),
            tables: crate::provider::Baked::SINGLETON_CANONICAL_DECOMPOSITION_TABLES_V1_MARKER,
            supplementary_tables: Some(
                crate::provider::Baked::SINGLETON_COMPATIBILITY_DECOMPOSITION_TABLES_V1_MARKER,
            ),
            decomposition_passthrough_bound: decomposition_capped as u8,
            composition_passthrough_bound: composition_capped,
        }
    }
}

impl DecomposingNormalizerBorrowed<'_> {
    /// Wraps a delegate iterator into a decomposing iterator
    /// adapter by using the data already held by this normalizer.
    pub fn normalize_iter<I: Iterator<Item = char>>(&self, iter: I) -> Decomposition<I> {
        Decomposition::new_with_supplements(
            iter,
            self.decompositions,
            self.supplementary_decompositions,
            self.tables,
            self.supplementary_tables,
            self.decomposition_passthrough_bound,
            IgnorableBehavior::Unsupported,
        )
    }

    normalizer_methods!();

    decomposing_normalize_to!(
        /// Normalize a string slice into a `Write` sink.
        ,
        normalize_to,
        core::fmt::Write,
        &str,
        {
        },
        as_str,
        {
            let decomposition_passthrough_byte_bound = if decomposition_passthrough_bound == 0xC0 {
                0xC3u8
            } else {
                decomposition_passthrough_bound.min(0x80) as u8
            };
            // The attribute belongs on an inner statement, but Rust doesn't allow it there.
            #[allow(clippy::unwrap_used)]
            'fast: loop {
                let mut code_unit_iter = decomposition.delegate.as_str().as_bytes().iter();
                'fastest: loop {
                    if let Some(&upcoming_byte) = code_unit_iter.next() {
                        if upcoming_byte < decomposition_passthrough_byte_bound {
                            // Fast-track succeeded!
                            continue 'fastest;
                        }
                        decomposition.delegate = pending_slice[pending_slice.len() - code_unit_iter.as_slice().len() - 1..].chars();
                        break 'fastest;
                    }
                    // End of stream
                    sink.write_str(pending_slice)?;
                    return Ok(());
                }

                // `unwrap()` OK, because the slice is valid UTF-8 and we know there
                // is an upcoming byte.
                let upcoming = decomposition.delegate.next().unwrap();
                let upcoming_with_trie_value = decomposition.attach_trie_value(upcoming);
                if upcoming_with_trie_value.starter_and_decomposes_to_self() {
                    continue 'fast;
                }
                let consumed_so_far_slice = &pending_slice[..pending_slice.len()
                    - decomposition.delegate.as_str().len()
                    - upcoming.len_utf8()];
                sink.write_str(consumed_so_far_slice)?;

                // Now let's figure out if we got a starter or a non-starter.
                if decomposition_starts_with_non_starter(
                    upcoming_with_trie_value.trie_val,
                ) {
                    // Let this trie value to be reprocessed in case it is
                    // one of the rare decomposing ones.
                    decomposition.pending = Some(upcoming_with_trie_value);
                    decomposition.gather_and_sort_combining(0);
                    continue 'outer;
                }
                undecomposed_starter = upcoming_with_trie_value;
                debug_assert!(decomposition.pending.is_none());
                break 'fast;
            }
        },
        text,
        sink,
        decomposition,
        decomposition_passthrough_bound,
        undecomposed_starter,
        pending_slice,
        'outer,
    );

    decomposing_normalize_to!(
        /// Normalize a slice of potentially-invalid UTF-8 into a `Write` sink.
        ///
        /// Ill-formed byte sequences are mapped to the REPLACEMENT CHARACTER
        /// according to the WHATWG Encoding Standard.
        ,
        normalize_utf8_to,
        core::fmt::Write,
        &[u8],
        {
        },
        as_slice,
        {
            let decomposition_passthrough_byte_bound = decomposition_passthrough_bound.min(0x80) as u8;
            // The attribute belongs on an inner statement, but Rust doesn't allow it there.
            #[allow(clippy::unwrap_used)]
            'fast: loop {
                let mut code_unit_iter = decomposition.delegate.as_slice().iter();
                'fastest: loop {
                    if let Some(&upcoming_byte) = code_unit_iter.next() {
                        if upcoming_byte < decomposition_passthrough_byte_bound {
                            // Fast-track succeeded!
                            continue 'fastest;
                        }
                        break 'fastest;
                    }
                    // End of stream
                    sink.write_str(unsafe { from_utf8_unchecked(pending_slice) })?;
                    return Ok(());
                }
                decomposition.delegate = pending_slice[pending_slice.len() - code_unit_iter.as_slice().len() - 1..].chars();

                // `unwrap()` OK, because the slice is valid UTF-8 and we know there
                // is an upcoming byte.
                let upcoming = decomposition.delegate.next().unwrap();
                let upcoming_with_trie_value = decomposition.attach_trie_value(upcoming);
                if upcoming_with_trie_value.starter_and_decomposes_to_self() {
                    if upcoming != REPLACEMENT_CHARACTER {
                        continue 'fast;
                    }
                    // We might have an error, so fall out of the fast path.

                    // Since the U+FFFD might signify an error, we can't
                    // assume `upcoming.len_utf8()` for the backoff length.
                    let mut consumed_so_far = pending_slice[..pending_slice.len() - decomposition.delegate.as_slice().len()].chars();
                    let back = consumed_so_far.next_back();
                    debug_assert_eq!(back, Some(REPLACEMENT_CHARACTER));
                    let consumed_so_far_slice = consumed_so_far.as_slice();
                    sink.write_str(unsafe{from_utf8_unchecked(consumed_so_far_slice)})?;

                    // We could call `gather_and_sort_combining` here and
                    // `continue 'outer`, but this should be better for code
                    // size.
                    undecomposed_starter = upcoming_with_trie_value;
                    debug_assert!(decomposition.pending.is_none());
                    break 'fast;
                }
                let consumed_so_far_slice = &pending_slice[..pending_slice.len()
                    - decomposition.delegate.as_slice().len()
                    - upcoming.len_utf8()];
                sink.write_str(unsafe{from_utf8_unchecked(consumed_so_far_slice)})?;

                // Now let's figure out if we got a starter or a non-starter.
                if decomposition_starts_with_non_starter(
                    upcoming_with_trie_value.trie_val,
                ) {
                    // Let this trie value to be reprocessed in case it is
                    // one of the rare decomposing ones.
                    decomposition.pending = Some(upcoming_with_trie_value);
                    decomposition.gather_and_sort_combining(0);
                    continue 'outer;
                }
                undecomposed_starter = upcoming_with_trie_value;
                debug_assert!(decomposition.pending.is_none());
                break 'fast;
            }
        },
        text,
        sink,
        decomposition,
        decomposition_passthrough_bound,
        undecomposed_starter,
        pending_slice,
        'outer,
    );

    decomposing_normalize_to!(
        /// Normalize a slice of potentially-invalid UTF-16 into a `Write16` sink.
        ///
        /// Unpaired surrogates are mapped to the REPLACEMENT CHARACTER
        /// before normalizing.
        ,
        normalize_utf16_to,
        write16::Write16,
        &[u16],
        {
            sink.size_hint(text.len())?;
        },
        as_slice,
        {
            let mut code_unit_iter = decomposition.delegate.as_slice().iter();
            // The purpose of the counter is to flush once in a while. If we flush
            // too much, there is too much flushing overhead. If we flush too rarely,
            // the flush starts reading from too far behind compared to the hot
            // recently-read memory.
            let mut counter = UTF16_FAST_PATH_FLUSH_THRESHOLD;
            'fast: loop {
                counter -= 1;
                if let Some(&upcoming_code_unit) = code_unit_iter.next() {
                    let mut upcoming32 = u32::from(upcoming_code_unit);
                    if upcoming32 < decomposition_passthrough_bound && counter != 0 {
                        continue 'fast;
                    }
                    // The loop is only broken out of as goto forward
                    #[allow(clippy::never_loop)]
                    'surrogateloop: loop {
                        let surrogate_base = upcoming32.wrapping_sub(0xD800);
                        if surrogate_base > (0xDFFF - 0xD800) {
                            // Not surrogate
                            break 'surrogateloop;
                        }
                        if surrogate_base <= (0xDBFF - 0xD800) {
                            let iter_backup = code_unit_iter.clone();
                            if let Some(&low) = code_unit_iter.next() {
                                if in_inclusive_range16(low, 0xDC00, 0xDFFF) {
                                    upcoming32 = (upcoming32 << 10) + u32::from(low)
                                        - (((0xD800u32 << 10) - 0x10000u32) + 0xDC00u32);
                                    break 'surrogateloop;
                                } else {
                                    code_unit_iter = iter_backup;
                                }
                            }
                        }
                        // unpaired surrogate
                        let slice_to_write = &pending_slice
                            [..pending_slice.len() - code_unit_iter.as_slice().len() - 1];
                        sink.write_slice(slice_to_write)?;
                        undecomposed_starter =
                            CharacterAndTrieValue::new(REPLACEMENT_CHARACTER, 0);
                        debug_assert!(decomposition.pending.is_none());
                        // We could instead call `gather_and_sort_combining` and `continue 'outer`, but
                        // assuming this is better for code size.
                        break 'fast;
                    }
                    // Not unpaired surrogate
                    let upcoming = unsafe { char::from_u32_unchecked(upcoming32) };
                    let upcoming_with_trie_value =
                        decomposition.attach_trie_value(upcoming);
                    if upcoming_with_trie_value.starter_and_decomposes_to_self() && counter != 0 {
                        continue 'fast;
                    }
                    let consumed_so_far_slice = &pending_slice[..pending_slice.len()
                        - code_unit_iter.as_slice().len()
                        - upcoming.len_utf16()];
                    sink.write_slice(consumed_so_far_slice)?;

                    // Now let's figure out if we got a starter or a non-starter.
                    if decomposition_starts_with_non_starter(
                        upcoming_with_trie_value.trie_val,
                    ) {
                        // Sync with main iterator
                        decomposition.delegate = code_unit_iter.as_slice().chars();
                        // Let this trie value to be reprocessed in case it is
                        // one of the rare decomposing ones.
                        decomposition.pending = Some(upcoming_with_trie_value);
                        decomposition.gather_and_sort_combining(0);
                        continue 'outer;
                    }
                    undecomposed_starter = upcoming_with_trie_value;
                    debug_assert!(decomposition.pending.is_none());
                    break 'fast;
                }
                // End of stream
                sink.write_slice(pending_slice)?;
                return Ok(());
            }
            // Sync the main iterator
            decomposition.delegate = code_unit_iter.as_slice().chars();
        },
        text,
        sink,
        decomposition,
        decomposition_passthrough_bound,
        undecomposed_starter,
        pending_slice,
        'outer,
    );
}

/// A normalizer for performing decomposing normalization.
#[derive(Debug)]
pub struct DecomposingNormalizer {
    decompositions: DataPayload<CanonicalDecompositionDataV1Marker>,
    supplementary_decompositions: Option<SupplementPayloadHolder>,
    tables: DataPayload<CanonicalDecompositionTablesV1Marker>,
    supplementary_tables: Option<DataPayload<CompatibilityDecompositionTablesV1Marker>>,
    decomposition_passthrough_bound: u8, // never above 0xC0
    composition_passthrough_bound: u16,  // never above 0x0300
}

impl DecomposingNormalizer {
    /// Constructs a borrowed version of this type for more efficient querying.
    pub fn as_borrowed(&self) -> DecomposingNormalizerBorrowed {
        DecomposingNormalizerBorrowed {
            decompositions: self.decompositions.get(),
            supplementary_decompositions: self
                .supplementary_decompositions
                .as_ref()
                .map(|s| s.get()),
            tables: self.tables.get(),
            supplementary_tables: self.supplementary_tables.as_ref().map(|s| s.get()),
            decomposition_passthrough_bound: self.decomposition_passthrough_bound,
            composition_passthrough_bound: self.composition_passthrough_bound,
        }
    }

    /// NFD constructor using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new_nfd() -> DecomposingNormalizerBorrowed<'static> {
        DecomposingNormalizerBorrowed::new_nfd()
    }

    icu_provider::gen_any_buffer_data_constructors!(
        () -> error: DataError,
        functions: [
            new_nfd: skip,
            try_new_nfd_with_any_provider,
            try_new_nfd_with_buffer_provider,
            try_new_nfd_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new_nfd)]
    pub fn try_new_nfd_unstable<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<CanonicalDecompositionDataV1Marker>
            + DataProvider<CanonicalDecompositionTablesV1Marker>
            + ?Sized,
    {
        let decompositions: DataPayload<CanonicalDecompositionDataV1Marker> =
            provider.load(Default::default())?.payload;
        let tables: DataPayload<CanonicalDecompositionTablesV1Marker> =
            provider.load(Default::default())?.payload;

        if tables.get().scalars16.len() + tables.get().scalars24.len() > 0xFFF {
            // The data is from a future where there exists a normalization flavor whose
            // complex decompositions take more than 0xFFF but fewer than 0x1FFF code points
            // of space. If a good use case from such a decomposition flavor arises, we can
            // dynamically change the bit masks so that the length mask becomes 0x1FFF instead
            // of 0xFFF and the all-non-starters mask becomes 0 instead of 0x1000. However,
            // since for now the masks are hard-coded, error out.
            return Err(DataError::custom("future extension")
                .with_marker(CanonicalDecompositionTablesV1Marker::INFO));
        }

        Ok(DecomposingNormalizer {
            decompositions,
            supplementary_decompositions: None,
            tables,
            supplementary_tables: None,
            decomposition_passthrough_bound: 0xC0,
            composition_passthrough_bound: 0x0300,
        })
    }

    icu_provider::gen_any_buffer_data_constructors!(
        () -> error: DataError,
        functions: [
            new_nfkd: skip,
            try_new_nfkd_with_any_provider,
            try_new_nfkd_with_buffer_provider,
            try_new_nfkd_unstable,
            Self,
        ]
    );

    /// NFKD constructor using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new_nfkd() -> DecomposingNormalizerBorrowed<'static> {
        DecomposingNormalizerBorrowed::new_nfkd()
    }

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new_nfkd)]
    pub fn try_new_nfkd_unstable<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<CanonicalDecompositionDataV1Marker>
            + DataProvider<CompatibilityDecompositionSupplementV1Marker>
            + DataProvider<CanonicalDecompositionTablesV1Marker>
            + DataProvider<CompatibilityDecompositionTablesV1Marker>
            + ?Sized,
    {
        let decompositions: DataPayload<CanonicalDecompositionDataV1Marker> =
            provider.load(Default::default())?.payload;
        let supplementary_decompositions: DataPayload<
            CompatibilityDecompositionSupplementV1Marker,
        > = provider.load(Default::default())?.payload;
        let tables: DataPayload<CanonicalDecompositionTablesV1Marker> =
            provider.load(Default::default())?.payload;
        let supplementary_tables: DataPayload<CompatibilityDecompositionTablesV1Marker> =
            provider.load(Default::default())?.payload;

        if tables.get().scalars16.len()
            + tables.get().scalars24.len()
            + supplementary_tables.get().scalars16.len()
            + supplementary_tables.get().scalars24.len()
            > 0xFFF
        {
            // The data is from a future where there exists a normalization flavor whose
            // complex decompositions take more than 0xFFF but fewer than 0x1FFF code points
            // of space. If a good use case from such a decomposition flavor arises, we can
            // dynamically change the bit masks so that the length mask becomes 0x1FFF instead
            // of 0xFFF and the all-non-starters mask becomes 0 instead of 0x1000. However,
            // since for now the masks are hard-coded, error out.
            return Err(DataError::custom("future extension")
                .with_marker(CanonicalDecompositionTablesV1Marker::INFO));
        }

        let cap = supplementary_decompositions.get().passthrough_cap;
        if cap > 0x0300 {
            return Err(DataError::custom("invalid")
                .with_marker(CompatibilityDecompositionSupplementV1Marker::INFO));
        }
        let decomposition_capped = cap.min(0xC0);
        let composition_capped = cap.min(0x0300);

        Ok(DecomposingNormalizer {
            decompositions,
            supplementary_decompositions: Some(SupplementPayloadHolder::Compatibility(
                supplementary_decompositions,
            )),
            tables,
            supplementary_tables: Some(supplementary_tables),
            decomposition_passthrough_bound: decomposition_capped as u8,
            composition_passthrough_bound: composition_capped,
        })
    }

    /// UTS 46 decomposed constructor (testing only)
    ///
    /// This is a special building block normalization for IDNA. It is the decomposed counterpart of
    /// ICU4C's UTS 46 normalization with two exceptions: characters that UTS 46 disallows and
    /// ICU4C maps to U+FFFD and characters that UTS 46 maps to the empty string normalize as in
    /// NFD in this normalization. In both cases, the previous UTS 46 processing before using
    /// normalization is expected to deal with these characters. Making the disallowed characters
    /// behave like this is beneficial to data size, and this normalizer implementation cannot
    /// deal with a character normalizing to the empty string, which doesn't happen in NFD or
    /// NFKD as of Unicode 14.
    ///
    /// Warning: In this normalization, U+0345 COMBINING GREEK YPOGEGRAMMENI exhibits a behavior
    /// that no character in Unicode exhibits in NFD, NFKD, NFC, or NFKC: Case folding turns
    /// U+0345 from a reordered character into a non-reordered character before reordering happens.
    /// Therefore, the output of this normalization may differ for different inputs that are
    /// canonically equivalent with each other if they differ by how U+0345 is ordered relative
    /// to other reorderable characters.
    pub(crate) fn try_new_uts46_decomposed_unstable<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<CanonicalDecompositionDataV1Marker>
            + DataProvider<Uts46DecompositionSupplementV1Marker>
            + DataProvider<CanonicalDecompositionTablesV1Marker>
            + DataProvider<CompatibilityDecompositionTablesV1Marker>
            // UTS 46 tables merged into CompatibilityDecompositionTablesV1Marker
            + ?Sized,
    {
        let decompositions: DataPayload<CanonicalDecompositionDataV1Marker> =
            provider.load(Default::default())?.payload;
        let supplementary_decompositions: DataPayload<Uts46DecompositionSupplementV1Marker> =
            provider.load(Default::default())?.payload;
        let tables: DataPayload<CanonicalDecompositionTablesV1Marker> =
            provider.load(Default::default())?.payload;
        let supplementary_tables: DataPayload<CompatibilityDecompositionTablesV1Marker> =
            provider.load(Default::default())?.payload;

        if tables.get().scalars16.len()
            + tables.get().scalars24.len()
            + supplementary_tables.get().scalars16.len()
            + supplementary_tables.get().scalars24.len()
            > 0xFFF
        {
            // The data is from a future where there exists a normalization flavor whose
            // complex decompositions take more than 0xFFF but fewer than 0x1FFF code points
            // of space. If a good use case from such a decomposition flavor arises, we can
            // dynamically change the bit masks so that the length mask becomes 0x1FFF instead
            // of 0xFFF and the all-non-starters mask becomes 0 instead of 0x1000. However,
            // since for now the masks are hard-coded, error out.
            return Err(DataError::custom("future extension")
                .with_marker(CanonicalDecompositionTablesV1Marker::INFO));
        }

        let cap = supplementary_decompositions.get().passthrough_cap;
        if cap > 0x0300 {
            return Err(DataError::custom("invalid")
                .with_marker(Uts46DecompositionSupplementV1Marker::INFO));
        }
        let decomposition_capped = cap.min(0xC0);
        let composition_capped = cap.min(0x0300);

        Ok(DecomposingNormalizer {
            decompositions,
            supplementary_decompositions: Some(SupplementPayloadHolder::Uts46(
                supplementary_decompositions,
            )),
            tables,
            supplementary_tables: Some(supplementary_tables),
            decomposition_passthrough_bound: decomposition_capped as u8,
            composition_passthrough_bound: composition_capped,
        })
    }
}

/// Borrowed version of a normalizer for performing composing normalization.
#[derive(Debug)]
pub struct ComposingNormalizerBorrowed<'a> {
    decomposing_normalizer: DecomposingNormalizerBorrowed<'a>,
    canonical_compositions: &'a CanonicalCompositionsV1<'a>,
}

impl ComposingNormalizerBorrowed<'static> {
    /// Cheaply converts a [`ComposingNormalizerBorrowed<'static>`] into a [`ComposingNormalizer`].
    ///
    /// Note: Due to branching and indirection, using [`ComposingNormalizer`] might inhibit some
    /// compile-time optimizations that are possible with [`ComposingNormalizerBorrowed`].
    pub const fn static_to_owned(self) -> ComposingNormalizer {
        ComposingNormalizer {
            decomposing_normalizer: self.decomposing_normalizer.static_to_owned(),
            canonical_compositions: DataPayload::from_static_ref(self.canonical_compositions),
        }
    }

    /// NFC constructor using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new_nfc() -> Self {
        ComposingNormalizerBorrowed {
            decomposing_normalizer: DecomposingNormalizerBorrowed::new_nfd(),
            canonical_compositions:
                crate::provider::Baked::SINGLETON_CANONICAL_COMPOSITIONS_V1_MARKER,
        }
    }

    /// NFKC constructor using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new_nfkc() -> Self {
        ComposingNormalizerBorrowed {
            decomposing_normalizer: DecomposingNormalizerBorrowed::new_nfkd(),
            canonical_compositions:
                crate::provider::Baked::SINGLETON_CANONICAL_COMPOSITIONS_V1_MARKER,
        }
    }

    /// This is a special building block normalization for IDNA that implements parts of the Map
    /// step and the following Normalize step.
    ///
    /// Warning: In this normalization, U+0345 COMBINING GREEK YPOGEGRAMMENI exhibits a behavior
    /// that no character in Unicode exhibits in NFD, NFKD, NFC, or NFKC: Case folding turns
    /// U+0345 from a reordered character into a non-reordered character before reordering happens.
    /// Therefore, the output of this normalization may differ for different inputs that are
    /// canonically equivalents with each other if they differ by how U+0345 is ordered relative
    /// to other reorderable characters.
    #[cfg(feature = "compiled_data")]
    pub(crate) const fn new_uts46() -> Self {
        ComposingNormalizerBorrowed {
            decomposing_normalizer: DecomposingNormalizerBorrowed::new_uts46_decomposed(),
            canonical_compositions:
                crate::provider::Baked::SINGLETON_CANONICAL_COMPOSITIONS_V1_MARKER,
        }
    }
}

impl ComposingNormalizerBorrowed<'_> {
    /// Wraps a delegate iterator into a composing iterator
    /// adapter by using the data already held by this normalizer.
    pub fn normalize_iter<I: Iterator<Item = char>>(&self, iter: I) -> Composition<I> {
        self.normalize_iter_private(iter, IgnorableBehavior::Unsupported)
    }

    fn normalize_iter_private<I: Iterator<Item = char>>(
        &self,
        iter: I,
        ignorable_behavior: IgnorableBehavior,
    ) -> Composition<I> {
        Composition::new(
            Decomposition::new_with_supplements(
                iter,
                self.decomposing_normalizer.decompositions,
                self.decomposing_normalizer.supplementary_decompositions,
                self.decomposing_normalizer.tables,
                self.decomposing_normalizer.supplementary_tables,
                self.decomposing_normalizer.decomposition_passthrough_bound,
                ignorable_behavior,
            ),
            self.canonical_compositions.canonical_compositions.clone(),
            self.decomposing_normalizer.composition_passthrough_bound,
        )
    }

    normalizer_methods!();

    composing_normalize_to!(
        /// Normalize a string slice into a `Write` sink.
        ,
        normalize_to,
        core::fmt::Write,
        &str,
        {},
        true,
        as_str,
        {
            // Let's hope LICM hoists this outside `'outer`.
            let composition_passthrough_byte_bound = if composition_passthrough_bound == 0x300 {
                0xCCu8
            } else {
                // We can make this fancy if a normalization other than NFC where looking at
                // non-ASCII lead bytes is worthwhile is ever introduced.
                composition_passthrough_bound.min(0x80) as u8
            };
            // This is basically an `Option` discriminant for `undecomposed_starter`,
            // but making it a boolean so that writes in the tightest loop are as
            // simple as possible (and potentially as peel-hoistable as possible).
            // Furthermore, this reduces `unwrap()` later.
            let mut undecomposed_starter_valid = true;
            // Annotation belongs really on inner statements, but Rust doesn't
            // allow it there.
            #[allow(clippy::unwrap_used)]
            'fast: loop {
                let mut code_unit_iter = composition.decomposition.delegate.as_str().as_bytes().iter();
                'fastest: loop {
                    if let Some(&upcoming_byte) = code_unit_iter.next() {
                        if upcoming_byte < composition_passthrough_byte_bound {
                            // Fast-track succeeded!
                            undecomposed_starter_valid = false;
                            continue 'fastest;
                        }
                        composition.decomposition.delegate = pending_slice[pending_slice.len() - code_unit_iter.as_slice().len() - 1..].chars();
                        break 'fastest;
                    }
                    // End of stream
                    sink.write_str(pending_slice)?;
                    return Ok(());
                }
                // `unwrap()` OK, because the slice is valid UTF-8 and we know there
                // is an upcoming byte.
                let upcoming = composition.decomposition.delegate.next().unwrap();
                let upcoming_with_trie_value = composition.decomposition.attach_trie_value(upcoming);
                if upcoming_with_trie_value.potential_passthrough_and_cannot_combine_backwards() {
                    // Can't combine backwards, hence a plain (non-backwards-combining)
                    // starter albeit past `composition_passthrough_bound`

                    // Fast-track succeeded!
                    undecomposed_starter = upcoming_with_trie_value;
                    undecomposed_starter_valid = true;
                    continue 'fast;
                }
                // We need to fall off the fast path.
                composition.decomposition.pending = Some(upcoming_with_trie_value);
                let consumed_so_far_slice = if undecomposed_starter_valid {
                    &pending_slice[..pending_slice.len() - composition.decomposition.delegate.as_str().len() - upcoming.len_utf8() - undecomposed_starter.character.len_utf8()]
                } else {
                    // slicing and unwrap OK, because we've just evidently read enough previously.
                    let mut consumed_so_far = pending_slice[..pending_slice.len() - composition.decomposition.delegate.as_str().len() - upcoming.len_utf8()].chars();
                    // `unwrap` OK, because we've previously manage to read the previous character
                    undecomposed_starter = composition.decomposition.attach_trie_value(consumed_so_far.next_back().unwrap());
                    undecomposed_starter_valid = true;
                    consumed_so_far.as_str()
                };
                sink.write_str(consumed_so_far_slice)?;
                break 'fast;
            }
            debug_assert!(undecomposed_starter_valid);
        },
        text,
        sink,
        composition,
        composition_passthrough_bound,
        undecomposed_starter,
        pending_slice,
        len_utf8,
    );

    composing_normalize_to!(
        /// Normalize a slice of potentially-invalid UTF-8 into a `Write` sink.
        ///
        /// Ill-formed byte sequences are mapped to the REPLACEMENT CHARACTER
        /// according to the WHATWG Encoding Standard.
        ,
        normalize_utf8_to,
        core::fmt::Write,
        &[u8],
        {},
        false,
        as_slice,
        {
            // This is basically an `Option` discriminant for `undecomposed_starter`,
            // but making it a boolean so that writes in the tightest loop are as
            // simple as possible (and potentially as peel-hoistable as possible).
            // Furthermore, this reduces `unwrap()` later.
            let mut undecomposed_starter_valid = true;
            'fast: loop {
                if let Some(upcoming) = composition.decomposition.delegate.next() {
                    if u32::from(upcoming) < composition_passthrough_bound {
                        // Fast-track succeeded!
                        undecomposed_starter_valid = false;
                        continue 'fast;
                    }
                    // TODO(#2006): Annotate as unlikely
                    if upcoming == REPLACEMENT_CHARACTER {
                        // Can't tell if this is an error or a literal U+FFFD in
                        // the input. Assuming the former to be sure.

                        // Since the U+FFFD might signify an error, we can't
                        // assume `upcoming.len_utf8()` for the backoff length.
                        let mut consumed_so_far = pending_slice[..pending_slice.len() - composition.decomposition.delegate.as_slice().len()].chars();
                        let back = consumed_so_far.next_back();
                        debug_assert_eq!(back, Some(REPLACEMENT_CHARACTER));
                        let consumed_so_far_slice = consumed_so_far.as_slice();
                        sink.write_str(unsafe{ from_utf8_unchecked(consumed_so_far_slice)})?;
                        undecomposed_starter = CharacterAndTrieValue::new(REPLACEMENT_CHARACTER, 0);
                        undecomposed_starter_valid = true;
                        composition.decomposition.pending = None;
                        break 'fast;
                    }
                    let upcoming_with_trie_value = composition.decomposition.attach_trie_value(upcoming);
                    if upcoming_with_trie_value.potential_passthrough_and_cannot_combine_backwards() {
                        // Can't combine backwards, hence a plain (non-backwards-combining)
                        // starter albeit past `composition_passthrough_bound`

                        // Fast-track succeeded!
                        undecomposed_starter = upcoming_with_trie_value;
                        undecomposed_starter_valid = true;
                        continue 'fast;
                    }
                    // We need to fall off the fast path.
                    composition.decomposition.pending = Some(upcoming_with_trie_value);
                    // Annotation belongs really on inner statement, but Rust doesn't
                    // allow it there.
                    #[allow(clippy::unwrap_used)]
                    let consumed_so_far_slice = if undecomposed_starter_valid {
                        &pending_slice[..pending_slice.len() - composition.decomposition.delegate.as_slice().len() - upcoming.len_utf8() - undecomposed_starter.character.len_utf8()]
                    } else {
                        // slicing and unwrap OK, because we've just evidently read enough previously.
                        let mut consumed_so_far = pending_slice[..pending_slice.len() - composition.decomposition.delegate.as_slice().len() - upcoming.len_utf8()].chars();
                        // `unwrap` OK, because we've previously manage to read the previous character
                        undecomposed_starter = composition.decomposition.attach_trie_value(consumed_so_far.next_back().unwrap());
                        undecomposed_starter_valid = true;
                        consumed_so_far.as_slice()
                    };
                    sink.write_str(unsafe { from_utf8_unchecked(consumed_so_far_slice)})?;
                    break 'fast;
                }
                // End of stream
                sink.write_str(unsafe {from_utf8_unchecked(pending_slice) })?;
                return Ok(());
            }
            debug_assert!(undecomposed_starter_valid);
        },
        text,
        sink,
        composition,
        composition_passthrough_bound,
        undecomposed_starter,
        pending_slice,
        len_utf8,
    );

    composing_normalize_to!(
        /// Normalize a slice of potentially-invalid UTF-16 into a `Write16` sink.
        ///
        /// Unpaired surrogates are mapped to the REPLACEMENT CHARACTER
        /// before normalizing.
        ,
        normalize_utf16_to,
        write16::Write16,
        &[u16],
        {
            sink.size_hint(text.len())?;
        },
        false,
        as_slice,
        {
            let mut code_unit_iter = composition.decomposition.delegate.as_slice().iter();
            let mut upcoming32;
            // This is basically an `Option` discriminant for `undecomposed_starter`,
            // but making it a boolean so that writes to it are  are as
            // simple as possible.
            // Furthermore, this removes the need for `unwrap()` later.
            let mut undecomposed_starter_valid;
            // The purpose of the counter is to flush once in a while. If we flush
            // too much, there is too much flushing overhead. If we flush too rarely,
            // the flush starts reading from too far behind compared to the hot
            // recently-read memory.
            let mut counter = UTF16_FAST_PATH_FLUSH_THRESHOLD;
            // The purpose of this trickiness is to avoid writing to
            // `undecomposed_starter_valid` from the tightest loop. Writing to it
            // from there destroys performance.
            let mut counter_reference = counter - 1;
            'fast: loop {
                counter -= 1;
                if let Some(&upcoming_code_unit) = code_unit_iter.next() {
                    upcoming32 = u32::from(upcoming_code_unit); // may be surrogate
                    if upcoming32 < composition_passthrough_bound && counter != 0 {
                        // No need for surrogate or U+FFFD check, because
                        // `composition_passthrough_bound` cannot be higher than
                        // U+0300.
                        // Fast-track succeeded!
                        continue 'fast;
                    }
                    // if `counter` equals `counter_reference`, the `continue 'fast`
                    // line above has not executed and `undecomposed_starter` is still
                    // valid.
                    undecomposed_starter_valid = counter == counter_reference;
                    // The loop is only broken out of as goto forward
                    #[allow(clippy::never_loop)]
                    'surrogateloop: loop {
                        let surrogate_base = upcoming32.wrapping_sub(0xD800);
                        if surrogate_base > (0xDFFF - 0xD800) {
                            // Not surrogate
                            break 'surrogateloop;
                        }
                        if surrogate_base <= (0xDBFF - 0xD800) {
                            let iter_backup = code_unit_iter.clone();
                            if let Some(&low) = code_unit_iter.next() {
                                if in_inclusive_range16(low, 0xDC00, 0xDFFF) {
                                    upcoming32 = (upcoming32 << 10) + u32::from(low)
                                        - (((0xD800u32 << 10) - 0x10000u32) + 0xDC00u32);
                                    break 'surrogateloop;
                                } else {
                                    code_unit_iter = iter_backup;
                                }
                            }
                        }
                        // unpaired surrogate
                        let slice_to_write = &pending_slice[..pending_slice.len() - code_unit_iter.as_slice().len() - 1];
                        sink.write_slice(slice_to_write)?;
                        undecomposed_starter = CharacterAndTrieValue::new(REPLACEMENT_CHARACTER, 0);
                        undecomposed_starter_valid = true;
                        composition.decomposition.pending = None;
                        break 'fast;
                    }
                    // Not unpaired surrogate
                    let upcoming = unsafe { char::from_u32_unchecked(upcoming32) };
                    let upcoming_with_trie_value = composition.decomposition.attach_trie_value(upcoming);
                    if upcoming_with_trie_value.potential_passthrough_and_cannot_combine_backwards() && counter != 0 {
                        // Can't combine backwards, hence a plain (non-backwards-combining)
                        // starter albeit past `composition_passthrough_bound`

                        // Fast-track succeeded!
                        undecomposed_starter = upcoming_with_trie_value;
                        // Cause `undecomposed_starter_valid` to be set to true.
                        // This regresses English performance on Haswell by 11%
                        // compared to commenting out this assignment to
                        // `counter_reference`.
                        counter_reference = counter - 1;
                        continue 'fast;
                    }
                    // We need to fall off the fast path.
                    composition.decomposition.pending = Some(upcoming_with_trie_value);
                    // Annotation belongs really on inner statement, but Rust doesn't
                    // allow it there.
                    #[allow(clippy::unwrap_used)]
                    let consumed_so_far_slice = if undecomposed_starter_valid {
                        &pending_slice[..pending_slice.len() - code_unit_iter.as_slice().len() - upcoming.len_utf16() - undecomposed_starter.character.len_utf16()]
                    } else {
                        // slicing and unwrap OK, because we've just evidently read enough previously.
                        let mut consumed_so_far = pending_slice[..pending_slice.len() - code_unit_iter.as_slice().len() - upcoming.len_utf16()].chars();
                        // `unwrap` OK, because we've previously manage to read the previous character
                        undecomposed_starter = composition.decomposition.attach_trie_value(consumed_so_far.next_back().unwrap());
                        undecomposed_starter_valid = true;
                        consumed_so_far.as_slice()
                    };
                    sink.write_slice(consumed_so_far_slice)?;
                    break 'fast;
                }
                // End of stream
                sink.write_slice(pending_slice)?;
                return Ok(());
            }
            debug_assert!(undecomposed_starter_valid);
            // Sync the main iterator
            composition.decomposition.delegate = code_unit_iter.as_slice().chars();
        },
        text,
        sink,
        composition,
        composition_passthrough_bound,
        undecomposed_starter,
        pending_slice,
        len_utf16,
    );
}

/// A normalizer for performing composing normalization.
#[derive(Debug)]
pub struct ComposingNormalizer {
    decomposing_normalizer: DecomposingNormalizer,
    canonical_compositions: DataPayload<CanonicalCompositionsV1Marker>,
}

impl ComposingNormalizer {
    /// Constructs a borrowed version of this type for more efficient querying.
    pub fn as_borrowed(&self) -> ComposingNormalizerBorrowed<'_> {
        ComposingNormalizerBorrowed {
            decomposing_normalizer: self.decomposing_normalizer.as_borrowed(),
            canonical_compositions: self.canonical_compositions.get(),
        }
    }

    /// NFC constructor using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new_nfc() -> ComposingNormalizerBorrowed<'static> {
        ComposingNormalizerBorrowed::new_nfc()
    }

    icu_provider::gen_any_buffer_data_constructors!(
        () -> error: DataError,
        functions: [
            new_nfc: skip,
            try_new_nfc_with_any_provider,
            try_new_nfc_with_buffer_provider,
            try_new_nfc_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new_nfc)]
    pub fn try_new_nfc_unstable<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<CanonicalDecompositionDataV1Marker>
            + DataProvider<CanonicalDecompositionTablesV1Marker>
            + DataProvider<CanonicalCompositionsV1Marker>
            + ?Sized,
    {
        let decomposing_normalizer = DecomposingNormalizer::try_new_nfd_unstable(provider)?;

        let canonical_compositions: DataPayload<CanonicalCompositionsV1Marker> =
            provider.load(Default::default())?.payload;

        Ok(ComposingNormalizer {
            decomposing_normalizer,
            canonical_compositions,
        })
    }

    /// NFKC constructor using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new_nfkc() -> ComposingNormalizerBorrowed<'static> {
        ComposingNormalizerBorrowed::new_nfkc()
    }

    icu_provider::gen_any_buffer_data_constructors!(
        () -> error: DataError,
        functions: [
            new_nfkc: skip,
            try_new_nfkc_with_any_provider,
            try_new_nfkc_with_buffer_provider,
            try_new_nfkc_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new_nfkc)]
    pub fn try_new_nfkc_unstable<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<CanonicalDecompositionDataV1Marker>
            + DataProvider<CompatibilityDecompositionSupplementV1Marker>
            + DataProvider<CanonicalDecompositionTablesV1Marker>
            + DataProvider<CompatibilityDecompositionTablesV1Marker>
            + DataProvider<CanonicalCompositionsV1Marker>
            + ?Sized,
    {
        let decomposing_normalizer = DecomposingNormalizer::try_new_nfkd_unstable(provider)?;

        let canonical_compositions: DataPayload<CanonicalCompositionsV1Marker> =
            provider.load(Default::default())?.payload;

        Ok(ComposingNormalizer {
            decomposing_normalizer,
            canonical_compositions,
        })
    }

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new_uts46)]
    pub(crate) fn try_new_uts46_unstable<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<CanonicalDecompositionDataV1Marker>
            + DataProvider<Uts46DecompositionSupplementV1Marker>
            + DataProvider<CanonicalDecompositionTablesV1Marker>
            + DataProvider<CompatibilityDecompositionTablesV1Marker>
            // UTS 46 tables merged into CompatibilityDecompositionTablesV1Marker
            + DataProvider<CanonicalCompositionsV1Marker>
            + ?Sized,
    {
        let decomposing_normalizer =
            DecomposingNormalizer::try_new_uts46_decomposed_unstable(provider)?;

        let canonical_compositions: DataPayload<CanonicalCompositionsV1Marker> =
            provider.load(Default::default())?.payload;

        Ok(ComposingNormalizer {
            decomposing_normalizer,
            canonical_compositions,
        })
    }
}

struct IsNormalizedSinkUtf16<'a> {
    expect: &'a [u16],
}

impl<'a> IsNormalizedSinkUtf16<'a> {
    pub fn new(slice: &'a [u16]) -> Self {
        IsNormalizedSinkUtf16 { expect: slice }
    }
    pub fn finished(&self) -> bool {
        self.expect.is_empty()
    }
    pub fn remaining_len(&self) -> usize {
        self.expect.len()
    }
}

impl Write16 for IsNormalizedSinkUtf16<'_> {
    fn write_slice(&mut self, s: &[u16]) -> core::fmt::Result {
        // We know that if we get a slice, it's a pass-through,
        // so we can compare addresses. Indexing is OK, because
        // an indexing failure would be a code bug rather than
        // an input or data issue.
        #[allow(clippy::indexing_slicing)]
        if s.as_ptr() == self.expect.as_ptr() {
            self.expect = &self.expect[s.len()..];
            Ok(())
        } else {
            Err(core::fmt::Error {})
        }
    }

    fn write_char(&mut self, c: char) -> core::fmt::Result {
        let mut iter = self.expect.chars();
        if iter.next() == Some(c) {
            self.expect = iter.as_slice();
            Ok(())
        } else {
            Err(core::fmt::Error {})
        }
    }
}

struct IsNormalizedSinkUtf8<'a> {
    expect: &'a [u8],
}

impl<'a> IsNormalizedSinkUtf8<'a> {
    pub fn new(slice: &'a [u8]) -> Self {
        IsNormalizedSinkUtf8 { expect: slice }
    }
    pub fn finished(&self) -> bool {
        self.expect.is_empty()
    }
    pub fn remaining_len(&self) -> usize {
        self.expect.len()
    }
}

impl core::fmt::Write for IsNormalizedSinkUtf8<'_> {
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        // We know that if we get a slice, it's a pass-through,
        // so we can compare addresses. Indexing is OK, because
        // an indexing failure would be a code bug rather than
        // an input or data issue.
        #[allow(clippy::indexing_slicing)]
        if s.as_ptr() == self.expect.as_ptr() {
            self.expect = &self.expect[s.len()..];
            Ok(())
        } else {
            Err(core::fmt::Error {})
        }
    }

    fn write_char(&mut self, c: char) -> core::fmt::Result {
        let mut iter = self.expect.chars();
        if iter.next() == Some(c) {
            self.expect = iter.as_slice();
            Ok(())
        } else {
            Err(core::fmt::Error {})
        }
    }
}

struct IsNormalizedSinkStr<'a> {
    expect: &'a str,
}

impl<'a> IsNormalizedSinkStr<'a> {
    pub fn new(slice: &'a str) -> Self {
        IsNormalizedSinkStr { expect: slice }
    }
    pub fn finished(&self) -> bool {
        self.expect.is_empty()
    }
    pub fn remaining_len(&self) -> usize {
        self.expect.len()
    }
}

impl core::fmt::Write for IsNormalizedSinkStr<'_> {
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        // We know that if we get a slice, it's a pass-through,
        // so we can compare addresses. Indexing is OK, because
        // an indexing failure would be a code bug rather than
        // an input or data issue.
        #[allow(clippy::indexing_slicing)]
        if s.as_ptr() == self.expect.as_ptr() {
            self.expect = &self.expect[s.len()..];
            Ok(())
        } else {
            Err(core::fmt::Error {})
        }
    }

    fn write_char(&mut self, c: char) -> core::fmt::Result {
        let mut iter = self.expect.chars();
        if iter.next() == Some(c) {
            self.expect = iter.as_str();
            Ok(())
        } else {
            Err(core::fmt::Error {})
        }
    }
}
