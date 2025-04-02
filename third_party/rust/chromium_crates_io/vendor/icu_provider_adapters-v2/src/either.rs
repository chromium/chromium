// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Helpers for switching between multiple providers.

use alloc::collections::BTreeSet;
#[cfg(feature = "export")]
use icu_provider::export::ExportableProvider;
use icu_provider::prelude::*;

/// A provider that is one of two types determined at runtime.
///
/// Data provider traits implemented by both `P0` and `P1` are implemented on
/// `EitherProvider<P0, P1>`.
#[allow(clippy::exhaustive_enums)] // this is stable
#[derive(Debug)]
pub enum EitherProvider<P0, P1> {
    /// A value of type `P0`.
    A(P0),
    /// A value of type `P1`.
    B(P1),
}

impl<M: DynamicDataMarker, P0: DynamicDataProvider<M>, P1: DynamicDataProvider<M>>
    DynamicDataProvider<M> for EitherProvider<P0, P1>
{
    #[inline]
    fn load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponse<M>, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.load_data(marker, req),
            B(p) => p.load_data(marker, req),
        }
    }
}

impl<M: DynamicDataMarker, P0: DynamicDryDataProvider<M>, P1: DynamicDryDataProvider<M>>
    DynamicDryDataProvider<M> for EitherProvider<P0, P1>
{
    #[inline]
    fn dry_load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponseMetadata, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.dry_load_data(marker, req),
            B(p) => p.dry_load_data(marker, req),
        }
    }
}

impl<M: DataMarker, P0: DataProvider<M>, P1: DataProvider<M>> DataProvider<M>
    for EitherProvider<P0, P1>
{
    #[inline]
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.load(req),
            B(p) => p.load(req),
        }
    }
}

impl<M: DataMarker, P0: DryDataProvider<M>, P1: DryDataProvider<M>> DryDataProvider<M>
    for EitherProvider<P0, P1>
{
    #[inline]
    fn dry_load(&self, req: DataRequest) -> Result<DataResponseMetadata, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.dry_load(req),
            B(p) => p.dry_load(req),
        }
    }
}

impl<
        M: DynamicDataMarker,
        P0: IterableDynamicDataProvider<M>,
        P1: IterableDynamicDataProvider<M>,
    > IterableDynamicDataProvider<M> for EitherProvider<P0, P1>
{
    #[inline]
    fn iter_ids_for_marker(
        &self,
        marker: DataMarkerInfo,
    ) -> Result<BTreeSet<DataIdentifierCow>, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.iter_ids_for_marker(marker),
            B(p) => p.iter_ids_for_marker(marker),
        }
    }
}

impl<M: DataMarker, P0: IterableDataProvider<M>, P1: IterableDataProvider<M>>
    IterableDataProvider<M> for EitherProvider<P0, P1>
{
    #[inline]
    fn iter_ids(&self) -> Result<BTreeSet<DataIdentifierCow>, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.iter_ids(),
            B(p) => p.iter_ids(),
        }
    }
}

#[cfg(feature = "export")]
impl<P0, P1> ExportableProvider for EitherProvider<P0, P1>
where
    P0: ExportableProvider,
    P1: ExportableProvider,
{
    fn supported_markers(&self) -> alloc::collections::BTreeSet<DataMarkerInfo> {
        use EitherProvider::*;
        match self {
            A(p) => p.supported_markers(),
            B(p) => p.supported_markers(),
        }
    }
}
