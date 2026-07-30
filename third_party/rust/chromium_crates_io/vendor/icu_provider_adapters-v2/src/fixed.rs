// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider always serving the same struct.

use core::fmt;
use icu_provider::prelude::*;
use yoke::Yokeable;

/// A data provider that returns clones of a fixed type-erased payload.
///
/// # Examples
///
/// ```
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
/// use icu_provider_adapters::fixed::FixedProvider;
/// use std::borrow::Cow;
/// use writeable::assert_writeable_eq;
///
/// let provider = FixedProvider::<HelloWorldV1>::from_static(&HelloWorld {
///     message: Cow::Borrowed("custom hello world"),
/// });
///
/// // Check that it works:
/// let formatter =
///     HelloWorldFormatter::try_new_unstable(&provider, Default::default())
///         .expect("marker matches");
/// assert_writeable_eq!(formatter.format(), "custom hello world");
/// ```
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct FixedProvider<M: DataMarker> {
    data: DataPayload<M>,
}

impl<M: DataMarker> FixedProvider<M> {
    /// Creates a `FixedProvider` with an owned (allocated) payload of the given data.
    pub fn from_owned(data: M::DataStruct) -> Self {
        Self::from_payload(DataPayload::from_owned(data))
    }

    /// Creates a `FixedProvider` with a statically borrowed payload of the given data.
    pub fn from_static(data: &'static M::DataStruct) -> Self {
        FixedProvider {
            data: DataPayload::from_static_ref(data),
        }
    }

    /// Creates a `FixedProvider` from an existing [`DataPayload`].
    pub fn from_payload(data: DataPayload<M>) -> Self {
        FixedProvider { data }
    }

    /// Creates a `FixedProvider` with the default (allocated) version of the data struct.
    pub fn new_default() -> Self
    where
        M::DataStruct: Default,
    {
        Self::from_owned(M::DataStruct::default())
    }
}

impl<M> DataProvider<M> for FixedProvider<M>
where
    M: DataMarker,
    for<'a> <M::DataStruct as Yokeable<'a>>::Output: Clone,
{
    fn load(&self, _: DataRequest) -> Result<DataResponse<M>, DataError> {
        Ok(DataResponse {
            metadata: Default::default(),
            payload: self.data.clone(),
        })
    }
}

impl<M> fmt::Debug for FixedProvider<M>
where
    M: DynamicDataMarker,
    M: DataMarker,
    for<'a> &'a <M::DataStruct as Yokeable<'a>>::Output: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.data.fmt(f)
    }
}
