// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::{boxed::Box, collections::BTreeSet};
use core::marker::PhantomData;
use yoke::Yokeable;

use crate::prelude::*;

/// A data provider that loads data for a specific [`DataMarkerInfo`].
pub trait DataProvider<M>
where
    M: DataMarker,
{
    /// Query the provider for data, returning the result.
    ///
    /// Returns [`Ok`] if the request successfully loaded data. If data failed to load, returns an
    /// Error with more information.
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError>;
}

impl<M, P> DataProvider<M> for &P
where
    M: DataMarker,
    P: DataProvider<M> + ?Sized,
{
    #[inline]
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        (*self).load(req)
    }
}

impl<M, P> DataProvider<M> for Box<P>
where
    M: DataMarker,
    P: DataProvider<M> + ?Sized,
{
    #[inline]
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        (**self).load(req)
    }
}

impl<M, P> DataProvider<M> for alloc::rc::Rc<P>
where
    M: DataMarker,
    P: DataProvider<M> + ?Sized,
{
    #[inline]
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        (**self).load(req)
    }
}

/// A [`DataProvider`] that can iterate over all supported [`DataIdentifierCow`]s.
///
/// The provider is not allowed to return `Ok` for requests that were not returned by `iter_ids`,
/// and must not fail with a [`DataErrorKind::IdentifierNotFound`] for requests that were returned.
pub trait IterableDataProvider<M: DataMarker>: DataProvider<M> {
    /// Returns a set of [`DataIdentifierCow`].
    fn iter_ids(&self) -> Result<BTreeSet<DataIdentifierCow>, DataError>;
}

#[cfg(target_has_atomic = "ptr")]
impl<M, P> DataProvider<M> for alloc::sync::Arc<P>
where
    M: DataMarker,
    P: DataProvider<M> + ?Sized,
{
    #[inline]
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        (**self).load(req)
    }
}

/// A data provider that can determine whether it can load a particular data identifier,
/// potentially cheaper than actually performing the load.
pub trait DryDataProvider<M: DataMarker>: DataProvider<M> {
    /// This method goes through the motions of [`load`], but only returns the metadata.
    ///
    /// If `dry_load` returns an error, [`load`] must return the same error, but
    /// not vice-versa. Concretely, [`load`] could return deserialization or I/O errors
    /// that `dry_load` cannot predict.
    ///
    /// [`load`]: DataProvider::load
    fn dry_load(&self, req: DataRequest) -> Result<DataResponseMetadata, DataError>;
}

/// A data provider that loads data for a specific data type.
///
/// Unlike [`DataProvider`], there may be multiple markers corresponding to the same data type.
/// This is often the case when returning `dyn` trait objects such as [`AnyMarker`].
///
/// [`AnyMarker`]: crate::any::AnyMarker
pub trait DynamicDataProvider<M>
where
    M: DynamicDataMarker,
{
    /// Query the provider for data, returning the result.
    ///
    /// Returns [`Ok`] if the request successfully loaded data. If data failed to load, returns an
    /// Error with more information.
    fn load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponse<M>, DataError>;
}

/// A dynanmic data provider that can determine whether it can load a particular data identifier,
/// potentially cheaper than actually performing the load.
pub trait DynamicDryDataProvider<M: DynamicDataMarker>: DynamicDataProvider<M> {
    /// This method goes through the motions of [`load_data`], but only returns the metadata.
    ///
    /// If `dry_load_data` returns an error, [`load_data`] must return the same error, but
    /// not vice-versa. Concretely, [`load_data`] could return deserialization or I/O errors
    /// that `dry_load_data` cannot predict.
    ///
    /// [`load_data`]: DynamicDataProvider::load_data
    fn dry_load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponseMetadata, DataError>;
}

impl<M, P> DynamicDataProvider<M> for &P
where
    M: DynamicDataMarker,
    P: DynamicDataProvider<M> + ?Sized,
{
    #[inline]
    fn load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponse<M>, DataError> {
        (*self).load_data(marker, req)
    }
}

impl<M, P> DynamicDataProvider<M> for Box<P>
where
    M: DynamicDataMarker,
    P: DynamicDataProvider<M> + ?Sized,
{
    #[inline]
    fn load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponse<M>, DataError> {
        (**self).load_data(marker, req)
    }
}

impl<M, P> DynamicDataProvider<M> for alloc::rc::Rc<P>
where
    M: DynamicDataMarker,
    P: DynamicDataProvider<M> + ?Sized,
{
    #[inline]
    fn load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponse<M>, DataError> {
        (**self).load_data(marker, req)
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<M, P> DynamicDataProvider<M> for alloc::sync::Arc<P>
where
    M: DynamicDataMarker,
    P: DynamicDataProvider<M> + ?Sized,
{
    #[inline]
    fn load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponse<M>, DataError> {
        (**self).load_data(marker, req)
    }
}

/// A [`DynamicDataProvider`] that can iterate over all supported [`DataIdentifierCow`]s for a certain marker.
///
/// The provider is not allowed to return `Ok` for requests that were not returned by `iter_ids`,
/// and must not fail with a [`DataErrorKind::IdentifierNotFound`] for requests that were returned.
pub trait IterableDynamicDataProvider<M: DynamicDataMarker>: DynamicDataProvider<M> {
    /// Given a [`DataMarkerInfo`], returns a set of [`DataIdentifierCow`].
    fn iter_ids_for_marker(
        &self,
        marker: DataMarkerInfo,
    ) -> Result<BTreeSet<DataIdentifierCow>, DataError>;
}

impl<M, P> IterableDynamicDataProvider<M> for Box<P>
where
    M: DynamicDataMarker,
    P: IterableDynamicDataProvider<M> + ?Sized,
{
    fn iter_ids_for_marker(
        &self,
        marker: DataMarkerInfo,
    ) -> Result<BTreeSet<DataIdentifierCow>, DataError> {
        (**self).iter_ids_for_marker(marker)
    }
}

/// A data provider that loads data for a specific data type.
///
/// Unlike [`DataProvider`], the provider is bound to a specific marker ahead of time.
///
/// This crate provides [`DataProviderWithMarker`] which implements this trait on a single provider
/// with a single marker. However, this trait can also be implemented on providers that fork between
/// multiple markers that all return the same data type. For example, it can abstract over many
/// calendar systems in the datetime formatter.
///
/// [`AnyMarker`]: crate::any::AnyMarker
pub trait BoundDataProvider<M>
where
    M: DynamicDataMarker,
{
    /// Query the provider for data, returning the result.
    ///
    /// Returns [`Ok`] if the request successfully loaded data. If data failed to load, returns an
    /// Error with more information.
    fn load_bound(&self, req: DataRequest) -> Result<DataResponse<M>, DataError>;
    /// Returns the [`DataMarkerInfo`] that this provider uses for loading data.
    fn bound_marker(&self) -> DataMarkerInfo;
}

impl<M, P> BoundDataProvider<M> for &P
where
    M: DynamicDataMarker,
    P: BoundDataProvider<M> + ?Sized,
{
    #[inline]
    fn load_bound(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        (*self).load_bound(req)
    }
    #[inline]
    fn bound_marker(&self) -> DataMarkerInfo {
        (*self).bound_marker()
    }
}

impl<M, P> BoundDataProvider<M> for Box<P>
where
    M: DynamicDataMarker,
    P: BoundDataProvider<M> + ?Sized,
{
    #[inline]
    fn load_bound(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        (**self).load_bound(req)
    }
    #[inline]
    fn bound_marker(&self) -> DataMarkerInfo {
        (**self).bound_marker()
    }
}

impl<M, P> BoundDataProvider<M> for alloc::rc::Rc<P>
where
    M: DynamicDataMarker,
    P: BoundDataProvider<M> + ?Sized,
{
    #[inline]
    fn load_bound(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        (**self).load_bound(req)
    }
    #[inline]
    fn bound_marker(&self) -> DataMarkerInfo {
        (**self).bound_marker()
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<M, P> BoundDataProvider<M> for alloc::sync::Arc<P>
where
    M: DynamicDataMarker,
    P: BoundDataProvider<M> + ?Sized,
{
    #[inline]
    fn load_bound(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        (**self).load_bound(req)
    }
    #[inline]
    fn bound_marker(&self) -> DataMarkerInfo {
        (**self).bound_marker()
    }
}

/// A [`DataProvider`] associated with a specific marker.
///
/// Implements [`BoundDataProvider`].
#[derive(Debug)]
pub struct DataProviderWithMarker<M, P> {
    inner: P,
    _marker: PhantomData<M>,
}

impl<M, P> DataProviderWithMarker<M, P>
where
    M: DataMarker,
    P: DataProvider<M>,
{
    /// Creates a [`DataProviderWithMarker`] from a [`DataProvider`] with a [`DataMarker`].
    pub const fn new(inner: P) -> Self {
        Self {
            inner,
            _marker: PhantomData,
        }
    }
}

impl<M, M0, Y, P> BoundDataProvider<M0> for DataProviderWithMarker<M, P>
where
    M: DataMarker<DataStruct = Y>,
    M0: DynamicDataMarker<DataStruct = Y>,
    Y: for<'a> Yokeable<'a>,
    P: DataProvider<M>,
{
    #[inline]
    fn load_bound(&self, req: DataRequest) -> Result<DataResponse<M0>, DataError> {
        self.inner.load(req).map(DataResponse::cast)
    }
    #[inline]
    fn bound_marker(&self) -> DataMarkerInfo {
        M::INFO
    }
}

#[cfg(test)]
mod test {

    use super::*;
    use crate::hello_world::*;
    use alloc::borrow::Cow;
    use alloc::string::String;
    use core::fmt::Debug;
    use serde::{Deserialize, Serialize};

    // This tests DataProvider borrow semantics with a dummy data provider based on a
    // JSON string. It also exercises most of the data provider code paths.

    /// Key for HelloAlt, used for testing mismatched types
    const HELLO_ALT_KEY: DataMarkerInfo =
        DataMarkerInfo::from_path(crate::marker::data_marker_path!("core/helloalt1@1"));

    /// A data struct serialization-compatible with HelloWorldV1 used for testing mismatched types
    #[derive(
        Serialize, Deserialize, Debug, Clone, Default, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom,
    )]
    struct HelloAlt {
        #[zerofrom(clone)]
        message: String,
    }

    /// Marker type for [`HelloAlt`].
    struct HelloAltMarker {}

    impl DynamicDataMarker for HelloAltMarker {
        type DataStruct = HelloAlt;
    }

    impl DataMarker for HelloAltMarker {
        const INFO: DataMarkerInfo = HELLO_ALT_KEY;
    }

    #[derive(Deserialize, Debug, Clone, Default, PartialEq)]
    struct HelloCombined<'data> {
        #[serde(borrow)]
        pub hello_v1: HelloWorldV1<'data>,
        pub hello_alt: HelloAlt,
    }

    /// A DataProvider that owns its data, returning an Rc-variant DataPayload.
    /// Supports only key::HELLO_WORLD_V1. Uses `impl_dynamic_data_provider!()`.
    #[derive(Debug)]
    struct DataWarehouse {
        hello_v1: HelloWorldV1<'static>,
        hello_alt: HelloAlt,
    }

    impl DataProvider<HelloWorldV1Marker> for DataWarehouse {
        fn load(&self, _: DataRequest) -> Result<DataResponse<HelloWorldV1Marker>, DataError> {
            Ok(DataResponse {
                metadata: DataResponseMetadata::default(),
                payload: DataPayload::from_owned(self.hello_v1.clone()),
            })
        }
    }

    crate::dynutil::impl_dynamic_data_provider!(DataWarehouse, [HelloWorldV1Marker,], AnyMarker);

    /// A DataProvider that supports both key::HELLO_WORLD_V1 and HELLO_ALT.
    #[derive(Debug)]
    struct DataProvider2 {
        data: DataWarehouse,
    }

    impl From<DataWarehouse> for DataProvider2 {
        fn from(warehouse: DataWarehouse) -> Self {
            DataProvider2 { data: warehouse }
        }
    }

    impl DataProvider<HelloWorldV1Marker> for DataProvider2 {
        fn load(&self, _: DataRequest) -> Result<DataResponse<HelloWorldV1Marker>, DataError> {
            Ok(DataResponse {
                metadata: DataResponseMetadata::default(),
                payload: DataPayload::from_owned(self.data.hello_v1.clone()),
            })
        }
    }

    impl DataProvider<HelloAltMarker> for DataProvider2 {
        fn load(&self, _: DataRequest) -> Result<DataResponse<HelloAltMarker>, DataError> {
            Ok(DataResponse {
                metadata: DataResponseMetadata::default(),
                payload: DataPayload::from_owned(self.data.hello_alt.clone()),
            })
        }
    }

    crate::dynutil::impl_dynamic_data_provider!(
        DataProvider2,
        [HelloWorldV1Marker, HelloAltMarker,],
        AnyMarker
    );

    const DATA: &str = r#"{
        "hello_v1": {
            "message": "Hello V1"
        },
        "hello_alt": {
            "message": "Hello Alt"
        }
    }"#;

    fn get_warehouse(data: &'static str) -> DataWarehouse {
        let data: HelloCombined = serde_json::from_str(data).expect("Well-formed data");
        DataWarehouse {
            hello_v1: data.hello_v1,
            hello_alt: data.hello_alt,
        }
    }

    fn get_payload_v1<P: DataProvider<HelloWorldV1Marker> + ?Sized>(
        provider: &P,
    ) -> Result<DataPayload<HelloWorldV1Marker>, DataError> {
        provider.load(Default::default()).map(|r| r.payload)
    }

    fn get_payload_alt<P: DataProvider<HelloAltMarker> + ?Sized>(
        provider: &P,
    ) -> Result<DataPayload<HelloAltMarker>, DataError> {
        provider.load(Default::default()).map(|r| r.payload)
    }

    #[test]
    fn test_warehouse_owned() {
        let warehouse = get_warehouse(DATA);
        let hello_data = get_payload_v1(&warehouse).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_warehouse_owned_dyn_erased() {
        use crate::any::*;
        let warehouse = get_warehouse(DATA);
        let hello_data = get_payload_v1(&warehouse.as_any_provider().as_downcasting()).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_warehouse_owned_dyn_generic() {
        let warehouse = get_warehouse(DATA);
        let hello_data =
            get_payload_v1(&warehouse as &dyn DataProvider<HelloWorldV1Marker>).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_warehouse_owned_dyn_erased_alt() {
        let warehouse = get_warehouse(DATA);
        let response = get_payload_alt(&warehouse.as_any_provider().as_downcasting());
        assert!(matches!(
            response,
            Err(DataError {
                kind: DataErrorKind::MarkerNotFound,
                ..
            })
        ));
    }

    #[test]
    fn test_provider2() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        let hello_data = get_payload_v1(&provider).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_provider2_dyn_erased() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        let hello_data = get_payload_v1(&provider.as_any_provider().as_downcasting()).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_provider2_dyn_erased_alt() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        let hello_data = get_payload_alt(&provider.as_any_provider().as_downcasting()).unwrap();
        assert!(matches!(hello_data.get(), HelloAlt { .. }));
    }

    #[test]
    fn test_provider2_dyn_generic() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        let hello_data =
            get_payload_v1(&provider as &dyn DataProvider<HelloWorldV1Marker>).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_provider2_dyn_generic_alt() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        let hello_data = get_payload_alt(&provider as &dyn DataProvider<HelloAltMarker>).unwrap();
        assert!(matches!(hello_data.get(), HelloAlt { .. }));
    }

    #[test]
    fn test_mismatched_types() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        // Request is for v2, but type argument is for v1
        let response: Result<DataResponse<HelloWorldV1Marker>, DataError> = AnyProvider::load_any(
            &provider.as_any_provider(),
            HELLO_ALT_KEY,
            Default::default(),
        )
        .unwrap()
        .downcast();
        assert!(matches!(
            response,
            Err(DataError {
                kind: DataErrorKind::Downcast(_),
                ..
            })
        ));
    }

    fn check_v1_v2<P>(d: &P)
    where
        P: DataProvider<HelloWorldV1Marker> + DataProvider<HelloAltMarker> + ?Sized,
    {
        let v1: DataPayload<HelloWorldV1Marker> = d.load(Default::default()).unwrap().payload;
        let v2: DataPayload<HelloAltMarker> = d.load(Default::default()).unwrap().payload;
        if v1.get().message == v2.get().message {
            panic!()
        }
    }

    #[test]
    fn test_v1_v2_generic() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        check_v1_v2(&provider);
    }

    #[test]
    fn test_v1_v2_dyn_erased() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        check_v1_v2(&provider.as_any_provider().as_downcasting());
    }
}
