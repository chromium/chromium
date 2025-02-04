// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::data_struct_impl;
use proc_macro2::TokenStream as TokenStream2;
use quote::quote;

fn check(attr: TokenStream2, item: TokenStream2, expected: TokenStream2) {
    let actual = data_struct_impl(syn::parse2(attr).unwrap(), syn::parse2(item).unwrap());
    assert_eq!(expected.to_string(), actual.to_string());
}

#[test]
#[rustfmt::skip] // inserts a comma
fn test_basic() {
    // #[data_struct]
    check(
        quote!(),
        quote!(
            pub struct FooV1;
        ),
        quote!(
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            pub struct FooV1;
        ),
    );
}

#[test]
fn test_dyn_data_marker() {
    // #[data_struct(FooV1Marker)]
    check(
        quote!(FooV1Marker),
        quote!(
            pub struct FooV1;
        ),
        quote!(
            #[doc = "Marker type for [`FooV1`]"]
            pub struct FooV1Marker;
            impl icu_provider::DynamicDataMarker for FooV1Marker {
                type DataStruct = FooV1;
            }
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            pub struct FooV1;
        ),
    );
}

#[test]
fn test_data_marker() {
    // #[data_struct(BarV1Marker = "demo/bar@1")]
    check(
        quote!(BarV1Marker = "demo/bar@1"),
        quote!(
            pub struct FooV1;
        ),
        quote!(
            #[doc = "Marker type for [`FooV1`]: \"demo/bar@1\"\n\n- Fallback priority: language (default)"]
            pub struct BarV1Marker;
            impl icu_provider::DynamicDataMarker for BarV1Marker {
                type DataStruct = FooV1;
            }
            impl icu_provider::DataMarker for BarV1Marker {
                const INFO: icu_provider::DataMarkerInfo = {
                    let mut info = icu_provider::DataMarkerInfo::from_path(icu_provider::marker::data_marker_path!("demo/bar@1"));
                    info.is_singleton = false;
                    info.fallback_config.priority = icu_provider::fallback::LocaleFallbackPriority::default();
                    info
                };
            }
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            pub struct FooV1;
        ),
    );
}

#[test]
fn test_multi_named_keyed_data_marker() {
    // #[data_struct(FooV1Marker, BarV1Marker = "demo/bar@1", BazV1Marker = "demo/baz@1")]
    check(
        quote![
            FooV1Marker,
            BarV1Marker = "demo/bar@1",
            BazV1Marker = "demo/baz@1",
        ],
        quote!(
            pub struct FooV1<'data>;
        ),
        quote!(
            #[doc = "Marker type for [`FooV1`]"]
            pub struct FooV1Marker;
            impl icu_provider::DynamicDataMarker for FooV1Marker {
                type DataStruct = FooV1<'static>;
            }
            #[doc = "Marker type for [`FooV1`]: \"demo/bar@1\"\n\n- Fallback priority: language (default)"]
            pub struct BarV1Marker;
            impl icu_provider::DynamicDataMarker for BarV1Marker {
                type DataStruct = FooV1<'static>;
            }
            impl icu_provider::DataMarker for BarV1Marker {
                const INFO: icu_provider::DataMarkerInfo = {
                    let mut info = icu_provider::DataMarkerInfo::from_path(icu_provider::marker::data_marker_path!("demo/bar@1"));
                    info.is_singleton = false;
                    info.fallback_config.priority = icu_provider::fallback::LocaleFallbackPriority::default();
                    info
                };
            }
            #[doc = "Marker type for [`FooV1`]: \"demo/baz@1\"\n\n- Fallback priority: language (default)"]
            pub struct BazV1Marker;
            impl icu_provider::DynamicDataMarker for BazV1Marker {
                type DataStruct = FooV1<'static>;
            }
            impl icu_provider::DataMarker for BazV1Marker {
                const INFO: icu_provider::DataMarkerInfo =  {
                    let mut info = icu_provider::DataMarkerInfo::from_path(icu_provider::marker::data_marker_path!("demo/baz@1"));
                    info.is_singleton = false;
                    info.fallback_config.priority = icu_provider::fallback::LocaleFallbackPriority::default();
                    info
                };
            }
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            pub struct FooV1<'data>;
        ),
    );
}

#[test]
fn test_attributes() {
    // #[data_struct(FooV1Marker, marker(BarV1Marker, "demo/bar@1", fallback_by = "region", extension_kw = "ca"))]
    check(
        quote![
            FooV1Marker,
            marker(BarV1Marker, "demo/bar@1", fallback_by = "region", singleton),
        ],
        quote!(
            pub struct FooV1<'data>;
        ),
        quote!(
            #[doc = "Marker type for [`FooV1`]"]
            pub struct FooV1Marker;
            impl icu_provider::DynamicDataMarker for FooV1Marker {
                type DataStruct = FooV1<'static>;
            }
            #[doc = "Marker type for [`FooV1`]: \"demo/bar@1\"\n\n- Fallback priority: region"]
            pub struct BarV1Marker;
            impl icu_provider::DynamicDataMarker for BarV1Marker {
                type DataStruct = FooV1<'static>;
            }
            impl icu_provider::DataMarker for BarV1Marker {
                const INFO: icu_provider::DataMarkerInfo = {
                    let mut info = icu_provider::DataMarkerInfo::from_path(icu_provider::marker::data_marker_path!("demo/bar@1"));
                    info.is_singleton = true;
                    info.fallback_config.priority = icu_provider::fallback::LocaleFallbackPriority::Region;
                    info
                };
            }
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            pub struct FooV1<'data>;
        ),
    );
}
