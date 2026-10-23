// @generated
include!("datetime_relative_hour_long_v1.rs.data");
include!("person_names_format_v1.rs.data");
include!("units_duration_digital_v1.rs.data");
include!("datetime_relative_quarter_long_v1.rs.data");
include!("units_names_mass_outlier_v1.rs.data");
include!("units_id_v1.rs.data");
include!("datetime_relative_quarter_short_v1.rs.data");
include!("datetime_relative_week_long_v1.rs.data");
include!("datetime_relative_minute_long_v1.rs.data");
include!("units_names_duration_extended_v1.rs.data");
include!("locale_names_region_v0.rs.data");
include!("datetime_relative_second_short_v1.rs.data");
include!("datetime_relative_month_narrow_v1.rs.data");
include!("datetime_relative_month_long_v1.rs.data");
include!("currency_displayname_v1.rs.data");
include!("currency_patterns_data_v1.rs.data");
include!("datetime_relative_day_long_v1.rs.data");
include!("datetime_relative_second_narrow_v1.rs.data");
include!("currency_extended_data_v1.rs.data");
include!("datetime_relative_week_narrow_v1.rs.data");
include!("units_names_volume_extended_v1.rs.data");
include!("datetime_relative_quarter_narrow_v1.rs.data");
include!("datetime_relative_hour_narrow_v1.rs.data");
include!("datetime_relative_year_long_v1.rs.data");
include!("datetime_relative_minute_short_v1.rs.data");
include!("currency_patterns_no_currency_v1.rs.data");
include!("datetime_relative_hour_short_v1.rs.data");
include!("units_names_area_extended_v1.rs.data");
include!("units_names_length_core_v1.rs.data");
include!("datetime_relative_week_short_v1.rs.data");
include!("units_names_area_core_v1.rs.data");
include!("units_essentials_v1.rs.data");
include!("units_names_volume_outlier_v1.rs.data");
include!("datetime_relative_day_narrow_v1.rs.data");
include!("units_names_mass_extended_v1.rs.data");
include!("units_names_area_outlier_v1.rs.data");
include!("datetime_relative_day_short_v1.rs.data");
include!("units_names_mass_core_v1.rs.data");
include!("units_names_length_extended_v1.rs.data");
include!("locale_names_language_v0.rs.data");
include!("currency_essentials_v1.rs.data");
include!("locale_names_script_v0.rs.data");
include!("currency_decimal_symbols_v1.rs.data");
include!("units_names_length_outlier_v1.rs.data");
include!("datetime_relative_month_short_v1.rs.data");
include!("locale_names_locale_v0.rs.data");
include!("datetime_relative_second_long_v1.rs.data");
include!("locale_names_variant_v0.rs.data");
include!("transliterator_rules_v1.rs.data");
include!("units_names_duration_outlier_v1.rs.data");
include!("datetime_relative_year_narrow_v1.rs.data");
include!("units_names_volume_core_v1.rs.data");
include!("currency_symbols_v1.rs.data");
include!("datetime_relative_year_short_v1.rs.data");
include!("units_names_duration_core_v1.rs.data");
include!("currency_fractions_v1.rs.data");
include!("units_info_v1.rs.data");
include!("datetime_relative_minute_narrow_v1.rs.data");
include!("decimal_percent_v1.rs.data");
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
        #[clippy::msrv = "1.88"]
        impl $name {
            #[allow(dead_code)]
            pub(crate) const MUST_USE_MAKE_PROVIDER_MACRO: () = ();
        }
        icu_provider::marker::impl_data_provider_never_marker!($name);
    };
}
#[doc(inline)]
pub use __make_provider as make_provider;
/// This macro requires the following crates:
/// * `alloc`
/// * `icu`
/// * `icu_provider`
/// * `icu_provider/baked`
/// * `zerovec`
#[allow(unused_macros)]
macro_rules! impl_data_provider {
    ($ provider : ty) => {
        make_provider!($provider);
        impl_datetime_relative_hour_long_v1!($provider);
        impl_person_names_format_v1!($provider);
        impl_units_duration_digital_v1!($provider);
        impl_datetime_relative_quarter_long_v1!($provider);
        impl_units_names_mass_outlier_v1!($provider);
        impl_units_id_v1!($provider);
        impl_datetime_relative_quarter_short_v1!($provider);
        impl_datetime_relative_week_long_v1!($provider);
        impl_datetime_relative_minute_long_v1!($provider);
        impl_units_names_duration_extended_v1!($provider);
        impl_locale_names_region_v0!($provider);
        impl_datetime_relative_second_short_v1!($provider);
        impl_datetime_relative_month_narrow_v1!($provider);
        impl_datetime_relative_month_long_v1!($provider);
        impl_currency_displayname_v1!($provider);
        impl_currency_patterns_data_v1!($provider);
        impl_datetime_relative_day_long_v1!($provider);
        impl_datetime_relative_second_narrow_v1!($provider);
        impl_currency_extended_data_v1!($provider);
        impl_datetime_relative_week_narrow_v1!($provider);
        impl_units_names_volume_extended_v1!($provider);
        impl_datetime_relative_quarter_narrow_v1!($provider);
        impl_datetime_relative_hour_narrow_v1!($provider);
        impl_datetime_relative_year_long_v1!($provider);
        impl_datetime_relative_minute_short_v1!($provider);
        impl_currency_patterns_no_currency_v1!($provider);
        impl_datetime_relative_hour_short_v1!($provider);
        impl_units_names_area_extended_v1!($provider);
        impl_units_names_length_core_v1!($provider);
        impl_datetime_relative_week_short_v1!($provider);
        impl_units_names_area_core_v1!($provider);
        impl_units_essentials_v1!($provider);
        impl_units_names_volume_outlier_v1!($provider);
        impl_datetime_relative_day_narrow_v1!($provider);
        impl_units_names_mass_extended_v1!($provider);
        impl_units_names_area_outlier_v1!($provider);
        impl_datetime_relative_day_short_v1!($provider);
        impl_units_names_mass_core_v1!($provider);
        impl_units_names_length_extended_v1!($provider);
        impl_locale_names_language_v0!($provider);
        impl_currency_essentials_v1!($provider);
        impl_locale_names_script_v0!($provider);
        impl_currency_decimal_symbols_v1!($provider);
        impl_units_names_length_outlier_v1!($provider);
        impl_datetime_relative_month_short_v1!($provider);
        impl_locale_names_locale_v0!($provider);
        impl_datetime_relative_second_long_v1!($provider);
        impl_locale_names_variant_v0!($provider);
        impl_transliterator_rules_v1!($provider);
        impl_units_names_duration_outlier_v1!($provider);
        impl_datetime_relative_year_narrow_v1!($provider);
        impl_units_names_volume_core_v1!($provider);
        impl_currency_symbols_v1!($provider);
        impl_datetime_relative_year_short_v1!($provider);
        impl_units_names_duration_core_v1!($provider);
        impl_currency_fractions_v1!($provider);
        impl_units_info_v1!($provider);
        impl_datetime_relative_minute_narrow_v1!($provider);
        impl_decimal_percent_v1!($provider);
    };
}
