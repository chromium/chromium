// @generated
include!("aliases_v2_marker.rs.data");
include!("exemplar_characters_auxiliary_v1_marker.rs.data");
include!("exemplar_characters_index_v1_marker.rs.data");
include!("exemplar_characters_main_v1_marker.rs.data");
include!("exemplar_characters_numbers_v1_marker.rs.data");
include!("exemplar_characters_punctuation_v1_marker.rs.data");
include!("likely_subtags_extended_v1_marker.rs.data");
include!("likely_subtags_for_language_v1_marker.rs.data");
include!("likely_subtags_for_script_region_v1_marker.rs.data");
include!("parents_v1_marker.rs.data");
include!("script_direction_v1_marker.rs.data");
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
        impl_aliases_v2_marker!($provider);
        impl_exemplar_characters_auxiliary_v1_marker!($provider);
        impl_exemplar_characters_index_v1_marker!($provider);
        impl_exemplar_characters_main_v1_marker!($provider);
        impl_exemplar_characters_numbers_v1_marker!($provider);
        impl_exemplar_characters_punctuation_v1_marker!($provider);
        impl_likely_subtags_extended_v1_marker!($provider);
        impl_likely_subtags_for_language_v1_marker!($provider);
        impl_likely_subtags_for_script_region_v1_marker!($provider);
        impl_parents_v1_marker!($provider);
        impl_script_direction_v1_marker!($provider);
    };
}
#[allow(unused_macros)]
macro_rules! impl_any_provider {
    ($ provider : ty) => {
        #[clippy::msrv = "1.71.1"]
        impl icu_provider::any::AnyProvider for $provider {
            fn load_any(&self, marker: icu_provider::DataMarkerInfo, req: icu_provider::DataRequest) -> Result<icu_provider::AnyResponse, icu_provider::DataError> {
                match marker.path.hashed() {
                    h if h == <icu::locale::provider::AliasesV2Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::AliasesV2Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::locale::provider::ExemplarCharactersAuxiliaryV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::ExemplarCharactersAuxiliaryV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::locale::provider::ExemplarCharactersIndexV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::ExemplarCharactersIndexV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::locale::provider::ExemplarCharactersMainV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::ExemplarCharactersMainV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::locale::provider::ExemplarCharactersNumbersV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::ExemplarCharactersNumbersV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::locale::provider::ExemplarCharactersPunctuationV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::ExemplarCharactersPunctuationV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::locale::provider::LikelySubtagsExtendedV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::LikelySubtagsExtendedV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::locale::provider::LikelySubtagsForLanguageV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::LikelySubtagsForLanguageV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::locale::provider::LikelySubtagsForScriptRegionV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::LikelySubtagsForScriptRegionV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::locale::provider::ParentsV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::ParentsV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::locale::provider::ScriptDirectionV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::locale::provider::ScriptDirectionV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    _ => Err(icu_provider::DataErrorKind::MarkerNotFound.with_req(marker, req)),
                }
            }
        }
    };
}
