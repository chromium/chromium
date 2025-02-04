// @generated
include!("long_compact_decimal_format_data_v1_marker.rs.data");
include!("short_compact_decimal_format_data_v1_marker.rs.data");
include!("short_currency_compact_v1_marker.rs.data");
include!("currency_displayname_v1_marker.rs.data");
include!("currency_essentials_v1_marker.rs.data");
include!("currency_extended_data_v1_marker.rs.data");
include!("currency_patterns_data_v1_marker.rs.data");
include!("language_display_names_v1_marker.rs.data");
include!("locale_display_names_v1_marker.rs.data");
include!("region_display_names_v1_marker.rs.data");
include!("script_display_names_v1_marker.rs.data");
include!("variant_display_names_v1_marker.rs.data");
include!("digital_duration_data_v1_marker.rs.data");
include!("percent_essentials_v1_marker.rs.data");
include!("person_names_format_v1_marker.rs.data");
include!("long_day_relative_time_format_data_v1_marker.rs.data");
include!("long_hour_relative_time_format_data_v1_marker.rs.data");
include!("long_minute_relative_time_format_data_v1_marker.rs.data");
include!("long_month_relative_time_format_data_v1_marker.rs.data");
include!("long_quarter_relative_time_format_data_v1_marker.rs.data");
include!("long_second_relative_time_format_data_v1_marker.rs.data");
include!("long_week_relative_time_format_data_v1_marker.rs.data");
include!("long_year_relative_time_format_data_v1_marker.rs.data");
include!("narrow_day_relative_time_format_data_v1_marker.rs.data");
include!("narrow_hour_relative_time_format_data_v1_marker.rs.data");
include!("narrow_minute_relative_time_format_data_v1_marker.rs.data");
include!("narrow_month_relative_time_format_data_v1_marker.rs.data");
include!("narrow_quarter_relative_time_format_data_v1_marker.rs.data");
include!("narrow_second_relative_time_format_data_v1_marker.rs.data");
include!("narrow_week_relative_time_format_data_v1_marker.rs.data");
include!("narrow_year_relative_time_format_data_v1_marker.rs.data");
include!("short_day_relative_time_format_data_v1_marker.rs.data");
include!("short_hour_relative_time_format_data_v1_marker.rs.data");
include!("short_minute_relative_time_format_data_v1_marker.rs.data");
include!("short_month_relative_time_format_data_v1_marker.rs.data");
include!("short_quarter_relative_time_format_data_v1_marker.rs.data");
include!("short_second_relative_time_format_data_v1_marker.rs.data");
include!("short_week_relative_time_format_data_v1_marker.rs.data");
include!("short_year_relative_time_format_data_v1_marker.rs.data");
include!("transliterator_rules_v1_marker.rs.data");
include!("units_display_name_v1_marker.rs.data");
include!("units_essentials_v1_marker.rs.data");
include!("units_info_v1_marker.rs.data");
include!("units_trie_v1_marker.rs.data");
/// Marks a type as a data provider. You can then use macros like
/// `impl_core_helloworld_v1` to add implementations.
///
/// ```ignore
/// struct MyProvider;
/// const _: () = {
///     include!("path/to/generated/macros.rs");
///     make_provider!(MyProvider);
///     impl_core_helloworld_v1!(MyProvider);
/// }
/// ```
#[doc(hidden)]
#[macro_export]
macro_rules! __make_provider {
    ($ name : ty) => {
        #[clippy::msrv = "1.71.1"]
        impl $name {
            #[allow(dead_code)]
            pub(crate) const MUST_USE_MAKE_PROVIDER_MACRO: () = ();
        }
        icu_provider::marker::impl_data_provider_never_marker!($name);
    };
}
#[doc(inline)]
pub use __make_provider as make_provider;
#[allow(unused_macros)]
macro_rules! impl_data_provider {
    ($ provider : ty) => {
        make_provider!($provider);
        impl_long_compact_decimal_format_data_v1_marker!($provider);
        impl_short_compact_decimal_format_data_v1_marker!($provider);
        impl_short_currency_compact_v1_marker!($provider);
        impl_currency_displayname_v1_marker!($provider);
        impl_currency_essentials_v1_marker!($provider);
        impl_currency_extended_data_v1_marker!($provider);
        impl_currency_patterns_data_v1_marker!($provider);
        impl_language_display_names_v1_marker!($provider);
        impl_locale_display_names_v1_marker!($provider);
        impl_region_display_names_v1_marker!($provider);
        impl_script_display_names_v1_marker!($provider);
        impl_variant_display_names_v1_marker!($provider);
        impl_digital_duration_data_v1_marker!($provider);
        impl_percent_essentials_v1_marker!($provider);
        impl_person_names_format_v1_marker!($provider);
        impl_long_day_relative_time_format_data_v1_marker!($provider);
        impl_long_hour_relative_time_format_data_v1_marker!($provider);
        impl_long_minute_relative_time_format_data_v1_marker!($provider);
        impl_long_month_relative_time_format_data_v1_marker!($provider);
        impl_long_quarter_relative_time_format_data_v1_marker!($provider);
        impl_long_second_relative_time_format_data_v1_marker!($provider);
        impl_long_week_relative_time_format_data_v1_marker!($provider);
        impl_long_year_relative_time_format_data_v1_marker!($provider);
        impl_narrow_day_relative_time_format_data_v1_marker!($provider);
        impl_narrow_hour_relative_time_format_data_v1_marker!($provider);
        impl_narrow_minute_relative_time_format_data_v1_marker!($provider);
        impl_narrow_month_relative_time_format_data_v1_marker!($provider);
        impl_narrow_quarter_relative_time_format_data_v1_marker!($provider);
        impl_narrow_second_relative_time_format_data_v1_marker!($provider);
        impl_narrow_week_relative_time_format_data_v1_marker!($provider);
        impl_narrow_year_relative_time_format_data_v1_marker!($provider);
        impl_short_day_relative_time_format_data_v1_marker!($provider);
        impl_short_hour_relative_time_format_data_v1_marker!($provider);
        impl_short_minute_relative_time_format_data_v1_marker!($provider);
        impl_short_month_relative_time_format_data_v1_marker!($provider);
        impl_short_quarter_relative_time_format_data_v1_marker!($provider);
        impl_short_second_relative_time_format_data_v1_marker!($provider);
        impl_short_week_relative_time_format_data_v1_marker!($provider);
        impl_short_year_relative_time_format_data_v1_marker!($provider);
        impl_transliterator_rules_v1_marker!($provider);
        impl_units_display_name_v1_marker!($provider);
        impl_units_essentials_v1_marker!($provider);
        impl_units_info_v1_marker!($provider);
        impl_units_trie_v1_marker!($provider);
    };
}
#[allow(unused_macros)]
macro_rules! impl_any_provider {
    ($ provider : ty) => {
        #[clippy::msrv = "1.71.1"]
        impl icu_provider::any::AnyProvider for $provider {
            fn load_any(&self, marker: icu_provider::DataMarkerInfo, req: icu_provider::DataRequest) -> Result<icu_provider::AnyResponse, icu_provider::DataError> {
                match marker.path.hashed() {
                    h if h == <icu::experimental::compactdecimal::provider::LongCompactDecimalFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::compactdecimal::provider::LongCompactDecimalFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::compactdecimal::provider::ShortCompactDecimalFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::compactdecimal::provider::ShortCompactDecimalFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::dimension::provider::currency_compact::ShortCurrencyCompactV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::dimension::provider::currency_compact::ShortCurrencyCompactV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::dimension::provider::currency_displayname::CurrencyDisplaynameV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::dimension::provider::currency_displayname::CurrencyDisplaynameV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::dimension::provider::currency::CurrencyEssentialsV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::dimension::provider::currency::CurrencyEssentialsV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::dimension::provider::extended_currency::CurrencyExtendedDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::dimension::provider::extended_currency::CurrencyExtendedDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::dimension::provider::currency_patterns::CurrencyPatternsDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::dimension::provider::currency_patterns::CurrencyPatternsDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::displaynames::provider::LanguageDisplayNamesV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::displaynames::provider::LanguageDisplayNamesV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::displaynames::provider::LocaleDisplayNamesV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::displaynames::provider::LocaleDisplayNamesV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::displaynames::provider::RegionDisplayNamesV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::displaynames::provider::RegionDisplayNamesV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::displaynames::provider::ScriptDisplayNamesV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::displaynames::provider::ScriptDisplayNamesV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::displaynames::provider::VariantDisplayNamesV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::displaynames::provider::VariantDisplayNamesV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::duration::provider::DigitalDurationDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::duration::provider::DigitalDurationDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::dimension::provider::percent::PercentEssentialsV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::dimension::provider::percent::PercentEssentialsV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::personnames::provider::PersonNamesFormatV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::personnames::provider::PersonNamesFormatV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::LongDayRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::LongDayRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::LongHourRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::LongHourRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::LongMinuteRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::LongMinuteRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::LongMonthRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::LongMonthRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::LongQuarterRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::LongQuarterRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::LongSecondRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::LongSecondRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::LongWeekRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::LongWeekRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::LongYearRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::LongYearRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::NarrowDayRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::NarrowDayRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::NarrowHourRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::NarrowHourRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::NarrowMinuteRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::NarrowMinuteRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::NarrowMonthRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::NarrowMonthRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::NarrowQuarterRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::NarrowQuarterRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::NarrowSecondRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::NarrowSecondRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::NarrowWeekRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::NarrowWeekRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::NarrowYearRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::NarrowYearRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::ShortDayRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::ShortDayRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::ShortHourRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::ShortHourRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::ShortMinuteRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::ShortMinuteRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::ShortMonthRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::ShortMonthRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::ShortQuarterRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::ShortQuarterRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::ShortSecondRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::ShortSecondRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::ShortWeekRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::ShortWeekRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::relativetime::provider::ShortYearRelativeTimeFormatDataV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::relativetime::provider::ShortYearRelativeTimeFormatDataV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::transliterate::provider::TransliteratorRulesV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::transliterate::provider::TransliteratorRulesV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::dimension::provider::units::UnitsDisplayNameV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::dimension::provider::units::UnitsDisplayNameV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::dimension::provider::units_essentials::UnitsEssentialsV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::dimension::provider::units_essentials::UnitsEssentialsV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::units::provider::UnitsInfoV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::units::provider::UnitsInfoV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::experimental::measure::provider::trie::UnitsTrieV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::experimental::measure::provider::trie::UnitsTrieV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    _ => Err(icu_provider::DataErrorKind::MarkerNotFound.with_req(marker, req)),
                }
            }
        }
    };
}
