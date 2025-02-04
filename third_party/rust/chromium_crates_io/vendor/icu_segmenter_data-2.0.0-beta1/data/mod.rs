// @generated
include!("dictionary_for_word_only_auto_v1_marker.rs.data");
include!("dictionary_for_word_line_extended_v1_marker.rs.data");
include!("grapheme_cluster_break_data_v2_marker.rs.data");
include!("line_break_data_v2_marker.rs.data");
include!("lstm_for_word_line_auto_v1_marker.rs.data");
include!("sentence_break_data_override_v1_marker.rs.data");
include!("sentence_break_data_v2_marker.rs.data");
include!("word_break_data_override_v1_marker.rs.data");
include!("word_break_data_v2_marker.rs.data");
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
        impl_dictionary_for_word_only_auto_v1_marker!($provider);
        impl_dictionary_for_word_line_extended_v1_marker!($provider);
        impl_grapheme_cluster_break_data_v2_marker!($provider);
        impl_line_break_data_v2_marker!($provider);
        impl_lstm_for_word_line_auto_v1_marker!($provider);
        impl_sentence_break_data_override_v1_marker!($provider);
        impl_sentence_break_data_v2_marker!($provider);
        impl_word_break_data_override_v1_marker!($provider);
        impl_word_break_data_v2_marker!($provider);
    };
}
#[allow(unused_macros)]
macro_rules! impl_any_provider {
    ($ provider : ty) => {
        #[clippy::msrv = "1.71.1"]
        impl icu_provider::any::AnyProvider for $provider {
            fn load_any(&self, marker: icu_provider::DataMarkerInfo, req: icu_provider::DataRequest) -> Result<icu_provider::AnyResponse, icu_provider::DataError> {
                match marker.path.hashed() {
                    h if h == <icu::segmenter::provider::DictionaryForWordOnlyAutoV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::segmenter::provider::DictionaryForWordOnlyAutoV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::segmenter::provider::DictionaryForWordLineExtendedV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::segmenter::provider::DictionaryForWordLineExtendedV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::segmenter::provider::GraphemeClusterBreakDataV2Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::segmenter::provider::GraphemeClusterBreakDataV2Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::segmenter::provider::LineBreakDataV2Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::segmenter::provider::LineBreakDataV2Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::segmenter::provider::LstmForWordLineAutoV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::segmenter::provider::LstmForWordLineAutoV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::segmenter::provider::SentenceBreakDataOverrideV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::segmenter::provider::SentenceBreakDataOverrideV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::segmenter::provider::SentenceBreakDataV2Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::segmenter::provider::SentenceBreakDataV2Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::segmenter::provider::WordBreakDataOverrideV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::segmenter::provider::WordBreakDataOverrideV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::segmenter::provider::WordBreakDataV2Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::segmenter::provider::WordBreakDataV2Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    _ => Err(icu_provider::DataErrorKind::MarkerNotFound.with_req(marker, req)),
                }
            }
        }
    };
}
