// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Providers that filter resource requests.
//!
//! Requests that fail a filter test will return [`DataError`] of kind [`Filtered`](
//! DataErrorKind::IdentifierNotFound) and will not appear in [`IterableDynamicDataProvider`] iterators.
//!
//! # Examples
//!
//! ```
//! use icu_locale::subtags::language;
//! use icu_provider::hello_world::*;
//! use icu_provider::prelude::*;
//! use icu_provider_adapters::filter::FilterDataProvider;
//!
//! // Only return German data from a HelloWorldProvider:
//! FilterDataProvider::new(HelloWorldProvider, "Demo German-only filter")
//!     .with_filter(|id| id.locale.language == language!("de"));
//! ```

mod impls;

use alloc::collections::BTreeSet;
#[cfg(feature = "export")]
use icu_provider::export::ExportableProvider;
use icu_provider::prelude::*;

/// A data provider that selectively filters out data requests.
///
/// Data requests that are rejected by the filter will return a [`DataError`] with kind
/// [`Filtered`](DataErrorKind::IdentifierNotFound), and they will not be returned
/// by [`IterableDynamicDataProvider::iter_ids_for_marker`].
///
/// Although this struct can be created directly, the traits in this module provide helper
/// functions for common filtering patterns.
#[allow(clippy::exhaustive_structs)] // this type is stable
#[derive(Debug)]
pub struct FilterDataProvider<D, F>
where
    F: Fn(DataIdentifierBorrowed) -> bool,
{
    /// The data provider to which we delegate requests.
    pub inner: D,

    /// The predicate function. A return value of `true` indicates that the request should
    /// proceed as normal; a return value of `false` will reject the request.
    pub predicate: F,

    /// A name for this filter, used in error messages.
    pub filter_name: &'static str,
}

impl<D, F> FilterDataProvider<D, F>
where
    F: Fn(DataIdentifierBorrowed) -> bool,
{
    fn check(&self, marker: DataMarkerInfo, req: DataRequest) -> Result<(), DataError> {
        if !(self.predicate)(req.id) {
            return Err(DataErrorKind::IdentifierNotFound
                .with_str_context(self.filter_name)
                .with_req(marker, req));
        }
        Ok(())
    }
}

impl<D, F, M> DynamicDataProvider<M> for FilterDataProvider<D, F>
where
    F: Fn(DataIdentifierBorrowed) -> bool,
    M: DynamicDataMarker,
    D: DynamicDataProvider<M>,
{
    fn load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponse<M>, DataError> {
        self.check(marker, req)?;
        self.inner.load_data(marker, req)
    }
}

impl<D, F, M> DynamicDryDataProvider<M> for FilterDataProvider<D, F>
where
    F: Fn(DataIdentifierBorrowed) -> bool,
    M: DynamicDataMarker,
    D: DynamicDryDataProvider<M>,
{
    fn dry_load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponseMetadata, DataError> {
        self.check(marker, req)?;
        self.inner.dry_load_data(marker, req)
    }
}

impl<D, F, M> DataProvider<M> for FilterDataProvider<D, F>
where
    F: Fn(DataIdentifierBorrowed) -> bool,
    M: DataMarker,
    D: DataProvider<M>,
{
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        self.check(M::INFO, req)?;
        self.inner.load(req)
    }
}

impl<D, F, M> DryDataProvider<M> for FilterDataProvider<D, F>
where
    F: Fn(DataIdentifierBorrowed) -> bool,
    M: DataMarker,
    D: DryDataProvider<M>,
{
    fn dry_load(&self, req: DataRequest) -> Result<DataResponseMetadata, DataError> {
        self.check(M::INFO, req)?;
        self.inner.dry_load(req)
    }
}

impl<M, D, F> IterableDynamicDataProvider<M> for FilterDataProvider<D, F>
where
    M: DynamicDataMarker,
    F: Fn(DataIdentifierBorrowed) -> bool,
    D: IterableDynamicDataProvider<M>,
{
    fn iter_ids_for_marker(
        &self,
        marker: DataMarkerInfo,
    ) -> Result<BTreeSet<DataIdentifierCow>, DataError> {
        self.inner.iter_ids_for_marker(marker).map(|set| {
            // Use filter_map instead of filter to avoid cloning the locale
            set.into_iter()
                .filter(|id| (self.predicate)(id.as_borrowed()))
                .collect()
        })
    }
}

impl<M, D, F> IterableDataProvider<M> for FilterDataProvider<D, F>
where
    M: DataMarker,
    F: Fn(DataIdentifierBorrowed) -> bool,
    D: IterableDataProvider<M>,
{
    fn iter_ids(&self) -> Result<BTreeSet<DataIdentifierCow>, DataError> {
        self.inner.iter_ids().map(|vec| {
            // Use filter_map instead of filter to avoid cloning the locale
            vec.into_iter()
                .filter(|id| (self.predicate)(id.as_borrowed()))
                .collect()
        })
    }
}

#[cfg(feature = "export")]
impl<P0, F> ExportableProvider for FilterDataProvider<P0, F>
where
    P0: ExportableProvider,
    F: Fn(DataIdentifierBorrowed) -> bool + Sync,
{
    fn supported_markers(&self) -> alloc::collections::BTreeSet<DataMarkerInfo> {
        // The predicate only takes DataIdentifier, not DataMarker, so we are not impacted
        self.inner.supported_markers()
    }
}
