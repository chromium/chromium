// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::fields::Field;
use crate::pattern::PatternLoadError;
use crate::provider::neo::*;
use crate::provider::time_zones::tz;
use core::fmt;
use icu_provider::prelude::*;
use yoke::Yokeable;

use super::UnstableSealed;

/// Trait for a type that owns datetime names data, usually in the form of data payloads.
///
/// This trait allows for types that contain data for some but not all types of datetime names,
/// allowing for reduced stack size. For example, a type could contain year and month names but
/// not weekday, day period, or time zone names.
#[allow(missing_docs)]
pub trait DateTimeNamesMarker: UnstableSealed {
    type YearNames: DateTimeNamesHolderTrait<YearNamesV1Marker>;
    type MonthNames: DateTimeNamesHolderTrait<MonthNamesV1Marker>;
    type WeekdayNames: DateTimeNamesHolderTrait<WeekdayNamesV1Marker>;
    type DayPeriodNames: DateTimeNamesHolderTrait<DayPeriodNamesV1Marker>;
    type ZoneEssentials: DateTimeNamesHolderTrait<tz::EssentialsV1Marker>;
    type ZoneLocations: DateTimeNamesHolderTrait<tz::LocationsV1Marker>;
    type ZoneGenericLong: DateTimeNamesHolderTrait<tz::MzGenericLongV1Marker>;
    type ZoneGenericShort: DateTimeNamesHolderTrait<tz::MzGenericShortV1Marker>;
    type ZoneSpecificLong: DateTimeNamesHolderTrait<tz::MzSpecificLongV1Marker>;
    type ZoneSpecificShort: DateTimeNamesHolderTrait<tz::MzSpecificShortV1Marker>;
    type MetazoneLookup: DateTimeNamesHolderTrait<tz::MzPeriodV1Marker>;
}

/// Helper trait for [`DateTimeNamesMarker`].
#[allow(missing_docs)]
pub trait DateTimeNamesHolderTrait<M: DynamicDataMarker>: UnstableSealed {
    type Container<Variables: PartialEq + Copy + fmt::Debug>: MaybePayload<M, Variables>
        + fmt::Debug;
}

impl<M: DynamicDataMarker> DateTimeNamesHolderTrait<M> for () {
    type Container<Variables: PartialEq + Copy + fmt::Debug> = ();
}

macro_rules! impl_holder_trait {
    ($marker:path) => {
        impl UnstableSealed for $marker {}
        impl DateTimeNamesHolderTrait<$marker> for $marker {
            type Container<Variables: PartialEq + Copy + fmt::Debug> =
                DataPayloadWithVariables<$marker, Variables>;
        }
    };
}

impl_holder_trait!(YearNamesV1Marker);
impl_holder_trait!(MonthNamesV1Marker);
impl_holder_trait!(WeekdayNamesV1Marker);
impl_holder_trait!(DayPeriodNamesV1Marker);
impl_holder_trait!(tz::EssentialsV1Marker);
impl_holder_trait!(tz::LocationsV1Marker);
impl_holder_trait!(tz::MzGenericLongV1Marker);
impl_holder_trait!(tz::MzGenericShortV1Marker);
impl_holder_trait!(tz::MzSpecificLongV1Marker);
impl_holder_trait!(tz::MzSpecificShortV1Marker);
impl_holder_trait!(tz::MzPeriodV1Marker);

/// An error returned by [`MaybePayload`].
#[allow(missing_docs)]
#[derive(Debug)]
#[non_exhaustive]
pub enum MaybePayloadError {
    TypeTooSpecific,
    ConflictingField,
}

impl MaybePayloadError {
    pub(crate) fn into_load_error(self, field: Field) -> PatternLoadError {
        match self {
            Self::TypeTooSpecific => PatternLoadError::TypeTooSpecific(field),
            Self::ConflictingField => PatternLoadError::ConflictingField(field),
        }
    }
}

/// A type that may or may not be a [`DataPayload`] and may or may not contain
/// a value depending on the type parameter `Variables`.
///
/// Helper trait for [`DateTimeNamesMarker`].
#[allow(missing_docs)]
pub trait MaybePayload<M: DynamicDataMarker, Variables>: UnstableSealed {
    fn new_empty() -> Self;
    fn load_put<P>(
        &mut self,
        provider: &P,
        req: DataRequest,
        variables: Variables,
    ) -> Result<Result<(), DataError>, MaybePayloadError>
    where
        P: BoundDataProvider<M> + ?Sized,
        Self: Sized;
    fn get(&self) -> DataPayloadWithVariablesBorrowed<M, Variables>;
}

/// An implementation of [`MaybePayload`] that wraps a [`DataPayload`],
/// parameterized by `Variables`.
pub struct DataPayloadWithVariables<M: DynamicDataMarker, Variables> {
    inner: OptionalNames<Variables, DataPayload<M>>,
}

impl<M: DynamicDataMarker, Variables> UnstableSealed for DataPayloadWithVariables<M, Variables> {}

impl<M: DynamicDataMarker, Variables> fmt::Debug for DataPayloadWithVariables<M, Variables>
where
    Variables: fmt::Debug,
    DataPayload<M>: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.inner.fmt(f)
    }
}

/// Borrowed version of [`DataPayloadWithVariables`].
#[allow(missing_docs)]
pub struct DataPayloadWithVariablesBorrowed<'data, M: DynamicDataMarker, Variables> {
    pub(crate) inner: OptionalNames<Variables, &'data <M::DataStruct as Yokeable<'data>>::Output>,
}

impl<'data, M: DynamicDataMarker, Variables> fmt::Debug
    for DataPayloadWithVariablesBorrowed<'data, M, Variables>
where
    <M::DataStruct as Yokeable<'data>>::Output: fmt::Debug,
    Variables: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct(core::any::type_name::<Self>())
            .field("inner", &self.inner)
            .finish()
    }
}

impl<M: DynamicDataMarker, Variables> MaybePayload<M, Variables>
    for DataPayloadWithVariables<M, Variables>
where
    Variables: PartialEq + Copy,
{
    #[inline]
    fn new_empty() -> Self {
        Self {
            inner: OptionalNames::None,
        }
    }
    fn load_put<P>(
        &mut self,
        provider: &P,
        req: DataRequest,
        variables: Variables,
    ) -> Result<Result<(), DataError>, MaybePayloadError>
    where
        P: BoundDataProvider<M> + ?Sized,
        Self: Sized,
    {
        let arg_variables = variables;
        match &self.inner {
            OptionalNames::SingleLength { variables, .. } if arg_variables == *variables => {
                return Ok(Ok(()));
            }
            OptionalNames::SingleLength { .. } => {
                return Err(MaybePayloadError::ConflictingField);
            }
            OptionalNames::None => (),
        };
        match provider.load_bound(req) {
            Ok(response) => {
                self.inner = OptionalNames::SingleLength {
                    payload: response.payload,
                    variables: arg_variables,
                };
                Ok(Ok(()))
            }
            Err(e) => Ok(Err(e)),
        }
    }
    #[inline]
    fn get(&self) -> DataPayloadWithVariablesBorrowed<M, Variables> {
        DataPayloadWithVariablesBorrowed {
            inner: self.inner.as_borrowed(),
        }
    }
}

impl<M: DynamicDataMarker, Variables> MaybePayload<M, Variables> for () {
    #[inline]
    fn new_empty() -> Self {}
    #[inline]
    fn load_put<P>(
        &mut self,
        _: &P,
        _: DataRequest,
        _: Variables,
    ) -> Result<Result<(), DataError>, MaybePayloadError>
    where
        P: BoundDataProvider<M> + ?Sized,
        Self: Sized,
    {
        Err(MaybePayloadError::TypeTooSpecific)
    }
    #[allow(clippy::needless_lifetimes)] // Yokeable is involved
    #[inline]
    fn get(&self) -> DataPayloadWithVariablesBorrowed<M, Variables> {
        DataPayloadWithVariablesBorrowed {
            inner: OptionalNames::None,
        }
    }
}

/// This can be extended in the future to support multiple lengths.
/// For now, this type wraps a symbols object tagged with a single length. See #4337
#[derive(Debug, Copy, Clone)]
pub(crate) enum OptionalNames<Variables, Payload> {
    None,
    SingleLength {
        variables: Variables,
        payload: Payload,
    },
}

impl<Variables, Payload> OptionalNames<Variables, Payload>
where
    Variables: Copy + PartialEq,
    Payload: Copy,
{
    pub(crate) fn get_with_variables(&self, arg_variables: Variables) -> Option<Payload> {
        match self {
            Self::None => None,
            Self::SingleLength { variables, payload } if arg_variables == *variables => {
                Some(*payload)
            }
            _ => None,
        }
    }
}

impl<Payload> OptionalNames<(), Payload>
where
    Payload: Copy,
{
    pub(crate) fn get_option(&self) -> Option<Payload> {
        match self {
            Self::SingleLength {
                variables: (),
                payload,
            } => Some(*payload),
            _ => None,
        }
    }
}

impl<M: DynamicDataMarker, Variables> OptionalNames<Variables, DataPayload<M>>
where
    Variables: Copy,
{
    #[allow(clippy::needless_lifetimes)] // Yokeable is involved
    #[inline]
    pub(crate) fn as_borrowed<'a>(
        &'a self,
    ) -> OptionalNames<Variables, &'a <M::DataStruct as Yokeable<'a>>::Output> {
        match self {
            Self::None => OptionalNames::None,
            Self::SingleLength { variables, payload } => OptionalNames::SingleLength {
                variables: *variables,
                payload: payload.get(),
            },
        }
    }
}
