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
#![allow(clippy::exhaustive_structs)]
#![allow(clippy::exhaustive_enums)]

use alloc::borrow::Cow;
use icu_provider::prelude::*;
use zerovec::VarZeroCow;

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
    use icu_decimal_data::*;
    pub mod icu {
        pub use crate as decimal;
        pub use icu_decimal_data::icu_locale as locale;
    }
    make_provider!(Baked);
    impl_decimal_symbols_v2_marker!(Baked);
    impl_decimal_digits_v1_marker!(Baked);
};

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] = &[DecimalSymbolsV2Marker::INFO, DecimalDigitsV1Marker::INFO];

/// A collection of settings expressing where to put grouping separators in a decimal number.
/// For example, `1,000,000` has two grouping separators, positioned along every 3 digits.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, Copy, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_decimal::provider))]
pub struct GroupingSizesV1 {
    /// The size of the first (lowest-magnitude) group.
    ///
    /// If 0, grouping separators will never be shown.
    pub primary: u8,

    /// The size of groups after the first group.
    ///
    /// If 0, defaults to be the same as `primary`.
    pub secondary: u8,

    /// The minimum number of digits required before the first group. For example, if `primary=3`
    /// and `min_grouping=2`, grouping separators will be present on 10,000 and above.
    pub min_grouping: u8,
}

/// A stack representation of the strings used in [`DecimalSymbolsV2`], i.e. a builder type
/// for [`DecimalSymbolsStrs`]. This type can be obtained from a [`DecimalSymbolsStrs`]
/// the `From`/`Into` traits.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
#[zerovec::make_varule(DecimalSymbolsStrs)]
#[zerovec::derive(Debug)]
#[zerovec::skip_derive(Ord)]
#[cfg_attr(feature = "serde", zerovec::derive(Deserialize))]
#[cfg_attr(feature = "datagen", zerovec::derive(Serialize))]
// Each affix/separator is at most three characters, which tends to be around 3-12 bytes each
// and the numbering system is at most 8 ascii bytes, All put together the indexing is extremely
// unlikely to have to go past 256.
#[zerovec::format(zerovec::vecs::Index8)]
pub struct DecimalSymbolStrsBuilder<'data> {
    /// Prefix to apply when a negative sign is needed.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub minus_sign_prefix: Cow<'data, str>,
    /// Suffix to apply when a negative sign is needed.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub minus_sign_suffix: Cow<'data, str>,

    /// Prefix to apply when a positive sign is needed.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub plus_sign_prefix: Cow<'data, str>,
    /// Suffix to apply when a positive sign is needed.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub plus_sign_suffix: Cow<'data, str>,

    /// Character used to separate the integer and fraction parts of the number.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub decimal_separator: Cow<'data, str>,

    /// Character used to separate groups in the integer part of the number.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub grouping_separator: Cow<'data, str>,

    /// The numbering system to use.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub numsys: Cow<'data, str>,
}

impl<'data> DecimalSymbolStrsBuilder<'data> {
    /// Build a [`DecimalSymbolsStrs`]
    pub fn build(&self) -> VarZeroCow<'static, DecimalSymbolsStrs> {
        VarZeroCow::from_encodeable(self)
    }
}

/// Symbols and metadata required for formatting a [`FixedDecimal`](crate::FixedDecimal).
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(DecimalSymbolsV2Marker = "decimal/symbols@2")]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_decimal::provider))]
pub struct DecimalSymbolsV2<'data> {
    /// String data for the symbols: +/- affixes and separators
    #[cfg_attr(feature = "serde", serde(borrow))]
    // We use a VarZeroCow here to reduce the stack size of DecimalSymbolsV2: instead of serializing multiple strs,
    // this type will now serialize as a single u8 buffer with optimized indexing that packs all the data together
    pub strings: VarZeroCow<'data, DecimalSymbolsStrs>,

    /// Settings used to determine where to place groups in the integer part of the number.
    pub grouping_sizes: GroupingSizesV1,
}

/// The digits for a given numbering system. This data ought to be stored in the `und` locale with an auxiliary key
/// set to the numbering system code.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(DecimalDigitsV1Marker = "decimal/digits@1")]
#[derive(Debug, PartialEq, Clone, Copy)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_decimal::provider))]
pub struct DecimalDigitsV1 {
    /// Digit characters for the current numbering system. In most systems, these digits are
    /// contiguous, but in some systems, such as *hanidec*, they are not contiguous.
    pub digits: [char; 10],
}

impl<'data> DecimalSymbolsV2<'data> {
    /// Return (prefix, suffix) for the minus sign
    pub fn minus_sign_affixes(&self) -> (&str, &str) {
        (
            self.strings.minus_sign_prefix(),
            self.strings.minus_sign_suffix(),
        )
    }
    /// Return (prefix, suffix) for the minus sign
    pub fn plus_sign_affixes(&self) -> (&str, &str) {
        (
            self.strings.plus_sign_prefix(),
            self.strings.plus_sign_suffix(),
        )
    }
    /// Return thhe decimal separator
    pub fn decimal_separator(&self) -> &str {
        self.strings.decimal_separator()
    }
    /// Return thhe decimal separator
    pub fn grouping_separator(&self) -> &str {
        self.strings.grouping_separator()
    }

    /// Return the numbering system
    pub fn numsys(&self) -> &str {
        self.strings.numsys()
    }
}

impl DecimalSymbolsV2<'static> {
    #[cfg(test)]
    /// Create a new en-US format for use in testing
    pub(crate) fn new_en_for_testing() -> Self {
        let strings = DecimalSymbolStrsBuilder {
            minus_sign_prefix: Cow::Borrowed("-"),
            minus_sign_suffix: Cow::Borrowed(""),
            plus_sign_prefix: Cow::Borrowed("+"),
            plus_sign_suffix: Cow::Borrowed(""),
            decimal_separator: ".".into(),
            grouping_separator: ",".into(),
            numsys: Cow::Borrowed("latn"),
        };
        Self {
            strings: VarZeroCow::from_encodeable(&strings),
            grouping_sizes: GroupingSizesV1 {
                primary: 3,
                secondary: 3,
                min_grouping: 1,
            },
        }
    }
}
