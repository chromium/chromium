// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data structs for calendar-specific symbols and patterns.

#[cfg(feature = "datagen")]
mod skeletons;
mod symbols;

use crate::provider::pattern;
use crate::size_test_macro::size_test;
use icu_provider::prelude::*;
#[cfg(feature = "datagen")]
pub use skeletons::*;
pub use symbols::*;

size_test!(DateLengthsV1, date_lengths_v1_size, 224);

/// Pattern data for dates.
#[doc = date_lengths_v1_size!()]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(BuddhistDateLengthsV1Marker, "datetime/buddhist/datelengths@1"),
    marker(ChineseDateLengthsV1Marker, "datetime/chinese/datelengths@1"),
    marker(CopticDateLengthsV1Marker, "datetime/coptic/datelengths@1"),
    marker(DangiDateLengthsV1Marker, "datetime/dangi/datelengths@1"),
    marker(EthiopianDateLengthsV1Marker, "datetime/ethiopic/datelengths@1"),
    marker(GregorianDateLengthsV1Marker, "datetime/gregory/datelengths@1"),
    marker(HebrewDateLengthsV1Marker, "datetime/hebrew/datelengths@1"),
    marker(IndianDateLengthsV1Marker, "datetime/indian/datelengths@1"),
    marker(IslamicDateLengthsV1Marker, "datetime/islamic/datelengths@1"),
    marker(JapaneseDateLengthsV1Marker, "datetime/japanese/datelengths@1"),
    marker(JapaneseExtendedDateLengthsV1Marker, "datetime/japanext/datelengths@1"),
    marker(PersianDateLengthsV1Marker, "datetime/persian/datelengths@1"),
    marker(RocDateLengthsV1Marker, "datetime/roc/datelengths@1")
)]
#[derive(Debug, PartialEq, Clone, Default)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct DateLengthsV1<'data> {
    /// Date pattern data, broken down by pattern length.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub date: patterns::LengthPatternsV1<'data>,

    /// Patterns used to combine date and time length patterns into full date_time patterns.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub length_combinations: patterns::GenericLengthPatternsV1<'data>,
}

size_test!(TimeLengthsV1, time_lengths_v1_size, 264);

/// Pattern data for times.
#[doc = time_lengths_v1_size!()]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(TimeLengthsV1Marker, "datetime/timelengths@1",))]
#[derive(Debug, PartialEq, Clone, Default)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct TimeLengthsV1<'data> {
    /// These patterns are common uses of time formatting, broken down by the length of the
    /// pattern. Users can override the hour cycle with a preference, so there are two
    /// pattern groups stored here. Note that the pattern will contain either h11 or h12.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub time_h11_h12: patterns::LengthPatternsV1<'data>,

    /// These patterns are common uses of time formatting, broken down by the length of the
    /// pattern. Users can override the hour cycle with a preference, so there are two
    /// pattern groups stored here. Note that the pattern will contain either h23 or h24.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub time_h23_h24: patterns::LengthPatternsV1<'data>,

    /// By default a locale will prefer one hour cycle type over another.
    pub preferred_hour_cycle: pattern::CoarseHourCycle,
}

/// Data structs for date / time patterns that store data corresponding to pattern lengths
/// and/or plural forms.
pub mod patterns {
    use super::*;
    use crate::provider::pattern::runtime::{self, GenericPattern};

    /// An enum containing four lengths (full, long, medium, short) for interfacing
    /// with [`LengthPatternsV1`] and [`GenericLengthPatternsV1`]
    #[derive(Debug)]
    pub enum FullLongMediumShort {
        /// "full" length
        Full,
        /// "long" length
        Long,
        /// "medium" length
        Medium,
        /// "short" length
        Short,
    }

    /// Data struct for date/time patterns broken down by pattern length.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    #[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
    #[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
    #[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar::patterns))]
    #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
    pub struct LengthPatternsV1<'data> {
        /// A full length date/time pattern.
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub full: runtime::Pattern<'data>,
        /// A long length date/time pattern.
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub long: runtime::Pattern<'data>,
        /// A medium length date/time pattern.
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub medium: runtime::Pattern<'data>,
        /// A short length date/time pattern.
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub short: runtime::Pattern<'data>,
    }

    /// Data struct for generic date/time patterns, broken down by pattern length.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    #[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
    #[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
    #[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar::patterns))]
    #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
    pub struct GenericLengthPatternsV1<'data> {
        /// A full length glue pattern of other formatted elements.
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub full: GenericPattern<'data>,
        /// A long length glue pattern of other formatted elements.
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub long: GenericPattern<'data>,
        /// A medium length glue pattern of other formatted elements.
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub medium: GenericPattern<'data>,
        /// A short length glue pattern of other formatted elements.
        #[cfg_attr(feature = "serde", serde(borrow))]
        pub short: GenericPattern<'data>,
    }

    /// A general purpose pattern representation. Used for date-time glue patterns.
    ///
    /// Expresses the formatting positions of other formatted elements (ex: the order
    /// and formatting of a date and a time within a date-time pattern).
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
    /// to be stable, their Rust representation might not be. Use with caution.
    /// </div>
    #[icu_provider::data_struct]
    #[derive(Debug, PartialEq, Clone, Default)]
    #[cfg_attr(feature = "datagen", derive(serde::Serialize))]
    #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
    pub struct GenericPatternV1<'data>(
        #[cfg_attr(feature = "serde", serde(borrow))] pub GenericPattern<'data>,
    );
}
