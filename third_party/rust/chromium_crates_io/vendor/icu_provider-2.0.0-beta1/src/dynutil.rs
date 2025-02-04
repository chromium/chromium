// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Utilities for using trait objects with `DataPayload`.

/// Trait to allow conversion from `DataPayload<T>` to `DataPayload<S>`.
///
/// This trait can be manually implemented in order to enable [`impl_dynamic_data_provider`].
///
/// [`DataPayload::downcast`]: crate::DataPayload::downcast
pub trait UpcastDataPayload<M>
where
    M: crate::DynamicDataMarker,
    Self: Sized + crate::DynamicDataMarker,
{
    /// Upcast a `DataPayload<T>` to a `DataPayload<S>` where `T` implements trait `S`.
    ///
    /// # Examples
    ///
    /// Upcast and then downcast a data struct of type `Cow<str>` (cart type `String`) via
    /// [`AnyPayload`](crate::any::AnyPayload):
    ///
    /// ```
    /// use icu_provider::dynutil::UpcastDataPayload;
    /// use icu_provider::hello_world::*;
    /// use icu_provider::prelude::*;
    /// use std::borrow::Cow;
    ///
    /// let original = DataPayload::<HelloWorldV1Marker>::from_static_str("foo");
    /// let upcasted = AnyMarker::upcast(original);
    /// let downcasted = upcasted
    ///     .downcast::<HelloWorldV1Marker>()
    ///     .expect("Type conversion");
    /// assert_eq!(downcasted.get().message, "foo");
    /// ```
    fn upcast(other: crate::DataPayload<M>) -> crate::DataPayload<Self>;
}

/// Implements [`UpcastDataPayload`] from several data markers to a single data marker
/// that all share the same [`DynamicDataMarker::DataStruct`].
///
/// # Examples
///
/// ```
/// use icu_provider::prelude::*;
/// use std::borrow::Cow;
///
/// #[icu_provider::data_struct(
///     FooV1Marker,
///     BarV1Marker = "demo/bar@1",
///     BazV1Marker = "demo/baz@1"
/// )]
/// pub struct FooV1<'data> {
///     message: Cow<'data, str>,
/// };
///
/// icu_provider::dynutil::impl_casting_upcast!(
///     FooV1Marker,
///     [BarV1Marker, BazV1Marker,]
/// );
/// ```
///
/// [`DynamicDataMarker::DataStruct`]: crate::DynamicDataMarker::DataStruct
#[macro_export]
#[doc(hidden)] // macro
macro_rules! __impl_casting_upcast {
    ($dyn_m:path, [ $($struct_m:ident),+, ]) => {
        $(
            impl $crate::dynutil::UpcastDataPayload<$struct_m> for $dyn_m {
                fn upcast(other: $crate::DataPayload<$struct_m>) -> $crate::DataPayload<$dyn_m> {
                    other.cast()
                }
            }
        )+
    }
}
#[doc(inline)]
pub use __impl_casting_upcast as impl_casting_upcast;

/// Implements [`DynamicDataProvider`] for a marker type `S` on a type that already implements
/// [`DynamicDataProvider`] or [`DataProvider`] for one or more `M`, where `M` is a concrete type
/// that is convertible to `S` via [`UpcastDataPayload`].
///
/// Use this macro to add support to your data provider for:
///
/// - [`AnyPayload`] if your provider can return typed objects as [`Any`](core::any::Any).
///
/// ## Wrapping DataProvider
///
/// If your type implements [`DataProvider`], pass a list of markers as the second argument.
/// This results in a `DynamicDataProvider` that delegates to a specific marker if the marker
/// matches or else returns [`DataErrorKind::MarkerNotFound`].
///
/// ```
/// use icu_provider::prelude::*;
/// use icu_provider::hello_world::*;
/// use icu_provider::marker::NeverMarker;
/// use icu_locale_core::langid;
/// #
/// # // Duplicating HelloWorldProvider because the real one already implements DynamicDataProvider<AnyMarker>
/// # struct HelloWorldProvider;
/// # impl DataProvider<HelloWorldV1Marker> for HelloWorldProvider {
/// #     fn load(
/// #         &self,
/// #         req: DataRequest,
/// #     ) -> Result<DataResponse<HelloWorldV1Marker>, DataError> {
/// #         icu_provider::hello_world::HelloWorldProvider.load(req)
/// #     }
/// # }
///
/// // Implement DynamicDataProvider<AnyMarker> on HelloWorldProvider: DataProvider<HelloWorldV1Marker>
/// icu_provider::dynutil::impl_dynamic_data_provider!(HelloWorldProvider, [HelloWorldV1Marker,], AnyMarker);
///
/// // Successful because the marker matches:
/// HelloWorldProvider.load_data(HelloWorldV1Marker::INFO, DataRequest {
///     id: DataIdentifierBorrowed::for_locale(&langid!("de").into()),
///     ..Default::default()
/// }).unwrap();
///
/// # struct DummyMarker;
/// # impl DynamicDataMarker for DummyMarker {
/// #     type DataStruct = <HelloWorldV1Marker as DynamicDataMarker>::DataStruct;
/// # }
/// # impl DataMarker for DummyMarker {
/// #     const INFO: DataMarkerInfo = DataMarkerInfo::from_path(icu_provider::marker::data_marker_path!("dummy@1"));
/// # }
/// // MissingDataMarker error as the marker does not match:
/// assert_eq!(
///     HelloWorldProvider.load_data(DummyMarker::INFO, DataRequest {
///     id: DataIdentifierBorrowed::for_locale(&langid!("de").into()),
///     ..Default::default()
/// }).unwrap_err().kind,
///     DataErrorKind::MarkerNotFound,
/// );
/// ```
///
/// ## Wrapping DynamicDataProvider
///
/// It is also possible to wrap a [`DynamicDataProvider`] to create another [`DynamicDataProvider`]. To do this,
/// pass a match-like statement for markers as the second argument:
///
/// ```
/// use icu_provider::prelude::*;
/// use icu_provider::hello_world::*;
/// use icu_provider::any::*;
/// use icu_locale_core::langid;
/// #
/// # struct HelloWorldProvider;
/// # impl DynamicDataProvider<HelloWorldV1Marker> for HelloWorldProvider {
/// #     fn load_data(&self, marker: DataMarkerInfo, req: DataRequest)
/// #             -> Result<DataResponse<HelloWorldV1Marker>, DataError> {
/// #         icu_provider::hello_world::HelloWorldProvider.load(req)
/// #     }
/// # }
///
/// // Implement DataProvider<AnyMarker> on HelloWorldProvider: DynamicDataProvider<HelloWorldV1Marker>
/// icu_provider::dynutil::impl_dynamic_data_provider!(HelloWorldProvider, {
///     // Match HelloWorldV1Marker::INFO and delegate to DynamicDataProvider<HelloWorldV1Marker>.
///     HW = HelloWorldV1Marker::INFO => HelloWorldV1Marker,
///     // Send the wildcard match also to DynamicDataProvider<HelloWorldV1Marker>.
///     _ => HelloWorldV1Marker,
/// }, AnyMarker);
///
/// // Successful because the marker matches:
/// HelloWorldProvider.as_any_provider().load_any(HelloWorldV1Marker::INFO, DataRequest {
///     id: DataIdentifierBorrowed::for_locale(&langid!("de").into()),
///     ..Default::default()
/// }).unwrap();
///
/// // Because of the wildcard, any marker actually works:
/// struct DummyMarker;
/// impl DynamicDataMarker for DummyMarker {
///     type DataStruct = <HelloWorldV1Marker as DynamicDataMarker>::DataStruct;
/// }
/// impl DataMarker for DummyMarker {
///     const INFO: DataMarkerInfo = DataMarkerInfo::from_path(icu_provider::marker::data_marker_path!("dummy@1"));
/// }
/// HelloWorldProvider.as_any_provider().load_any(DummyMarker::INFO, DataRequest {
///     id: DataIdentifierBorrowed::for_locale(&langid!("de").into()),
///     ..Default::default()
/// }).unwrap();
/// ```
///
/// [`DynamicDataProvider`]: crate::DynamicDataProvider
/// [`DataProvider`]: crate::DataProvider
/// [`AnyPayload`]: (crate::any::AnyPayload)
/// [`DataErrorKind::MarkerNotFound`]: (crate::DataErrorKind::MarkerNotFound)
/// [`SerializeMarker`]: (crate::buf::SerializeMarker)
#[doc(hidden)] // macro
#[macro_export]
macro_rules! __impl_dynamic_data_provider {
    // allow passing in multiple things to do and get dispatched
    ($provider:ty, $arms:tt, $one:path, $($rest:path),+) => {
        $crate::dynutil::impl_dynamic_data_provider!(
            $provider,
            $arms,
            $one
        );

        $crate::dynutil::impl_dynamic_data_provider!(
            $provider,
            $arms,
            $($rest),+
        );
    };

    ($provider:ty, { $($ident:ident = $marker:path => $struct_m:ty),+, $(_ => $struct_d:ty,)?}, $dyn_m:ty) => {
        impl $crate::DynamicDataProvider<$dyn_m> for $provider
        {
            fn load_data(
                &self,
                marker: $crate::DataMarkerInfo,
                req: $crate::DataRequest,
            ) -> Result<
                $crate::DataResponse<$dyn_m>,
                $crate::DataError,
            > {
                match marker.path.hashed() {
                    $(
                        h if h == $marker.path.hashed() => {
                            let result: $crate::DataResponse<$struct_m> =
                                $crate::DynamicDataProvider::<$struct_m>::load_data(self, marker, req)?;
                            Ok($crate::DataResponse {
                                metadata: result.metadata,
                                payload: $crate::dynutil::UpcastDataPayload::<$struct_m>::upcast(result.payload),
                            })
                        }
                    )+,
                    $(
                        _ => {
                            let result: $crate::DataResponse<$struct_d> =
                                $crate::DynamicDataProvider::<$struct_d>::load_data(self, marker, req)?;
                            Ok($crate::DataResponse {
                                metadata: result.metadata,
                                payload: $crate::dynutil::UpcastDataPayload::<$struct_d>::upcast(result.payload),
                            })
                        }
                    )?
                    _ => Err($crate::DataErrorKind::MarkerNotFound.with_req(marker, req))
                }
            }
        }

    };
    ($provider:ty, [ $($(#[$cfg:meta])? $struct_m:ty),+, ], $dyn_m:path) => {
        impl $crate::DynamicDataProvider<$dyn_m> for $provider
        {
            fn load_data(
                &self,
                marker: $crate::DataMarkerInfo,
                req: $crate::DataRequest,
            ) -> Result<
                $crate::DataResponse<$dyn_m>,
                $crate::DataError,
            > {
                match marker.path.hashed() {
                    $(
                        $(#[$cfg])?
                        h if h == <$struct_m as $crate::DataMarker>::INFO.path.hashed() => {
                            let result: $crate::DataResponse<$struct_m> =
                                $crate::DataProvider::load(self, req)?;
                            Ok($crate::DataResponse {
                                metadata: result.metadata,
                                payload: $crate::dynutil::UpcastDataPayload::<$struct_m>::upcast(result.payload),
                            })
                        }
                    )+,
                    _ => Err($crate::DataErrorKind::MarkerNotFound.with_req(marker, req))
                }
            }
        }
    };
}
#[doc(inline)]
pub use __impl_dynamic_data_provider as impl_dynamic_data_provider;

#[doc(hidden)] // macro
#[macro_export]
macro_rules! __impl_iterable_dynamic_data_provider {
    ($provider:ty, [ $($(#[$cfg:meta])? $struct_m:ty),+, ], $dyn_m:path) => {
        impl $crate::IterableDynamicDataProvider<$dyn_m> for $provider {
            fn iter_ids_for_marker(&self, marker: $crate::DataMarkerInfo) -> Result<std::collections::BTreeSet<$crate::DataIdentifierCow>, $crate::DataError> {
                match marker.path.hashed() {
                    $(
                        $(#[$cfg])?
                        h if h == <$struct_m as $crate::DataMarker>::INFO.path.hashed() => {
                            $crate::IterableDataProvider::<$struct_m>::iter_ids(self)
                        }
                    )+,
                    _ => Err($crate::DataErrorKind::MarkerNotFound.with_marker(marker))
                }
            }
        }
    }
}
#[doc(inline)]
pub use __impl_iterable_dynamic_data_provider as impl_iterable_dynamic_data_provider;
