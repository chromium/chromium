// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 The experimental development module of the `ICU4X` project.
//!
//! This module is published as its own crate ([`icu_experimental`](https://docs.rs/icu_experimental/latest/icu_experimental/))
//! and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
//!
//! It will usually undergo a major SemVer bump for every ICU4X release. Components in this
//! crate will eventually stabilize and move to their own top-level components.

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, feature = "std")), no_std)]
// No boilerplate, each module has their own
#![allow(clippy::module_inception)]

extern crate alloc;

pub mod compactdecimal;
pub mod dimension;
pub mod displaynames;
pub mod duration;
pub mod measure;
pub mod personnames;
pub mod relativetime;
pub mod transliterate;
pub mod unicodeset_parse;
pub mod units;

#[doc(hidden)] // compiled constructors look for the baked provider here
pub mod provider {
    #[cfg(feature = "compiled_data")]
    pub struct Baked;

    #[cfg(feature = "compiled_data")]
    #[allow(unused_imports)]
    const _: () = {
        use icu_experimental_data::*;
        pub mod icu {
            pub use crate as experimental;
            pub use icu_collections as collections;
            pub use icu_experimental_data::icu_locale as locale;
            pub use icu_plurals as plurals;
        }
        make_provider!(Baked);

        impl_long_compact_decimal_format_data_v1_marker!(Baked);
        impl_short_compact_decimal_format_data_v1_marker!(Baked);
        impl_short_currency_compact_v1_marker!(Baked);
        impl_currency_essentials_v1_marker!(Baked);
        impl_currency_displayname_v1_marker!(Baked);
        impl_currency_patterns_data_v1_marker!(Baked);
        impl_currency_extended_data_v1_marker!(Baked);
        impl_units_display_name_v1_marker!(Baked);
        impl_units_essentials_v1_marker!(Baked);
        impl_language_display_names_v1_marker!(Baked);
        impl_digital_duration_data_v1_marker!(Baked);
        impl_locale_display_names_v1_marker!(Baked);
        impl_region_display_names_v1_marker!(Baked);
        impl_script_display_names_v1_marker!(Baked);
        impl_variant_display_names_v1_marker!(Baked);
        impl_percent_essentials_v1_marker!(Baked);
        impl_person_names_format_v1_marker!(Baked);
        impl_long_day_relative_time_format_data_v1_marker!(Baked);
        impl_long_hour_relative_time_format_data_v1_marker!(Baked);
        impl_long_minute_relative_time_format_data_v1_marker!(Baked);
        impl_long_month_relative_time_format_data_v1_marker!(Baked);
        impl_long_quarter_relative_time_format_data_v1_marker!(Baked);
        impl_long_second_relative_time_format_data_v1_marker!(Baked);
        impl_long_week_relative_time_format_data_v1_marker!(Baked);
        impl_long_year_relative_time_format_data_v1_marker!(Baked);
        impl_narrow_day_relative_time_format_data_v1_marker!(Baked);
        impl_narrow_hour_relative_time_format_data_v1_marker!(Baked);
        impl_narrow_minute_relative_time_format_data_v1_marker!(Baked);
        impl_narrow_month_relative_time_format_data_v1_marker!(Baked);
        impl_narrow_quarter_relative_time_format_data_v1_marker!(Baked);
        impl_narrow_second_relative_time_format_data_v1_marker!(Baked);
        impl_narrow_week_relative_time_format_data_v1_marker!(Baked);
        impl_narrow_year_relative_time_format_data_v1_marker!(Baked);
        impl_short_day_relative_time_format_data_v1_marker!(Baked);
        impl_short_hour_relative_time_format_data_v1_marker!(Baked);
        impl_short_minute_relative_time_format_data_v1_marker!(Baked);
        impl_short_month_relative_time_format_data_v1_marker!(Baked);
        impl_short_quarter_relative_time_format_data_v1_marker!(Baked);
        impl_short_second_relative_time_format_data_v1_marker!(Baked);
        impl_short_week_relative_time_format_data_v1_marker!(Baked);
        impl_short_year_relative_time_format_data_v1_marker!(Baked);
        impl_transliterator_rules_v1_marker!(Baked);
        impl_units_info_v1_marker!(Baked);
        impl_units_trie_v1_marker!(Baked);
    };

    #[cfg(feature = "datagen")]
    use icu_provider::prelude::*;

    #[cfg(feature = "datagen")]
    /// The latest minimum set of keys required by this component.
    pub const MARKERS: &[DataMarkerInfo] = &[
        super::compactdecimal::provider::LongCompactDecimalFormatDataV1Marker::INFO,
        super::compactdecimal::provider::ShortCompactDecimalFormatDataV1Marker::INFO,
        super::compactdecimal::provider::LongCompactDecimalFormatDataV1Marker::INFO,
        super::compactdecimal::provider::ShortCompactDecimalFormatDataV1Marker::INFO,
        super::dimension::provider::currency_compact::ShortCurrencyCompactV1Marker::INFO,
        super::dimension::provider::currency_displayname::CurrencyDisplaynameV1Marker::INFO,
        super::dimension::provider::currency::CurrencyEssentialsV1Marker::INFO,
        super::dimension::provider::currency_patterns::CurrencyPatternsDataV1Marker::INFO,
        super::dimension::provider::extended_currency::CurrencyExtendedDataV1Marker::INFO,
        super::dimension::provider::percent::PercentEssentialsV1Marker::INFO,
        super::dimension::provider::units_essentials::UnitsEssentialsV1Marker::INFO,
        super::dimension::provider::units::UnitsDisplayNameV1Marker::INFO,
        super::displaynames::provider::LanguageDisplayNamesV1Marker::INFO,
        super::duration::provider::DigitalDurationDataV1Marker::INFO,
        super::displaynames::provider::LocaleDisplayNamesV1Marker::INFO,
        super::displaynames::provider::RegionDisplayNamesV1Marker::INFO,
        super::displaynames::provider::ScriptDisplayNamesV1Marker::INFO,
        super::displaynames::provider::VariantDisplayNamesV1Marker::INFO,
        super::measure::provider::trie::UnitsTrieV1Marker::INFO,
        super::personnames::provider::PersonNamesFormatV1Marker::INFO,
        super::relativetime::provider::LongDayRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::LongHourRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::LongMinuteRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::LongMonthRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::LongQuarterRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::LongSecondRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::LongWeekRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::LongYearRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::NarrowDayRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::NarrowHourRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::NarrowMinuteRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::NarrowMonthRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::NarrowQuarterRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::NarrowSecondRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::NarrowWeekRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::NarrowYearRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::ShortDayRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::ShortHourRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::ShortMinuteRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::ShortMonthRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::ShortQuarterRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::ShortSecondRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::ShortWeekRelativeTimeFormatDataV1Marker::INFO,
        super::relativetime::provider::ShortYearRelativeTimeFormatDataV1Marker::INFO,
        super::transliterate::provider::TransliteratorRulesV1Marker::INFO,
        super::units::provider::UnitsInfoV1Marker::INFO,
    ];
}
