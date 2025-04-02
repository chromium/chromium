// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable.
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]
// Suppress a warning on zerovec::makevarule.
#![allow(missing_docs)]

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use alloc::borrow::Cow;
use icu_plurals::PluralCategory;
use icu_provider::prelude::*;
use zerovec::ZeroMap2d;

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
    /// `LongCompactDecimalFormatDataV1`
    LongCompactDecimalFormatDataV1,
    CompactDecimalPatternData<'static>,
);
icu_provider::data_marker!(
    /// `ShortCompactDecimalFormatDataV1`
    ShortCompactDecimalFormatDataV1,
    CompactDecimalPatternData<'static>,
);

/// Compact Decimal Pattern  data struct.
///
/// As in CLDR, this is a mapping from type (a power of ten, corresponding to
/// the magnitude of the number being formatted) and count (a plural case or an
/// explicit 1) to a pattern.
///
/// However, plural cases that are identical to the other case are omitted, thus
/// given
/// > (1000, one) ↦ 0K, (1000, other) ↦ 0K
///
/// only
/// > (1000, other) ↦ 0K
///
/// is stored.
///
/// Further, if all plural cases are compatible across consecutive types, the
/// larger types are omitted, thus given
/// > (1000, other) ↦ 0K, (10000, other) ↦ 00K, (100000, other) ↦ 000K
///
/// only
/// > (1000, other) ↦ 0K
///
/// is stored.
///
/// Finally, the pattern indicating noncompact notation for the first few powers
/// of ten is omitted; that is, there is an implicit (1, other) ↦ 0.
#[derive(Debug, Clone, Default, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::compactdecimal::provider))]
#[yoke(prove_covariance_manually)]
pub struct CompactDecimalPatternData<'data> {
    /// A map keyed on log10 of the CLDR `type` attribute and the CLDR `count` attribute.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub patterns: ZeroMap2d<'data, i8, Count, PatternULE>,
}

icu_provider::data_struct!(CompactDecimalPatternData<'_>, #[cfg(feature = "datagen")]);

/// A CLDR plural keyword, or the explicit value 1.
/// See <https://www.unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules>.
#[zerovec::make_ule(CountULE)]
#[zerovec::derive(Debug)]
#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::compactdecimal::provider))]
#[repr(u8)]
pub enum Count {
    /// The CLDR keyword `zero`.
    Zero = 0,
    /// The CLDR keyword `one`.
    One = 1,
    /// The CLDR keyword `two`.
    Two = 2,
    /// The CLDR keyword `few`.
    Few = 3,
    /// The CLDR keyword `many`.
    Many = 4,
    /// The CLDR keyword `other`.
    Other = 5,
    /// The explicit 1 case, see <https://www.unicode.org/reports/tr35/tr35-numbers.html#Explicit_0_1_rules>.
    Explicit1 = 6,
    // NOTE(egg): No explicit 0, because the compact decimal pattern selection
    // algorithm does not allow such a thing to arise.
}

impl From<PluralCategory> for Count {
    fn from(other: PluralCategory) -> Self {
        use PluralCategory::*;
        match other {
            Zero => Count::Zero,
            One => Count::One,
            Two => Count::Two,
            Few => Count::Few,
            Many => Count::Many,
            Other => Count::Other,
        }
    }
}

/// A compact decimal pattern, representing some literal text with an optional
/// placeholder, and the power of 10 expressed by the text.
#[derive(
    Debug, Clone, Default, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom, Ord, PartialOrd, Eq,
)]
#[zerovec::make_varule(PatternULE)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::compactdecimal::provider))]
#[cfg_attr(feature = "datagen", zerovec::derive(Serialize))]
#[zerovec::derive(Debug)]
#[cfg_attr(feature = "serde", zerovec::derive(Deserialize))]
pub struct Pattern<'data> {
    /// The compact decimal exponent, e.g., 6 for "million".
    /// The value 0 indicates that compact notation is not used; in that case,
    /// literal text must be empty; this corresponds to the CLDR pattern "0".
    /// This is derived from the numbers of 0s in the pattern and the associated
    /// `type` attribute; it is a more convenient representation than the number
    /// of 0s, because it is often common to multiple types; for instance, the
    /// following correspond to the same [`Pattern`]:
    ///   <pattern type="1000000" count="other">0 M</pattern>
    ///   <pattern type="10000000" count="other">00 M</pattern>
    pub exponent: i8,
    /// The index in literal_text before which the placeholder is inserted;
    /// this is 0 for insertion at the beginning, which is most common.
    /// The value 255 indicates that the pattern does not have a placeholder,
    /// as in French "mille" for 1000.
    pub index: u8,
    #[cfg_attr(feature = "serde", serde(borrow))]
    /// The underlying CLDR pattern with the placeholder removed, e.g.,
    /// " M" for the pattern "000 M"
    pub literal_text: Cow<'data, str>,
}
