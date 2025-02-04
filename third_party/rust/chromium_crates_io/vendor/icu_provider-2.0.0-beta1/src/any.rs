// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Traits for data providers that produce `Any` objects.

use crate::prelude::*;
use crate::response::DataPayloadInner;
use core::any::Any;
use yoke::trait_hack::YokeTraitHack;
use yoke::Yokeable;
use zerofrom::ZeroFrom;

#[cfg(not(feature = "sync"))]
use alloc::rc::Rc as SelectedRc;
#[cfg(feature = "sync")]
use alloc::sync::Arc as SelectedRc;

/// A trait that allows to specify `Send + Sync` bounds that are only required when
/// the `sync` Cargo feature is enabled. Without the Cargo feature, this is an empty bound.
#[cfg(feature = "sync")]
pub trait MaybeSendSync: Send + Sync {}
#[cfg(feature = "sync")]
impl<T: Send + Sync> MaybeSendSync for T {}

#[allow(missing_docs)] // docs generated with all features
#[cfg(not(feature = "sync"))]
pub trait MaybeSendSync {}
#[cfg(not(feature = "sync"))]
impl<T> MaybeSendSync for T {}

/// Representations of the `Any` trait object.
///
/// **Important Note:** The types enclosed by `StructRef` and `PayloadRc` are NOT the same!
/// The first refers to the struct itself, whereas the second refers to a `DataPayload`.
#[derive(Debug, Clone)]
enum AnyPayloadInner {
    /// A reference to `M::DataStruct`
    StructRef(&'static dyn Any),
    /// A boxed `DataPayload<M>`.
    ///
    /// Note: This needs to be reference counted, not a `Box`, so that `AnyPayload` is cloneable.
    /// If an `AnyPayload` is cloned, the actual cloning of the data is delayed until
    /// `downcast()` is invoked (at which point we have the concrete type).

    #[cfg(not(feature = "sync"))]
    PayloadRc(SelectedRc<dyn Any>),

    #[cfg(feature = "sync")]
    PayloadRc(SelectedRc<dyn Any + Send + Sync>),
}

/// A type-erased data payload.
///
/// The only useful method on this type is [`AnyPayload::downcast()`], which transforms this into
/// a normal `DataPayload` which you can subsequently access or mutate.
///
/// As with `DataPayload`, cloning is designed to be cheap.
#[derive(Debug, Clone, Yokeable)]
pub struct AnyPayload {
    inner: AnyPayloadInner,
    type_name: &'static str,
}

/// The [`DynamicDataMarker`] marker type for [`AnyPayload`].
#[allow(clippy::exhaustive_structs)] // marker type
#[derive(Debug)]
pub struct AnyMarker;

impl DynamicDataMarker for AnyMarker {
    type DataStruct = AnyPayload;
}

impl<M> crate::dynutil::UpcastDataPayload<M> for AnyMarker
where
    M: DynamicDataMarker,
    M::DataStruct: MaybeSendSync,
{
    #[inline]
    fn upcast(other: DataPayload<M>) -> DataPayload<AnyMarker> {
        DataPayload::from_owned(other.wrap_into_any_payload())
    }
}

impl AnyPayload {
    /// Transforms a type-erased `AnyPayload` into a concrete `DataPayload<M>`.
    ///
    /// Because it is expected that the call site knows the identity of the AnyPayload (e.g., from
    /// the data request), this function returns a `DataError` if the generic type does not match
    /// the type stored in the `AnyPayload`.
    pub fn downcast<M>(self) -> Result<DataPayload<M>, DataError>
    where
        M: DynamicDataMarker,
        // For the StructRef case:
        M::DataStruct: ZeroFrom<'static, M::DataStruct>,
        // For the PayloadRc case:
        M::DataStruct: MaybeSendSync,
        for<'a> YokeTraitHack<<M::DataStruct as Yokeable<'a>>::Output>: Clone,
    {
        use AnyPayloadInner::*;
        let type_name = self.type_name;
        match self.inner {
            StructRef(any_ref) => {
                let down_ref: &'static M::DataStruct = any_ref
                    .downcast_ref()
                    .ok_or_else(|| DataError::for_type::<M>().with_str_context(type_name))?;
                Ok(DataPayload::from_static_ref(down_ref))
            }
            PayloadRc(any_rc) => {
                let down_rc = any_rc
                    .downcast::<DataPayload<M>>()
                    .map_err(|_| DataError::for_type::<M>().with_str_context(type_name))?;
                Ok(SelectedRc::try_unwrap(down_rc).unwrap_or_else(|down_rc| (*down_rc).clone()))
            }
        }
    }

    /// Clones and then transforms a type-erased `AnyPayload` into a concrete `DataPayload<M>`.
    pub fn downcast_cloned<M>(&self) -> Result<DataPayload<M>, DataError>
    where
        M: DynamicDataMarker,
        // For the StructRef case:
        M::DataStruct: ZeroFrom<'static, M::DataStruct>,
        // For the PayloadRc case:
        M::DataStruct: MaybeSendSync,
        for<'a> YokeTraitHack<<M::DataStruct as Yokeable<'a>>::Output>: Clone,
    {
        self.clone().downcast()
    }

    /// Creates an `AnyPayload` from a static reference to a data struct.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_provider::hello_world::*;
    /// use icu_provider::prelude::*;
    /// use std::borrow::Cow;
    ///
    /// const HELLO_DATA: HelloWorldV1<'static> = HelloWorldV1 {
    ///     message: Cow::Borrowed("Custom Hello World"),
    /// };
    ///
    /// let any_payload = AnyPayload::from_static_ref(&HELLO_DATA);
    ///
    /// let payload: DataPayload<HelloWorldV1Marker> =
    ///     any_payload.downcast().expect("TypeId matches");
    /// assert_eq!("Custom Hello World", payload.get().message);
    /// ```
    pub fn from_static_ref<Y>(static_ref: &'static Y) -> Self
    where
        Y: for<'a> Yokeable<'a>,
    {
        AnyPayload {
            inner: AnyPayloadInner::StructRef(static_ref),
            // Note: This records the Yokeable type rather than the DataMarker type,
            // but that is okay since this is only for debugging
            type_name: core::any::type_name::<Y>(),
        }
    }
}

impl<M> DataPayload<M>
where
    M: DynamicDataMarker,
    M::DataStruct: MaybeSendSync,
{
    /// Converts this DataPayload into a type-erased `AnyPayload`. Unless the payload stores a static
    /// reference, this will move it to the heap.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_provider::hello_world::*;
    /// use icu_provider::prelude::*;
    /// use std::borrow::Cow;
    ///
    /// let payload: DataPayload<HelloWorldV1Marker> =
    ///     DataPayload::from_owned(HelloWorldV1 {
    ///         message: Cow::Borrowed("Custom Hello World"),
    ///     });
    ///
    /// let any_payload = payload.wrap_into_any_payload();
    ///
    /// let payload: DataPayload<HelloWorldV1Marker> =
    ///     any_payload.downcast().expect("TypeId matches");
    /// assert_eq!("Custom Hello World", payload.get().message);
    /// ```
    pub fn wrap_into_any_payload(self) -> AnyPayload {
        AnyPayload {
            inner: match self.0 {
                DataPayloadInner::StaticRef(r) => AnyPayloadInner::StructRef(r),
                inner => AnyPayloadInner::PayloadRc(SelectedRc::from(Self(inner))),
            },
            type_name: core::any::type_name::<M>(),
        }
    }
}

impl DataPayload<AnyMarker> {
    /// Transforms a type-erased `DataPayload<AnyMarker>` into a concrete `DataPayload<M>`.
    #[inline]
    pub fn downcast<M>(self) -> Result<DataPayload<M>, DataError>
    where
        M: DynamicDataMarker,
        for<'a> YokeTraitHack<<M::DataStruct as Yokeable<'a>>::Output>: Clone,
        M::DataStruct: ZeroFrom<'static, M::DataStruct>,
        M::DataStruct: MaybeSendSync,
    {
        match self.0 {
            DataPayloadInner::Yoke(yoke) => yoke.try_into_yokeable().ok(),
            DataPayloadInner::StaticRef(_) => None,
        }
        .ok_or_else(|| DataError::for_type::<M>().with_str_context("Payload not owned"))?
        .downcast()
    }
}

/// A [`DataResponse`] for type-erased values.
///
/// Convertible to and from `DataResponse<AnyMarker>`.
#[allow(clippy::exhaustive_structs)] // this type is stable (the metadata is allowed to grow)
#[derive(Debug)]
pub struct AnyResponse {
    /// Metadata about the returned object.
    pub metadata: DataResponseMetadata,

    /// The object itself
    pub payload: AnyPayload,
}

impl From<AnyResponse> for DataResponse<AnyMarker> {
    #[inline]
    fn from(other: AnyResponse) -> Self {
        Self {
            metadata: other.metadata,
            payload: DataPayload::from_owned(other.payload),
        }
    }
}

impl AnyResponse {
    /// Transforms a type-erased `AnyResponse` into a concrete `DataResponse<M>`.
    #[inline]
    pub fn downcast<M>(self) -> Result<DataResponse<M>, DataError>
    where
        M: DynamicDataMarker,
        for<'a> YokeTraitHack<<M::DataStruct as Yokeable<'a>>::Output>: Clone,
        M::DataStruct: ZeroFrom<'static, M::DataStruct>,
        M::DataStruct: MaybeSendSync,
    {
        Ok(DataResponse {
            metadata: self.metadata,
            payload: self.payload.downcast()?,
        })
    }

    /// Clones and then transforms a type-erased `AnyResponse` into a concrete `DataResponse<M>`.
    pub fn downcast_cloned<M>(&self) -> Result<DataResponse<M>, DataError>
    where
        M: DynamicDataMarker,
        M::DataStruct: ZeroFrom<'static, M::DataStruct>,
        M::DataStruct: MaybeSendSync,
        for<'a> YokeTraitHack<<M::DataStruct as Yokeable<'a>>::Output>: Clone,
    {
        Ok(DataResponse {
            metadata: self.metadata.clone(),
            payload: self.payload.downcast_cloned()?,
        })
    }
}

impl<M> DataResponse<M>
where
    M: DynamicDataMarker,
    M::DataStruct: MaybeSendSync,
{
    /// Moves the inner DataPayload to the heap (requiring an allocation) and returns it as an
    /// erased `AnyResponse`.
    pub fn wrap_into_any_response(self) -> AnyResponse {
        AnyResponse {
            metadata: self.metadata,
            payload: self.payload.wrap_into_any_payload(),
        }
    }
}

/// An object-safe data provider that returns data structs cast to `dyn Any` trait objects.
///
/// # Examples
///
/// ```
/// use icu_locale_core::langid;
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
/// use std::borrow::Cow;
///
/// let any_provider = HelloWorldProvider.as_any_provider();
///
/// // Downcasting manually
/// assert_eq!(
///     any_provider
///         .load_any(
///             HelloWorldV1Marker::INFO,
///             DataRequest {
///                 id: DataIdentifierBorrowed::for_locale(
///                     &langid!("de").into()
///                 ),
///                 ..Default::default()
///             }
///         )
///         .expect("load should succeed")
///         .downcast::<HelloWorldV1Marker>()
///         .expect("types should match")
///         .payload
///         .get(),
///     &HelloWorldV1 {
///         message: Cow::Borrowed("Hallo Welt"),
///     },
/// );
///
/// // Downcasting automatically
/// let downcasting_provider: &dyn DataProvider<HelloWorldV1Marker> =
///     &any_provider.as_downcasting();
///
/// assert_eq!(
///     downcasting_provider
///         .load(DataRequest {
///             id: DataIdentifierBorrowed::for_locale(&langid!("de").into()),
///             ..Default::default()
///         })
///         .expect("load should succeed")
///         .payload
///         .get(),
///     &HelloWorldV1 {
///         message: Cow::Borrowed("Hallo Welt"),
///     },
/// );
/// ```
pub trait AnyProvider {
    /// Loads an [`AnyPayload`] according to the marker and request.
    fn load_any(&self, marker: DataMarkerInfo, req: DataRequest) -> Result<AnyResponse, DataError>;
}

impl<T: AnyProvider + ?Sized> AnyProvider for &T {
    #[inline]
    fn load_any(&self, marker: DataMarkerInfo, req: DataRequest) -> Result<AnyResponse, DataError> {
        (**self).load_any(marker, req)
    }
}

impl<T: AnyProvider + ?Sized> AnyProvider for alloc::boxed::Box<T> {
    #[inline]
    fn load_any(&self, marker: DataMarkerInfo, req: DataRequest) -> Result<AnyResponse, DataError> {
        (**self).load_any(marker, req)
    }
}

impl<T: AnyProvider + ?Sized> AnyProvider for alloc::rc::Rc<T> {
    #[inline]
    fn load_any(&self, marker: DataMarkerInfo, req: DataRequest) -> Result<AnyResponse, DataError> {
        (**self).load_any(marker, req)
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<T: AnyProvider + ?Sized> AnyProvider for alloc::sync::Arc<T> {
    #[inline]
    fn load_any(&self, marker: DataMarkerInfo, req: DataRequest) -> Result<AnyResponse, DataError> {
        (**self).load_any(marker, req)
    }
}

/// A wrapper over `DynamicDataProvider<AnyMarker>` that implements `AnyProvider`
#[allow(clippy::exhaustive_structs)] // newtype
#[derive(Debug)]
pub struct DynamicDataProviderAnyMarkerWrap<'a, P: ?Sized>(pub &'a P);

/// Blanket-implemented trait adding the [`Self::as_any_provider()`] function.
pub trait AsDynamicDataProviderAnyMarkerWrap {
    /// Returns an object implementing `AnyProvider` when called on `DynamicDataProvider<AnyMarker>`
    fn as_any_provider(&self) -> DynamicDataProviderAnyMarkerWrap<Self>;
}

impl<P> AsDynamicDataProviderAnyMarkerWrap for P
where
    P: DynamicDataProvider<AnyMarker> + ?Sized,
{
    #[inline]
    fn as_any_provider(&self) -> DynamicDataProviderAnyMarkerWrap<P> {
        DynamicDataProviderAnyMarkerWrap(self)
    }
}

impl<P> AnyProvider for DynamicDataProviderAnyMarkerWrap<'_, P>
where
    P: DynamicDataProvider<AnyMarker> + ?Sized,
{
    #[inline]
    fn load_any(&self, marker: DataMarkerInfo, req: DataRequest) -> Result<AnyResponse, DataError> {
        let DataResponse { metadata, payload } = self.0.load_data(marker, req)?;
        Ok(AnyResponse {
            metadata,
            payload: match payload.0 {
                DataPayloadInner::Yoke(yoke) => yoke.try_into_yokeable().ok(),
                DataPayloadInner::StaticRef(_) => None,
            }
            .ok_or_else(|| DataError::custom("Payload not owned"))?,
        })
    }
}

/// A wrapper over `AnyProvider` that implements `DynamicDataProvider<M>` via downcasting
#[allow(clippy::exhaustive_structs)] // newtype
#[derive(Debug)]
pub struct DowncastingAnyProvider<'a, P: ?Sized>(pub &'a P);

/// Blanket-implemented trait adding the [`Self::as_downcasting()`] function.
pub trait AsDowncastingAnyProvider {
    /// Returns an object implementing `DynamicDataProvider<M>` when called on `AnyProvider`
    fn as_downcasting(&self) -> DowncastingAnyProvider<Self>;
}

impl<P> AsDowncastingAnyProvider for P
where
    P: AnyProvider + ?Sized,
{
    #[inline]
    fn as_downcasting(&self) -> DowncastingAnyProvider<P> {
        DowncastingAnyProvider(self)
    }
}

impl<M, P> DataProvider<M> for DowncastingAnyProvider<'_, P>
where
    P: AnyProvider + ?Sized,
    M: DataMarker,
    for<'a> YokeTraitHack<<M::DataStruct as Yokeable<'a>>::Output>: Clone,
    M::DataStruct: ZeroFrom<'static, M::DataStruct>,
    M::DataStruct: MaybeSendSync,
{
    #[inline]
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        self.0
            .load_any(M::INFO, req)?
            .downcast()
            .map_err(|e| e.with_req(M::INFO, req))
    }
}

impl<M, P> DynamicDataProvider<M> for DowncastingAnyProvider<'_, P>
where
    P: AnyProvider + ?Sized,
    M: DynamicDataMarker,
    for<'a> YokeTraitHack<<M::DataStruct as Yokeable<'a>>::Output>: Clone,
    M::DataStruct: ZeroFrom<'static, M::DataStruct>,
    M::DataStruct: MaybeSendSync,
{
    #[inline]
    fn load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponse<M>, DataError> {
        self.0
            .load_any(marker, req)?
            .downcast()
            .map_err(|e| e.with_req(marker, req))
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::hello_world::*;
    use alloc::borrow::Cow;

    const CONST_DATA: HelloWorldV1<'static> = HelloWorldV1 {
        message: Cow::Borrowed("Custom Hello World"),
    };

    #[test]
    fn test_debug() {
        let payload: DataPayload<HelloWorldV1Marker> = DataPayload::from_owned(HelloWorldV1 {
            message: Cow::Borrowed("Custom Hello World"),
        });

        let any_payload = payload.wrap_into_any_payload();
        assert_eq!(
            "AnyPayload { inner: PayloadRc(Any { .. }), type_name: \"icu_provider::hello_world::HelloWorldV1Marker\" }",
            format!("{any_payload:?}")
        );

        struct WrongMarker;

        impl DynamicDataMarker for WrongMarker {
            type DataStruct = u8;
        }

        let err = any_payload.downcast::<WrongMarker>().unwrap_err();
        assert_eq!(
            "ICU4X data error: Downcast: expected icu_provider::any::test::test_debug::WrongMarker, found: icu_provider::hello_world::HelloWorldV1Marker",
            format!("{err}")
        );
    }

    #[test]
    fn test_non_owned_any_marker() {
        // This test demonstrates a code path that can trigger a downcast error
        let payload_result: DataPayload<AnyMarker> =
            DataPayload::from_owned_buffer(Box::new(*b"pretend we're borrowing from here"))
                .map_project(|_, _| AnyPayload::from_static_ref(&CONST_DATA));
        payload_result.downcast::<HelloWorldV1Marker>().unwrap_err();
    }
}
