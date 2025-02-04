// @generated
include!("chinese_cache_v1_marker.rs.data");
include!("dangi_cache_v1_marker.rs.data");
include!("islamic_observational_cache_v1_marker.rs.data");
include!("islamic_umm_al_qura_cache_v1_marker.rs.data");
include!("japanese_eras_v1_marker.rs.data");
include!("japanese_extended_eras_v1_marker.rs.data");
include!("week_data_v2_marker.rs.data");
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
        impl_chinese_cache_v1_marker!($provider);
        impl_dangi_cache_v1_marker!($provider);
        impl_islamic_observational_cache_v1_marker!($provider);
        impl_islamic_umm_al_qura_cache_v1_marker!($provider);
        impl_japanese_eras_v1_marker!($provider);
        impl_japanese_extended_eras_v1_marker!($provider);
        impl_week_data_v2_marker!($provider);
    };
}
#[allow(unused_macros)]
macro_rules! impl_any_provider {
    ($ provider : ty) => {
        #[clippy::msrv = "1.71.1"]
        impl icu_provider::any::AnyProvider for $provider {
            fn load_any(&self, marker: icu_provider::DataMarkerInfo, req: icu_provider::DataRequest) -> Result<icu_provider::AnyResponse, icu_provider::DataError> {
                match marker.path.hashed() {
                    h if h == <icu::calendar::provider::ChineseCacheV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::calendar::provider::ChineseCacheV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::calendar::provider::DangiCacheV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::calendar::provider::DangiCacheV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::calendar::provider::IslamicObservationalCacheV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::calendar::provider::IslamicObservationalCacheV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::calendar::provider::IslamicUmmAlQuraCacheV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::calendar::provider::IslamicUmmAlQuraCacheV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::calendar::provider::JapaneseErasV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::calendar::provider::JapaneseErasV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::calendar::provider::JapaneseExtendedErasV1Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::calendar::provider::JapaneseExtendedErasV1Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    h if h == <icu::calendar::provider::WeekDataV2Marker as icu_provider::DataMarker>::INFO.path.hashed() => icu_provider::DataProvider::<icu::calendar::provider::WeekDataV2Marker>::load(self, req).map(icu_provider::DataResponse::wrap_into_any_response),
                    _ => Err(icu_provider::DataErrorKind::MarkerNotFound.with_req(marker, req)),
                }
            }
        }
    };
}
