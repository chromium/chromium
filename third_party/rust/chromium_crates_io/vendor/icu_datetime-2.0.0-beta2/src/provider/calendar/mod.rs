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

size_test!(DateLengths, date_lengths_v1_size, 224);

icu_provider::data_marker!(
    /// `BuddhistDateLengthsV1`
    BuddhistDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `ChineseDateLengthsV1`
    ChineseDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `CopticDateLengthsV1`
    CopticDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `DangiDateLengthsV1`
    DangiDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `EthiopianDateLengthsV1`
    EthiopianDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `GregorianDateLengthsV1`
    GregorianDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `HebrewDateLengthsV1`
    HebrewDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `IndianDateLengthsV1`
    IndianDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `IslamicDateLengthsV1`
    IslamicDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `JapaneseDateLengthsV1`
    JapaneseDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `JapaneseExtendedDateLengthsV1`
    JapaneseExtendedDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `PersianDateLengthsV1`
    PersianDateLengthsV1,
    DateLengths<'static>
);
icu_provider::data_marker!(
    /// `RocDateLengthsV1`
    RocDateLengthsV1,
    DateLengths<'static>
);

/// Pattern data for dates.
#[doc = date_lengths_v1_size!()]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, Default, zerofrom::ZeroFrom, yoke::Yokeable)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct DateLengths<'data> {
    /// Date pattern data, broken down by pattern length.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub date: patterns::LengthPatterns<'data>,

    /// Patterns used to combine date and time length patterns into full date_time patterns.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub length_combinations: patterns::GenericLengthPatterns<'data>,
}

icu_provider::data_struct!(
    DateLengths<'_>,
    #[cfg(feature = "datagen")]
);

icu_provider::data_marker!(
    /// `TimeLengthsV1`
    TimeLengthsV1,
    TimeLengths<'static>
);

size_test!(TimeLengths, time_lengths_v1_size, 264);

/// Pattern data for times.
#[doc = time_lengths_v1_size!()]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::calendar))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct TimeLengths<'data> {
    /// These patterns are common uses of time formatting, broken down by the length of the
    /// pattern. Users can override the hour cycle with a preference, so there are two
    /// pattern groups stored here. Note that the pattern will contain either h11 or h12.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub time_h11_h12: patterns::LengthPatterns<'data>,

    /// These patterns are common uses of time formatting, broken down by the length of the
    /// pattern. Users can override the hour cycle with a preference, so there are two
    /// pattern groups stored here. Note that the pattern will contain either h23 or h24.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub time_h23_h24: patterns::LengthPatterns<'data>,

    /// By default a locale will prefer one hour cycle type over another.
    pub preferred_hour_cycle: pattern::CoarseHourCycle,
}

icu_provider::data_struct!(TimeLengths<'_>, #[cfg(feature = "datagen")]);

/// Data structs for date / time patterns that store data corresponding to pattern lengths
/// and/or plural forms.
pub mod patterns {
    use super::*;
    use crate::provider::pattern::runtime::{self, GenericPattern};

    /// An enum containing four lengths (full, long, medium, short) for interfacing
    /// with [`LengthPatterns`] and [`GenericLengthPatterns`]
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
    pub struct LengthPatterns<'data> {
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
    pub struct GenericLengthPatterns<'data> {
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
}
