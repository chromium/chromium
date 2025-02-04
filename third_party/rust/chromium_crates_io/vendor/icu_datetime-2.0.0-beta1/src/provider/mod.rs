// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! 🚧 \[Unstable\] Data provider struct definitions for this ICU4X component.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

pub mod calendar;
pub mod neo;
pub(crate) mod packed_pattern;
pub mod pattern;
#[cfg(feature = "datagen")]
pub mod skeleton;
pub mod time_zones;

pub use packed_pattern::*;

pub(crate) type ErasedPackedPatterns =
    icu_provider::marker::ErasedMarker<PackedPatternsV1<'static>>;

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
    use icu_datetime_data::*;
    pub mod icu {
        pub use crate as datetime;
        pub use icu_datetime_data::icu_locale as locale;
    }
    make_provider!(Baked);

    impl_locations_v1_marker!(Baked);
    impl_metazone_generic_names_long_v1_marker!(Baked);
    impl_metazone_generic_names_short_v1_marker!(Baked);
    impl_metazone_period_v1_marker!(Baked);
    impl_metazone_specific_names_long_v1_marker!(Baked);
    impl_metazone_specific_names_short_v1_marker!(Baked);
    impl_time_zone_essentials_v1_marker!(Baked);

    impl_weekday_names_v1_marker!(Baked);
    impl_day_period_names_v1_marker!(Baked);
    impl_glue_pattern_v1_marker!(Baked);
    impl_time_neo_skeleton_patterns_v1_marker!(Baked);

    impl_buddhist_year_names_v1_marker!(Baked);
    impl_chinese_year_names_v1_marker!(Baked);
    impl_coptic_year_names_v1_marker!(Baked);
    impl_dangi_year_names_v1_marker!(Baked);
    impl_ethiopian_year_names_v1_marker!(Baked);
    impl_gregorian_year_names_v1_marker!(Baked);
    impl_hebrew_year_names_v1_marker!(Baked);
    impl_indian_year_names_v1_marker!(Baked);
    impl_islamic_year_names_v1_marker!(Baked);
    impl_japanese_year_names_v1_marker!(Baked);
    impl_japanese_extended_year_names_v1_marker!(Baked);
    impl_persian_year_names_v1_marker!(Baked);
    impl_roc_year_names_v1_marker!(Baked);

    impl_buddhist_month_names_v1_marker!(Baked);
    impl_chinese_month_names_v1_marker!(Baked);
    impl_coptic_month_names_v1_marker!(Baked);
    impl_dangi_month_names_v1_marker!(Baked);
    impl_ethiopian_month_names_v1_marker!(Baked);
    impl_gregorian_month_names_v1_marker!(Baked);
    impl_hebrew_month_names_v1_marker!(Baked);
    impl_indian_month_names_v1_marker!(Baked);
    impl_islamic_month_names_v1_marker!(Baked);
    impl_japanese_month_names_v1_marker!(Baked);
    impl_japanese_extended_month_names_v1_marker!(Baked);
    impl_persian_month_names_v1_marker!(Baked);
    impl_roc_month_names_v1_marker!(Baked);

    impl_buddhist_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_chinese_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_coptic_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_dangi_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_ethiopian_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_gregorian_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_hebrew_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_indian_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_islamic_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_japanese_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_japanese_extended_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_persian_date_neo_skeleton_patterns_v1_marker!(Baked);
    impl_roc_date_neo_skeleton_patterns_v1_marker!(Baked);
};

#[cfg(feature = "datagen")]
use icu_provider::prelude::*;

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] = &[
    time_zones::LocationsV1Marker::INFO,
    time_zones::MetazoneGenericNamesLongV1Marker::INFO,
    time_zones::MetazoneGenericNamesShortV1Marker::INFO,
    time_zones::MetazonePeriodV1Marker::INFO,
    time_zones::MetazoneSpecificNamesLongV1Marker::INFO,
    time_zones::MetazoneSpecificNamesShortV1Marker::INFO,
    time_zones::TimeZoneEssentialsV1Marker::INFO,
    neo::WeekdayNamesV1Marker::INFO,
    neo::DayPeriodNamesV1Marker::INFO,
    neo::GluePatternV1Marker::INFO,
    TimeNeoSkeletonPatternsV1Marker::INFO,
    neo::BuddhistYearNamesV1Marker::INFO,
    neo::ChineseYearNamesV1Marker::INFO,
    neo::CopticYearNamesV1Marker::INFO,
    neo::DangiYearNamesV1Marker::INFO,
    neo::EthiopianYearNamesV1Marker::INFO,
    neo::GregorianYearNamesV1Marker::INFO,
    neo::HebrewYearNamesV1Marker::INFO,
    neo::IndianYearNamesV1Marker::INFO,
    neo::IslamicYearNamesV1Marker::INFO,
    neo::JapaneseYearNamesV1Marker::INFO,
    neo::JapaneseExtendedYearNamesV1Marker::INFO,
    neo::PersianYearNamesV1Marker::INFO,
    neo::RocYearNamesV1Marker::INFO,
    neo::BuddhistMonthNamesV1Marker::INFO,
    neo::ChineseMonthNamesV1Marker::INFO,
    neo::CopticMonthNamesV1Marker::INFO,
    neo::DangiMonthNamesV1Marker::INFO,
    neo::EthiopianMonthNamesV1Marker::INFO,
    neo::GregorianMonthNamesV1Marker::INFO,
    neo::HebrewMonthNamesV1Marker::INFO,
    neo::IndianMonthNamesV1Marker::INFO,
    neo::IslamicMonthNamesV1Marker::INFO,
    neo::JapaneseMonthNamesV1Marker::INFO,
    neo::JapaneseExtendedMonthNamesV1Marker::INFO,
    neo::PersianMonthNamesV1Marker::INFO,
    neo::RocMonthNamesV1Marker::INFO,
    BuddhistDateNeoSkeletonPatternsV1Marker::INFO,
    ChineseDateNeoSkeletonPatternsV1Marker::INFO,
    CopticDateNeoSkeletonPatternsV1Marker::INFO,
    DangiDateNeoSkeletonPatternsV1Marker::INFO,
    EthiopianDateNeoSkeletonPatternsV1Marker::INFO,
    GregorianDateNeoSkeletonPatternsV1Marker::INFO,
    HebrewDateNeoSkeletonPatternsV1Marker::INFO,
    IndianDateNeoSkeletonPatternsV1Marker::INFO,
    IslamicDateNeoSkeletonPatternsV1Marker::INFO,
    JapaneseDateNeoSkeletonPatternsV1Marker::INFO,
    JapaneseExtendedDateNeoSkeletonPatternsV1Marker::INFO,
    PersianDateNeoSkeletonPatternsV1Marker::INFO,
    RocDateNeoSkeletonPatternsV1Marker::INFO,
];
