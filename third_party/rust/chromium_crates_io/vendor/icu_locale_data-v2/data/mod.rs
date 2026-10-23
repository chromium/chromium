// @generated
include!("locale_names_language_medium_tiny_v1.rs.data");
include!("locale_names_essentials_v1.rs.data");
include!("locale_names_script_medium_light_v1.rs.data");
include!("locale_names_language_menu_medium_heavy_v1.rs.data");
include!("locale_names_language_long_light_v1.rs.data");
include!("locale_exemplar_characters_main_v1.rs.data");
include!("locale_names_region_medium_tiny_v1.rs.data");
include!("locale_exemplar_characters_numbers_v1.rs.data");
include!("locale_aliases_v1.rs.data");
include!("locale_exemplar_characters_index_v1.rs.data");
include!("locale_names_script_short_heavy_v1.rs.data");
include!("locale_names_script_medium_tiny_v1.rs.data");
include!("locale_names_language_medium_light_v1.rs.data");
include!("locale_names_script_medium_heavy_v1.rs.data");
include!("locale_names_language_menu_medium_light_v1.rs.data");
include!("locale_names_language_medium_heavy_v1.rs.data");
include!("locale_names_variant_medium_heavy_v1.rs.data");
include!("locale_names_language_short_heavy_v1.rs.data");
include!("locale_exemplar_characters_auxiliary_v1.rs.data");
include!("locale_likely_subtags_extended_v1.rs.data");
include!("locale_names_region_medium_light_v1.rs.data");
include!("locale_names_language_long_heavy_v1.rs.data");
include!("locale_script_direction_v1.rs.data");
include!("locale_names_region_short_light_v1.rs.data");
include!("locale_names_language_short_light_v1.rs.data");
include!("locale_likely_subtags_script_region_v1.rs.data");
include!("locale_exemplar_characters_punctuation_v1.rs.data");
include!("locale_names_region_short_tiny_v1.rs.data");
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
/// * `icu`
/// * `icu_provider`
/// * `icu_provider/baked`
/// * `zerovec`
#[allow(unused_macros)]
macro_rules! impl_data_provider {
    ($ provider : ty) => {
        make_provider!($provider);
        impl_locale_names_language_medium_tiny_v1!($provider);
        impl_locale_names_essentials_v1!($provider);
        impl_locale_names_script_medium_light_v1!($provider);
        impl_locale_names_language_menu_medium_heavy_v1!($provider);
        impl_locale_names_language_long_light_v1!($provider);
        impl_locale_exemplar_characters_main_v1!($provider);
        impl_locale_names_region_medium_tiny_v1!($provider);
        impl_locale_exemplar_characters_numbers_v1!($provider);
        impl_locale_aliases_v1!($provider);
        impl_locale_exemplar_characters_index_v1!($provider);
        impl_locale_names_script_short_heavy_v1!($provider);
        impl_locale_names_script_medium_tiny_v1!($provider);
        impl_locale_names_language_medium_light_v1!($provider);
        impl_locale_names_script_medium_heavy_v1!($provider);
        impl_locale_names_language_menu_medium_light_v1!($provider);
        impl_locale_names_language_medium_heavy_v1!($provider);
        impl_locale_names_variant_medium_heavy_v1!($provider);
        impl_locale_names_language_short_heavy_v1!($provider);
        impl_locale_exemplar_characters_auxiliary_v1!($provider);
        impl_locale_likely_subtags_extended_v1!($provider);
        impl_locale_names_region_medium_light_v1!($provider);
        impl_locale_names_language_long_heavy_v1!($provider);
        impl_locale_script_direction_v1!($provider);
        impl_locale_names_region_short_light_v1!($provider);
        impl_locale_names_language_short_light_v1!($provider);
        impl_locale_likely_subtags_script_region_v1!($provider);
        impl_locale_exemplar_characters_punctuation_v1!($provider);
        impl_locale_names_region_short_tiny_v1!($provider);
    };
}
