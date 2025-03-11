// @generated
include!("metazone_generic_names_long_v1.rs.data");
include!("metazone_specific_names_short_v1.rs.data");
include!("coptic_month_names_v1.rs.data");
include!("exemplar_cities_v1.rs.data");
include!("coptic_year_names_v1.rs.data");
include!("roc_year_names_v1.rs.data");
include!("hebrew_date_neo_skeleton_patterns_v1.rs.data");
include!("islamic_month_names_v1.rs.data");
include!("chinese_date_neo_skeleton_patterns_v1.rs.data");
include!("indian_month_names_v1.rs.data");
include!("ethiopian_month_names_v1.rs.data");
include!("roc_month_names_v1.rs.data");
include!("roc_date_neo_skeleton_patterns_v1.rs.data");
include!("japanese_extended_year_names_v1.rs.data");
include!("ethiopian_year_names_v1.rs.data");
include!("persian_year_names_v1.rs.data");
include!("coptic_date_neo_skeleton_patterns_v1.rs.data");
include!("weekday_names_v1.rs.data");
include!("dangi_month_names_v1.rs.data");
include!("buddhist_date_neo_skeleton_patterns_v1.rs.data");
include!("glue_pattern_v1.rs.data");
include!("metazone_period_v1.rs.data");
include!("japanese_date_neo_skeleton_patterns_v1.rs.data");
include!("buddhist_month_names_v1.rs.data");
include!("islamic_date_neo_skeleton_patterns_v1.rs.data");
include!("dangi_date_neo_skeleton_patterns_v1.rs.data");
include!("metazone_specific_names_long_v1.rs.data");
include!("metazone_generic_names_short_v1.rs.data");
include!("gregorian_month_names_v1.rs.data");
include!("hebrew_year_names_v1.rs.data");
include!("japanese_extended_date_neo_skeleton_patterns_v1.rs.data");
include!("persian_date_neo_skeleton_patterns_v1.rs.data");
include!("japanese_year_names_v1.rs.data");
include!("gregorian_year_names_v1.rs.data");
include!("locations_root_v1.rs.data");
include!("ethiopian_date_neo_skeleton_patterns_v1.rs.data");
include!("exemplar_cities_root_v1.rs.data");
include!("buddhist_year_names_v1.rs.data");
include!("gregorian_date_neo_skeleton_patterns_v1.rs.data");
include!("chinese_year_names_v1.rs.data");
include!("hebrew_month_names_v1.rs.data");
include!("japanese_month_names_v1.rs.data");
include!("persian_month_names_v1.rs.data");
include!("time_zone_essentials_v1.rs.data");
include!("locations_v1.rs.data");
include!("islamic_year_names_v1.rs.data");
include!("metazone_standard_names_long_v1.rs.data");
include!("time_neo_skeleton_patterns_v1.rs.data");
include!("day_period_names_v1.rs.data");
include!("chinese_month_names_v1.rs.data");
include!("dangi_year_names_v1.rs.data");
include!("indian_year_names_v1.rs.data");
include!("indian_date_neo_skeleton_patterns_v1.rs.data");
include!("japanese_extended_month_names_v1.rs.data");
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
        #[clippy::msrv = "1.81"]
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
        impl_metazone_generic_names_long_v1!($provider);
        impl_metazone_specific_names_short_v1!($provider);
        impl_coptic_month_names_v1!($provider);
        impl_exemplar_cities_v1!($provider);
        impl_coptic_year_names_v1!($provider);
        impl_roc_year_names_v1!($provider);
        impl_hebrew_date_neo_skeleton_patterns_v1!($provider);
        impl_islamic_month_names_v1!($provider);
        impl_chinese_date_neo_skeleton_patterns_v1!($provider);
        impl_indian_month_names_v1!($provider);
        impl_ethiopian_month_names_v1!($provider);
        impl_roc_month_names_v1!($provider);
        impl_roc_date_neo_skeleton_patterns_v1!($provider);
        impl_japanese_extended_year_names_v1!($provider);
        impl_ethiopian_year_names_v1!($provider);
        impl_persian_year_names_v1!($provider);
        impl_coptic_date_neo_skeleton_patterns_v1!($provider);
        impl_weekday_names_v1!($provider);
        impl_dangi_month_names_v1!($provider);
        impl_buddhist_date_neo_skeleton_patterns_v1!($provider);
        impl_glue_pattern_v1!($provider);
        impl_metazone_period_v1!($provider);
        impl_japanese_date_neo_skeleton_patterns_v1!($provider);
        impl_buddhist_month_names_v1!($provider);
        impl_islamic_date_neo_skeleton_patterns_v1!($provider);
        impl_dangi_date_neo_skeleton_patterns_v1!($provider);
        impl_metazone_specific_names_long_v1!($provider);
        impl_metazone_generic_names_short_v1!($provider);
        impl_gregorian_month_names_v1!($provider);
        impl_hebrew_year_names_v1!($provider);
        impl_japanese_extended_date_neo_skeleton_patterns_v1!($provider);
        impl_persian_date_neo_skeleton_patterns_v1!($provider);
        impl_japanese_year_names_v1!($provider);
        impl_gregorian_year_names_v1!($provider);
        impl_locations_root_v1!($provider);
        impl_ethiopian_date_neo_skeleton_patterns_v1!($provider);
        impl_exemplar_cities_root_v1!($provider);
        impl_buddhist_year_names_v1!($provider);
        impl_gregorian_date_neo_skeleton_patterns_v1!($provider);
        impl_chinese_year_names_v1!($provider);
        impl_hebrew_month_names_v1!($provider);
        impl_japanese_month_names_v1!($provider);
        impl_persian_month_names_v1!($provider);
        impl_time_zone_essentials_v1!($provider);
        impl_locations_v1!($provider);
        impl_islamic_year_names_v1!($provider);
        impl_metazone_standard_names_long_v1!($provider);
        impl_time_neo_skeleton_patterns_v1!($provider);
        impl_day_period_names_v1!($provider);
        impl_chinese_month_names_v1!($provider);
        impl_dangi_year_names_v1!($provider);
        impl_indian_year_names_v1!($provider);
        impl_indian_date_neo_skeleton_patterns_v1!($provider);
        impl_japanese_extended_month_names_v1!($provider);
    };
}
