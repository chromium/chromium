// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Providers that combine multiple other providers.
//!
//! # Types of Forking Providers
//!
//! ## Marker-Based
//!
//! To fork between providers that support different data markers, see:
//!
//! - [`ForkByMarkerProvider`]
//! - [`MultiForkByMarkerProvider`]
//!
//! ## Locale-Based
//!
//! To fork between providers that support different locales, see:
//!
//! - [`ForkByErrorProvider`]`<`[`IdentiferNotFoundPredicate`]`>`
//! - [`MultiForkByErrorProvider`]`<`[`IdentiferNotFoundPredicate`]`>`
//!
//! [`IdentiferNotFoundPredicate`]: predicates::IdentifierNotFoundPredicate
//!
//! # Examples
//!
//! See:
//!
//! - [`ForkByMarkerProvider`]
//! - [`MultiForkByMarkerProvider`]
//! - [`IdentiferNotFoundPredicate`]

use alloc::vec::Vec;

mod by_error;

pub mod predicates;

#[macro_use]
mod macros;

pub use by_error::ForkByErrorProvider;
pub use by_error::MultiForkByErrorProvider;

use predicates::ForkByErrorPredicate;
use predicates::MarkerNotFoundPredicate;

/// Create a provider that returns data from one of two child providers based on the marker.
///
/// The result of the first provider that supports a particular [`DataMarkerInfo`] will be returned,
/// even if the request failed for other reasons (such as an unsupported language). Therefore,
/// you should add child providers that support disjoint sets of markers.
///
/// [`ForkByMarkerProvider`] does not support forking between [`DataProvider`]s. However, it
/// supports forking between [`AnyProvider`], [`BufferProvider`], and [`DynamicDataProvider`].
///
/// # Examples
///
/// Normal usage:
///
/// ```
/// use icu_locale::langid;
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
/// use icu_provider_adapters::fork::ForkByMarkerProvider;
///
/// struct DummyBufferProvider;
/// impl DynamicDataProvider<BufferMarker> for DummyBufferProvider {
///     fn load_data(
///         &self,
///         marker: DataMarkerInfo,
///         req: DataRequest,
///     ) -> Result<DataResponse<BufferMarker>, DataError> {
///         Err(DataErrorKind::MarkerNotFound.with_req(marker, req))
///     }
/// }
///
/// let forking_provider = ForkByMarkerProvider::new(
///     DummyBufferProvider,
///     HelloWorldProvider.into_json_provider(),
/// );
///
/// let provider = forking_provider.as_deserializing();
///
/// let german_hello_world: DataResponse<HelloWorldV1Marker> = provider
///     .load(DataRequest {
///         id: DataIdentifierBorrowed::for_locale(&langid!("de").into()),
///         ..Default::default()
///     })
///     .expect("Loading should succeed");
///
/// assert_eq!("Hallo Welt", german_hello_world.payload.get().message);
/// ```
///
/// Stops at the first provider supporting a marker, even if the locale is not supported:
///
/// ```
/// use icu_locale::{subtags::language, langid};
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
/// use icu_provider_adapters::filter::FilterDataProvider;
/// use icu_provider_adapters::fork::ForkByMarkerProvider;
///
/// let forking_provider = ForkByMarkerProvider::new(
///     FilterDataProvider::new(
///         HelloWorldProvider.into_json_provider(),
///         "Chinese"
///     )
///     .with_filter(|id| id.locale.language == language!("zh")),
///     FilterDataProvider::new(
///         HelloWorldProvider.into_json_provider(),
///         "German"
///     )
///     .with_filter(|id| id.locale.language == language!("de")),
/// );
///
/// let provider: &dyn DataProvider<HelloWorldV1Marker> =
///     &forking_provider.as_deserializing();
///
/// // Chinese is the first provider, so this succeeds
/// let chinese_hello_world = provider
///     .load(DataRequest {
///         id: DataIdentifierBorrowed::for_locale(&langid!("zh").into()),
///         ..Default::default()
///     })
///     .expect("Loading should succeed");
///
/// assert_eq!("你好世界", chinese_hello_world.payload.get().message);
///
/// // German is shadowed by Chinese, so this fails
/// provider
///     .load(DataRequest {
///         id: DataIdentifierBorrowed::for_locale(&langid!("de").into()),
///         ..Default::default()
///     })
///     .expect_err("Should stop at the first provider, even though the second has data");
/// ```
///
/// [`DataMarkerInfo`]: icu_provider::DataMarkerInfo
/// [`DataProvider`]: icu_provider::DataProvider
/// [`AnyProvider`]: icu_provider::any::AnyProvider
/// [`BufferProvider`]: icu_provider::buf::BufferProvider
/// [`DynamicDataProvider`]: icu_provider::DynamicDataProvider
pub type ForkByMarkerProvider<P0, P1> = ForkByErrorProvider<P0, P1, MarkerNotFoundPredicate>;

impl<P0, P1> ForkByMarkerProvider<P0, P1> {
    /// A provider that returns data from one of two child providers based on the marker.
    ///
    /// See [`ForkByMarkerProvider`].
    pub fn new(p0: P0, p1: P1) -> Self {
        ForkByErrorProvider::new_with_predicate(p0, p1, MarkerNotFoundPredicate)
    }
}

/// A provider that returns data from the first child provider supporting the marker.
///
/// The result of the first provider that supports a particular [`DataMarkerInfo`] will be returned,
/// even if the request failed for other reasons (such as an unsupported language). Therefore,
/// you should add child providers that support disjoint sets of markers.
///
/// [`MultiForkByMarkerProvider`] does not support forking between [`DataProvider`]s. However, it
/// supports forking between [`AnyProvider`], [`BufferProvider`], and [`DynamicDataProvider`].
///
/// # Examples
///
/// ```
/// use icu_locale::{subtags::language, langid};
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
/// use icu_provider_adapters::filter::FilterDataProvider;
/// use icu_provider_adapters::fork::MultiForkByMarkerProvider;
///
/// let forking_provider = MultiForkByMarkerProvider::new(
///     vec![
///         FilterDataProvider::new(
///             HelloWorldProvider.into_json_provider(),
///             "Chinese"
///         )
///         .with_filter(|id| id.locale.language == language!("zh")),
///         FilterDataProvider::new(
///             HelloWorldProvider.into_json_provider(),
///             "German"
///         )
///         .with_filter(|id| id.locale.language == language!("de")),
///     ],
/// );
///
/// let provider: &dyn DataProvider<HelloWorldV1Marker> =
///     &forking_provider.as_deserializing();
///
/// // Chinese is the first provider, so this succeeds
/// let chinese_hello_world = provider
///     .load(DataRequest {
///         id: DataIdentifierBorrowed::for_locale(&langid!("zh").into()),
///         ..Default::default()
///     })
///     .expect("Loading should succeed");
///
/// assert_eq!("你好世界", chinese_hello_world.payload.get().message);
///
/// // German is shadowed by Chinese, so this fails
/// provider
///     .load(DataRequest {
///         id: DataIdentifierBorrowed::for_locale(&langid!("de").into()),
///         ..Default::default()
///     })
///     .expect_err("Should stop at the first provider, even though the second has data");
/// ```
///
/// [`DataMarkerInfo`]: icu_provider::DataMarkerInfo
/// [`DataProvider`]: icu_provider::DataProvider
/// [`AnyProvider`]: icu_provider::any::AnyProvider
/// [`BufferProvider`]: icu_provider::buf::BufferProvider
/// [`DynamicDataProvider`]: icu_provider::DynamicDataProvider
pub type MultiForkByMarkerProvider<P> = MultiForkByErrorProvider<P, MarkerNotFoundPredicate>;

impl<P> MultiForkByMarkerProvider<P> {
    /// Create a provider that returns data from the first child provider supporting the marker.
    ///
    /// See [`MultiForkByMarkerProvider`].
    pub fn new(providers: Vec<P>) -> Self {
        MultiForkByErrorProvider::new_with_predicate(providers, MarkerNotFoundPredicate)
    }
}
