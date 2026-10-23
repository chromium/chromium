// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, doc)), no_std)]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
    )
)]
#![warn(missing_docs)]

//! 🚧 The experimental development module of the `ICU4X` project.
//!
//! This module is published as its own crate ([`icu_experimental`](https://docs.rs/icu_experimental/latest/icu_experimental/))
//! and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
//!
//! It will usually undergo a major `SemVer` bump for every ICU4X release. Components in this
//! crate will eventually stabilize and move to their own top-level components.

#![allow(clippy::module_inception)]

extern crate alloc;

pub mod dimension;
pub mod displaynames;
pub mod duration;
pub mod measure;
pub mod personnames;
pub mod relativetime;
pub mod transliterate;
pub mod units;

#[doc(hidden)] // compiled constructors look for the baked provider here
pub mod provider {
    // Provider structs must be stable
    #![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

    #[cfg(feature = "compiled_data")]
    #[derive(Debug)]
    pub struct Baked;

    #[cfg(feature = "compiled_data")]
    #[allow(unused_imports)]
    const _: () = {
        use icu_experimental_data::*;
        pub mod icu {
            pub use crate as experimental;
            pub use icu_collections as collections;
            pub use icu_decimal as decimal;
            pub use icu_plurals as plurals;
        }
        make_provider!(Baked);

        impl_currency_essentials_v1!(Baked);
        impl_currency_symbols_v1!(Baked);
        impl_currency_decimal_symbols_v1!(Baked);
        impl_currency_displayname_v1!(Baked);
        impl_currency_patterns_data_v1!(Baked);
        impl_currency_extended_data_v1!(Baked);
        impl_currency_fractions_v1!(Baked);
        impl_currency_patterns_no_currency_v1!(Baked);
        impl_units_names_area_core_v1!(Baked);
        impl_units_names_area_extended_v1!(Baked);
        impl_units_names_area_outlier_v1!(Baked);
        impl_units_names_duration_core_v1!(Baked);
        impl_units_names_duration_extended_v1!(Baked);
        impl_units_names_duration_outlier_v1!(Baked);
        impl_units_names_length_core_v1!(Baked);
        impl_units_names_length_extended_v1!(Baked);
        impl_units_names_length_outlier_v1!(Baked);
        impl_units_names_mass_core_v1!(Baked);
        impl_units_names_mass_extended_v1!(Baked);
        impl_units_names_mass_outlier_v1!(Baked);
        impl_units_names_volume_core_v1!(Baked);
        impl_units_names_volume_extended_v1!(Baked);
        impl_units_names_volume_outlier_v1!(Baked);
        impl_units_essentials_v1!(Baked);
        impl_locale_names_language_v0!(Baked);
        impl_units_duration_digital_v1!(Baked);
        impl_locale_names_locale_v0!(Baked);
        impl_locale_names_region_v0!(Baked);
        impl_locale_names_script_v0!(Baked);
        impl_locale_names_variant_v0!(Baked);
        impl_decimal_percent_v1!(Baked);
        impl_person_names_format_v1!(Baked);
        impl_datetime_relative_day_long_v1!(Baked);
        impl_datetime_relative_day_narrow_v1!(Baked);
        impl_datetime_relative_day_short_v1!(Baked);
        impl_datetime_relative_hour_long_v1!(Baked);
        impl_datetime_relative_hour_narrow_v1!(Baked);
        impl_datetime_relative_hour_short_v1!(Baked);
        impl_datetime_relative_minute_long_v1!(Baked);
        impl_datetime_relative_minute_narrow_v1!(Baked);
        impl_datetime_relative_minute_short_v1!(Baked);
        impl_datetime_relative_month_long_v1!(Baked);
        impl_datetime_relative_month_narrow_v1!(Baked);
        impl_datetime_relative_month_short_v1!(Baked);
        impl_datetime_relative_quarter_long_v1!(Baked);
        impl_datetime_relative_quarter_narrow_v1!(Baked);
        impl_datetime_relative_quarter_short_v1!(Baked);
        impl_datetime_relative_second_long_v1!(Baked);
        impl_datetime_relative_second_narrow_v1!(Baked);
        impl_datetime_relative_second_short_v1!(Baked);
        impl_datetime_relative_week_long_v1!(Baked);
        impl_datetime_relative_week_narrow_v1!(Baked);
        impl_datetime_relative_week_short_v1!(Baked);
        impl_datetime_relative_year_long_v1!(Baked);
        impl_datetime_relative_year_narrow_v1!(Baked);
        impl_datetime_relative_year_short_v1!(Baked);
        impl_transliterator_rules_v1!(Baked);
        impl_units_info_v1!(Baked);
        impl_units_id_v1!(Baked);
    };

    #[cfg(feature = "datagen")]
    use icu_provider::prelude::*;

    #[cfg(feature = "datagen")]
    /// The latest minimum set of keys required by this component.
    pub const MARKERS: &[DataMarkerInfo] = &[
        super::dimension::provider::currency::displayname::CurrencyDisplaynameV1::INFO,
        super::dimension::provider::currency::essentials::CurrencyEssentialsV1::INFO,
        super::dimension::provider::currency::symbols::CurrencySymbolsV1::INFO,
        super::dimension::provider::currency::symbols::CurrencyDecimalSymbolsV1::INFO,
        super::dimension::provider::currency::patterns::CurrencyPatternsDataV1::INFO,
        super::dimension::provider::currency::extended::CurrencyExtendedDataV1::INFO,
        super::dimension::provider::currency::fractions::CurrencyFractionsV1::INFO,
        super::dimension::provider::currency::no_currency::CurrencyPatternsNoCurrencyV1::INFO,
        super::dimension::provider::percent::DecimalPercentV1::INFO,
        super::dimension::provider::units::essentials::UnitsEssentialsV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesAreaCoreV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesAreaExtendedV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesAreaOutlierV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesDurationCoreV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesDurationExtendedV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesDurationOutlierV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesLengthCoreV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesLengthExtendedV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesLengthOutlierV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesMassCoreV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesMassExtendedV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesMassOutlierV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesVolumeCoreV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesVolumeExtendedV1::INFO,
        super::dimension::provider::units::categorized_display_names::UnitsNamesVolumeOutlierV1::INFO,
        super::displaynames::provider::LocaleNamesLanguageV0::INFO,
        super::duration::provider::UnitsDurationDigitalV1::INFO,
        super::displaynames::provider::LocaleNamesLocaleV0::INFO,
        super::displaynames::provider::LocaleNamesRegionV0::INFO,
        super::displaynames::provider::LocaleNamesScriptV0::INFO,
        super::displaynames::provider::LocaleNamesVariantV0::INFO,
        super::measure::provider::UnitsIdV1::INFO,
        super::personnames::provider::PersonNamesFormatV1::INFO,
        super::relativetime::provider::DatetimeRelativeDayLongV1::INFO,
        super::relativetime::provider::DatetimeRelativeHourLongV1::INFO,
        super::relativetime::provider::DatetimeRelativeMinuteLongV1::INFO,
        super::relativetime::provider::DatetimeRelativeMonthLongV1::INFO,
        super::relativetime::provider::DatetimeRelativeQuarterLongV1::INFO,
        super::relativetime::provider::DatetimeRelativeSecondLongV1::INFO,
        super::relativetime::provider::DatetimeRelativeWeekLongV1::INFO,
        super::relativetime::provider::DatetimeRelativeYearLongV1::INFO,
        super::relativetime::provider::DatetimeRelativeDayNarrowV1::INFO,
        super::relativetime::provider::DatetimeRelativeHourNarrowV1::INFO,
        super::relativetime::provider::DatetimeRelativeMinuteNarrowV1::INFO,
        super::relativetime::provider::DatetimeRelativeMonthNarrowV1::INFO,
        super::relativetime::provider::DatetimeRelativeQuarterNarrowV1::INFO,
        super::relativetime::provider::DatetimeRelativeSecondNarrowV1::INFO,
        super::relativetime::provider::DatetimeRelativeWeekNarrowV1::INFO,
        super::relativetime::provider::DatetimeRelativeYearNarrowV1::INFO,
        super::relativetime::provider::DatetimeRelativeDayShortV1::INFO,
        super::relativetime::provider::DatetimeRelativeHourShortV1::INFO,
        super::relativetime::provider::DatetimeRelativeMinuteShortV1::INFO,
        super::relativetime::provider::DatetimeRelativeMonthShortV1::INFO,
        super::relativetime::provider::DatetimeRelativeQuarterShortV1::INFO,
        super::relativetime::provider::DatetimeRelativeSecondShortV1::INFO,
        super::relativetime::provider::DatetimeRelativeWeekShortV1::INFO,
        super::relativetime::provider::DatetimeRelativeYearShortV1::INFO,
        super::transliterate::provider::TransliteratorRulesV1::INFO,
        super::units::provider::UnitsInfoV1::INFO,
    ];
}
