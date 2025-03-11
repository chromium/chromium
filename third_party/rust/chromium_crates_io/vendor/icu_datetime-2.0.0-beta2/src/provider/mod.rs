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
pub mod fields;
pub mod neo;
pub(crate) mod packed_pattern;
pub mod pattern;
#[cfg(feature = "datagen")]
pub mod skeleton;
pub mod time_zones;

pub use packed_pattern::*;

pub(crate) type ErasedPackedPatterns = icu_provider::marker::ErasedMarker<PackedPatterns<'static>>;

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

    impl_locations_v1!(Baked);
    impl_locations_root_v1!(Baked);
    impl_exemplar_cities_v1!(Baked);
    impl_exemplar_cities_root_v1!(Baked);
    impl_metazone_generic_names_long_v1!(Baked);
    impl_metazone_standard_names_long_v1!(Baked);
    impl_metazone_generic_names_short_v1!(Baked);
    impl_metazone_period_v1!(Baked);
    impl_metazone_specific_names_long_v1!(Baked);
    impl_metazone_specific_names_short_v1!(Baked);
    impl_time_zone_essentials_v1!(Baked);

    impl_weekday_names_v1!(Baked);
    impl_day_period_names_v1!(Baked);
    impl_glue_pattern_v1!(Baked);
    impl_time_neo_skeleton_patterns_v1!(Baked);

    impl_buddhist_year_names_v1!(Baked);
    impl_chinese_year_names_v1!(Baked);
    impl_coptic_year_names_v1!(Baked);
    impl_dangi_year_names_v1!(Baked);
    impl_ethiopian_year_names_v1!(Baked);
    impl_gregorian_year_names_v1!(Baked);
    impl_hebrew_year_names_v1!(Baked);
    impl_indian_year_names_v1!(Baked);
    impl_islamic_year_names_v1!(Baked);
    impl_japanese_year_names_v1!(Baked);
    impl_japanese_extended_year_names_v1!(Baked);
    impl_persian_year_names_v1!(Baked);
    impl_roc_year_names_v1!(Baked);

    impl_buddhist_month_names_v1!(Baked);
    impl_chinese_month_names_v1!(Baked);
    impl_coptic_month_names_v1!(Baked);
    impl_dangi_month_names_v1!(Baked);
    impl_ethiopian_month_names_v1!(Baked);
    impl_gregorian_month_names_v1!(Baked);
    impl_hebrew_month_names_v1!(Baked);
    impl_indian_month_names_v1!(Baked);
    impl_islamic_month_names_v1!(Baked);
    impl_japanese_month_names_v1!(Baked);
    impl_japanese_extended_month_names_v1!(Baked);
    impl_persian_month_names_v1!(Baked);
    impl_roc_month_names_v1!(Baked);

    impl_buddhist_date_neo_skeleton_patterns_v1!(Baked);
    impl_chinese_date_neo_skeleton_patterns_v1!(Baked);
    impl_coptic_date_neo_skeleton_patterns_v1!(Baked);
    impl_dangi_date_neo_skeleton_patterns_v1!(Baked);
    impl_ethiopian_date_neo_skeleton_patterns_v1!(Baked);
    impl_gregorian_date_neo_skeleton_patterns_v1!(Baked);
    impl_hebrew_date_neo_skeleton_patterns_v1!(Baked);
    impl_indian_date_neo_skeleton_patterns_v1!(Baked);
    impl_islamic_date_neo_skeleton_patterns_v1!(Baked);
    impl_japanese_date_neo_skeleton_patterns_v1!(Baked);
    impl_japanese_extended_date_neo_skeleton_patterns_v1!(Baked);
    impl_persian_date_neo_skeleton_patterns_v1!(Baked);
    impl_roc_date_neo_skeleton_patterns_v1!(Baked);
};

#[cfg(feature = "datagen")]
use icu_provider::prelude::*;

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] = &[
    time_zones::LocationsV1::INFO,
    time_zones::LocationsRootV1::INFO,
    time_zones::ExemplarCitiesV1::INFO,
    time_zones::ExemplarCitiesRootV1::INFO,
    time_zones::MetazoneGenericNamesLongV1::INFO,
    time_zones::MetazoneStandardNamesLongV1::INFO,
    time_zones::MetazoneGenericNamesShortV1::INFO,
    time_zones::MetazonePeriodV1::INFO,
    time_zones::MetazoneSpecificNamesLongV1::INFO,
    time_zones::MetazoneSpecificNamesShortV1::INFO,
    time_zones::TimeZoneEssentialsV1::INFO,
    neo::WeekdayNamesV1::INFO,
    neo::DayPeriodNamesV1::INFO,
    neo::GluePatternV1::INFO,
    TimeNeoSkeletonPatternsV1::INFO,
    neo::BuddhistYearNamesV1::INFO,
    neo::ChineseYearNamesV1::INFO,
    neo::CopticYearNamesV1::INFO,
    neo::DangiYearNamesV1::INFO,
    neo::EthiopianYearNamesV1::INFO,
    neo::GregorianYearNamesV1::INFO,
    neo::HebrewYearNamesV1::INFO,
    neo::IndianYearNamesV1::INFO,
    neo::IslamicYearNamesV1::INFO,
    neo::JapaneseYearNamesV1::INFO,
    neo::JapaneseExtendedYearNamesV1::INFO,
    neo::PersianYearNamesV1::INFO,
    neo::RocYearNamesV1::INFO,
    neo::BuddhistMonthNamesV1::INFO,
    neo::ChineseMonthNamesV1::INFO,
    neo::CopticMonthNamesV1::INFO,
    neo::DangiMonthNamesV1::INFO,
    neo::EthiopianMonthNamesV1::INFO,
    neo::GregorianMonthNamesV1::INFO,
    neo::HebrewMonthNamesV1::INFO,
    neo::IndianMonthNamesV1::INFO,
    neo::IslamicMonthNamesV1::INFO,
    neo::JapaneseMonthNamesV1::INFO,
    neo::JapaneseExtendedMonthNamesV1::INFO,
    neo::PersianMonthNamesV1::INFO,
    neo::RocMonthNamesV1::INFO,
    BuddhistDateNeoSkeletonPatternsV1::INFO,
    ChineseDateNeoSkeletonPatternsV1::INFO,
    CopticDateNeoSkeletonPatternsV1::INFO,
    DangiDateNeoSkeletonPatternsV1::INFO,
    EthiopianDateNeoSkeletonPatternsV1::INFO,
    GregorianDateNeoSkeletonPatternsV1::INFO,
    HebrewDateNeoSkeletonPatternsV1::INFO,
    IndianDateNeoSkeletonPatternsV1::INFO,
    IslamicDateNeoSkeletonPatternsV1::INFO,
    JapaneseDateNeoSkeletonPatternsV1::INFO,
    JapaneseExtendedDateNeoSkeletonPatternsV1::INFO,
    PersianDateNeoSkeletonPatternsV1::INFO,
    RocDateNeoSkeletonPatternsV1::INFO,
];
