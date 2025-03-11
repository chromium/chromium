// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::error::ErrorField;
use crate::pattern::{
    DayPeriodNameLength, MonthNameLength, PatternLoadError, WeekdayNameLength, YearNameLength,
};
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
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
#[allow(missing_docs)]
pub trait DateTimeNamesMarker: UnstableSealed {
    type YearNames: NamesContainer<YearNamesV1, YearNameLength>;
    type MonthNames: NamesContainer<MonthNamesV1, MonthNameLength>;
    type WeekdayNames: NamesContainer<WeekdayNamesV1, WeekdayNameLength>;
    type DayPeriodNames: NamesContainer<DayPeriodNamesV1, DayPeriodNameLength>;
    type ZoneEssentials: NamesContainer<tz::EssentialsV1, ()>;
    type ZoneLocations: NamesContainer<tz::LocationsV1, ()>;
    type ZoneLocationsRoot: NamesContainer<tz::LocationsRootV1, ()>;
    type ZoneExemplars: NamesContainer<tz::ExemplarCitiesV1, ()>;
    type ZoneExemplarsRoot: NamesContainer<tz::ExemplarCitiesRootV1, ()>;
    type ZoneGenericLong: NamesContainer<tz::MzGenericLongV1, ()>;
    type ZoneGenericShort: NamesContainer<tz::MzGenericShortV1, ()>;
    type ZoneStandardLong: NamesContainer<tz::MzStandardLongV1, ()>;
    type ZoneSpecificLong: NamesContainer<tz::MzSpecificLongV1, ()>;
    type ZoneSpecificShort: NamesContainer<tz::MzSpecificShortV1, ()>;
    type MetazoneLookup: NamesContainer<tz::MzPeriodV1, ()>;
}

/// Trait that associates a container for a payload parameterized by the given variables.
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
#[allow(missing_docs)]
pub trait NamesContainer<M: DynamicDataMarker, Variables>: UnstableSealed
where
    Variables: PartialEq + Copy + fmt::Debug,
{
    type Container: MaybePayload<M, Variables> + fmt::Debug;
}

impl<M: DynamicDataMarker, Variables> NamesContainer<M, Variables> for ()
where
    Variables: PartialEq + Copy + fmt::Debug,
{
    type Container = ();
}

macro_rules! impl_holder_trait {
    ($marker:path) => {
        impl UnstableSealed for $marker {}
        impl<Variables> NamesContainer<$marker, Variables> for $marker
        where
            Variables: PartialEq + Copy + fmt::Debug,
        {
            type Container = DataPayloadWithVariables<$marker, Variables>;
        }
    };
}

impl_holder_trait!(YearNamesV1);
impl_holder_trait!(MonthNamesV1);
impl_holder_trait!(WeekdayNamesV1);
impl_holder_trait!(DayPeriodNamesV1);
impl_holder_trait!(tz::EssentialsV1);
impl_holder_trait!(tz::LocationsV1);
impl_holder_trait!(tz::LocationsRootV1);
impl_holder_trait!(tz::ExemplarCitiesV1);
impl_holder_trait!(tz::ExemplarCitiesRootV1);
impl_holder_trait!(tz::MzGenericLongV1);
impl_holder_trait!(tz::MzGenericShortV1);
impl_holder_trait!(tz::MzStandardLongV1);
impl_holder_trait!(tz::MzSpecificLongV1);
impl_holder_trait!(tz::MzSpecificShortV1);
impl_holder_trait!(tz::MzPeriodV1);

/// An error returned by [`MaybePayload`].
#[allow(missing_docs)]
#[derive(Debug, displaydoc::Display)]
#[non_exhaustive]
pub enum MaybePayloadError {
    /// TODO
    TypeTooSpecific,
    /// TODO
    ConflictingField,
}

impl core::error::Error for MaybePayloadError {}

impl MaybePayloadError {
    pub(crate) fn into_load_error(self, error_field: ErrorField) -> PatternLoadError {
        match self {
            Self::TypeTooSpecific => PatternLoadError::TypeTooSpecific(error_field),
            Self::ConflictingField => PatternLoadError::ConflictingField(error_field),
        }
    }
}

/// A type that may or may not be a [`DataPayload`] and may or may not contain
/// a value depending on the type parameter `Variables`.
///
/// Helper trait for [`DateTimeNamesMarker`].
///
/// <div class="stab unstable">
/// 🚧 This trait is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not implement this trait in userland unless you are prepared for things to occasionally break.
/// </div>
#[allow(missing_docs)]
pub trait MaybePayload<M: DynamicDataMarker, Variables>: UnstableSealed {
    fn new_empty() -> Self;
    fn load_put<P>(
        &mut self,
        provider: &P,
        req: DataRequest,
        variables: Variables,
    ) -> Result<Result<DataResponseMetadata, DataError>, MaybePayloadError>
    where
        P: BoundDataProvider<M> + ?Sized,
        Self: Sized;
    fn get(&self) -> DataPayloadWithVariablesBorrowed<M, Variables>;
}

/// An implementation of [`MaybePayload`] that wraps an optional [`DataPayload`],
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

// NOTE: This impl enables `cast_into_fset` functions to work.
impl<M: DynamicDataMarker, Variables> From<()> for DataPayloadWithVariables<M, Variables> {
    #[inline]
    fn from(_: ()) -> Self {
        Self {
            inner: OptionalNames::None,
        }
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
    ) -> Result<Result<DataResponseMetadata, DataError>, MaybePayloadError>
    where
        P: BoundDataProvider<M> + ?Sized,
        Self: Sized,
    {
        let arg_variables = variables;
        match &self.inner {
            OptionalNames::SingleLength { variables, .. } if arg_variables == *variables => {
                // TODO(#6063): probably not correct
                return Ok(Ok(Default::default()));
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
                Ok(Ok(response.metadata))
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
    ) -> Result<Result<DataResponseMetadata, DataError>, MaybePayloadError>
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

/// A trait for a [`DateTimeNamesMarker`] that can be created from a more specific one, `M`.
///
/// This trait is blanket-implemented on all [field sets](crate::fieldsets) that are more general than `M`.
///
/// # Examples
///
/// Example pairs of field sets where the trait is implemented:
///
/// ```
/// use icu::datetime::fieldsets::enums::CompositeDateTimeFieldSet;
/// use icu::datetime::fieldsets::enums::CompositeFieldSet;
/// use icu::datetime::fieldsets::enums::DateFieldSet;
/// use icu::datetime::fieldsets::enums::TimeFieldSet;
/// use icu::datetime::fieldsets::T;
/// use icu::datetime::fieldsets::YMD;
/// use icu::datetime::scaffold::DateTimeNamesFrom;
/// use icu::datetime::scaffold::DateTimeNamesMarker;
///
/// fn is_trait_implemented<Source, Target>()
/// where
///     Source: DateTimeNamesMarker,
///     Target: DateTimeNamesFrom<Source>,
/// {
/// }
///
/// is_trait_implemented::<YMD, DateFieldSet>();
/// is_trait_implemented::<YMD, CompositeDateTimeFieldSet>();
/// is_trait_implemented::<YMD, CompositeFieldSet>();
/// is_trait_implemented::<T, TimeFieldSet>();
/// is_trait_implemented::<T, CompositeDateTimeFieldSet>();
/// is_trait_implemented::<T, CompositeFieldSet>();
/// is_trait_implemented::<DateFieldSet, CompositeDateTimeFieldSet>();
/// is_trait_implemented::<DateFieldSet, CompositeFieldSet>();
/// is_trait_implemented::<TimeFieldSet, CompositeDateTimeFieldSet>();
/// is_trait_implemented::<TimeFieldSet, CompositeFieldSet>();
/// ```
#[allow(missing_docs)]
// This trait is implicitly sealed due to sealed supertraits
pub trait DateTimeNamesFrom<M: DateTimeNamesMarker>: DateTimeNamesMarker {
    fn map_year_names(
        other: <M::YearNames as NamesContainer<YearNamesV1, YearNameLength>>::Container,
    ) -> <Self::YearNames as NamesContainer<YearNamesV1, YearNameLength>>::Container;
    fn map_month_names(
        other: <M::MonthNames as NamesContainer<MonthNamesV1, MonthNameLength>>::Container,
    ) -> <Self::MonthNames as NamesContainer<MonthNamesV1, MonthNameLength>>::Container;
    fn map_weekday_names(
        other: <M::WeekdayNames as NamesContainer<WeekdayNamesV1, WeekdayNameLength>>::Container,
    ) -> <Self::WeekdayNames as NamesContainer<WeekdayNamesV1, WeekdayNameLength>>::Container;
    fn map_day_period_names(
        other: <M::DayPeriodNames as NamesContainer<DayPeriodNamesV1, DayPeriodNameLength>>::Container,
    ) -> <Self::DayPeriodNames as NamesContainer<DayPeriodNamesV1, DayPeriodNameLength>>::Container;
    fn map_zone_essentials(
        other: <M::ZoneEssentials as NamesContainer<tz::EssentialsV1, ()>>::Container,
    ) -> <Self::ZoneEssentials as NamesContainer<tz::EssentialsV1, ()>>::Container;
    fn map_zone_locations(
        other: <M::ZoneLocations as NamesContainer<tz::LocationsV1, ()>>::Container,
    ) -> <Self::ZoneLocations as NamesContainer<tz::LocationsV1, ()>>::Container;
    fn map_zone_locations_root(
        other: <M::ZoneLocationsRoot as NamesContainer<tz::LocationsRootV1, ()>>::Container,
    ) -> <Self::ZoneLocationsRoot as NamesContainer<tz::LocationsRootV1, ()>>::Container;
    fn map_zone_exemplars(
        other: <M::ZoneExemplars as NamesContainer<tz::ExemplarCitiesV1, ()>>::Container,
    ) -> <Self::ZoneExemplars as NamesContainer<tz::ExemplarCitiesV1, ()>>::Container;
    fn map_zone_exemplars_root(
        other: <M::ZoneExemplarsRoot as NamesContainer<tz::ExemplarCitiesRootV1, ()>>::Container,
    ) -> <Self::ZoneExemplarsRoot as NamesContainer<tz::ExemplarCitiesRootV1, ()>>::Container;
    fn map_zone_generic_long(
        other: <M::ZoneGenericLong as NamesContainer<tz::MzGenericLongV1, ()>>::Container,
    ) -> <Self::ZoneGenericLong as NamesContainer<tz::MzGenericLongV1, ()>>::Container;
    fn map_zone_generic_short(
        other: <M::ZoneGenericShort as NamesContainer<tz::MzGenericShortV1, ()>>::Container,
    ) -> <Self::ZoneGenericShort as NamesContainer<tz::MzGenericShortV1, ()>>::Container;
    fn map_zone_standard_long(
        other: <M::ZoneStandardLong as NamesContainer<tz::MzStandardLongV1, ()>>::Container,
    ) -> <Self::ZoneStandardLong as NamesContainer<tz::MzStandardLongV1, ()>>::Container;
    fn map_zone_specific_long(
        other: <M::ZoneSpecificLong as NamesContainer<tz::MzSpecificLongV1, ()>>::Container,
    ) -> <Self::ZoneSpecificLong as NamesContainer<tz::MzSpecificLongV1, ()>>::Container;
    fn map_zone_specific_short(
        other: <M::ZoneSpecificShort as NamesContainer<tz::MzSpecificShortV1, ()>>::Container,
    ) -> <Self::ZoneSpecificShort as NamesContainer<tz::MzSpecificShortV1, ()>>::Container;
    fn map_metazone_lookup(
        other: <M::MetazoneLookup as NamesContainer<tz::MzPeriodV1, ()>>::Container,
    ) -> <Self::MetazoneLookup as NamesContainer<tz::MzPeriodV1, ()>>::Container;
}

impl<M: DateTimeNamesMarker, T: DateTimeNamesMarker> DateTimeNamesFrom<M> for T
where
    <Self::YearNames as NamesContainer<YearNamesV1, YearNameLength>>::Container:
        From<<M::YearNames as NamesContainer<YearNamesV1, YearNameLength>>::Container>,
    <Self::MonthNames as NamesContainer<MonthNamesV1, MonthNameLength>>::Container:
        From<<M::MonthNames as NamesContainer<MonthNamesV1, MonthNameLength>>::Container>,
    <Self::WeekdayNames as NamesContainer<WeekdayNamesV1, WeekdayNameLength>>::Container:
        From<<M::WeekdayNames as NamesContainer<WeekdayNamesV1, WeekdayNameLength>>::Container>,
    <Self::DayPeriodNames as NamesContainer<DayPeriodNamesV1, DayPeriodNameLength>>::Container:
        From<
            <M::DayPeriodNames as NamesContainer<DayPeriodNamesV1, DayPeriodNameLength>>::Container,
        >,
    <Self::ZoneEssentials as NamesContainer<tz::EssentialsV1, ()>>::Container:
        From<<M::ZoneEssentials as NamesContainer<tz::EssentialsV1, ()>>::Container>,
    <Self::ZoneLocations as NamesContainer<tz::LocationsV1, ()>>::Container:
        From<<M::ZoneLocations as NamesContainer<tz::LocationsV1, ()>>::Container>,
    <Self::ZoneLocationsRoot as NamesContainer<tz::LocationsRootV1, ()>>::Container:
        From<<M::ZoneLocationsRoot as NamesContainer<tz::LocationsRootV1, ()>>::Container>,
    <Self::ZoneExemplars as NamesContainer<tz::ExemplarCitiesV1, ()>>::Container:
        From<<M::ZoneExemplars as NamesContainer<tz::ExemplarCitiesV1, ()>>::Container>,
    <Self::ZoneExemplarsRoot as NamesContainer<tz::ExemplarCitiesRootV1, ()>>::Container:
        From<<M::ZoneExemplarsRoot as NamesContainer<tz::ExemplarCitiesRootV1, ()>>::Container>,
    <Self::ZoneGenericLong as NamesContainer<tz::MzGenericLongV1, ()>>::Container:
        From<<M::ZoneGenericLong as NamesContainer<tz::MzGenericLongV1, ()>>::Container>,
    <Self::ZoneGenericShort as NamesContainer<tz::MzGenericShortV1, ()>>::Container:
        From<<M::ZoneGenericShort as NamesContainer<tz::MzGenericShortV1, ()>>::Container>,
    <Self::ZoneStandardLong as NamesContainer<tz::MzStandardLongV1, ()>>::Container:
        From<<M::ZoneStandardLong as NamesContainer<tz::MzStandardLongV1, ()>>::Container>,
    <Self::ZoneSpecificLong as NamesContainer<tz::MzSpecificLongV1, ()>>::Container:
        From<<M::ZoneSpecificLong as NamesContainer<tz::MzSpecificLongV1, ()>>::Container>,
    <Self::ZoneSpecificShort as NamesContainer<tz::MzSpecificShortV1, ()>>::Container:
        From<<M::ZoneSpecificShort as NamesContainer<tz::MzSpecificShortV1, ()>>::Container>,
    <Self::MetazoneLookup as NamesContainer<tz::MzPeriodV1, ()>>::Container:
        From<<M::MetazoneLookup as NamesContainer<tz::MzPeriodV1, ()>>::Container>,
{
    #[inline]
    fn map_year_names(
        other: <M::YearNames as NamesContainer<YearNamesV1, YearNameLength>>::Container,
    ) -> <Self::YearNames as NamesContainer<YearNamesV1, YearNameLength>>::Container {
        other.into()
    }
    #[inline]
    fn map_month_names(
        other: <M::MonthNames as NamesContainer<MonthNamesV1, MonthNameLength>>::Container,
    ) -> <Self::MonthNames as NamesContainer<MonthNamesV1, MonthNameLength>>::Container {
        other.into()
    }
    #[inline]
    fn map_weekday_names(
        other: <M::WeekdayNames as NamesContainer<WeekdayNamesV1, WeekdayNameLength>>::Container,
    ) -> <Self::WeekdayNames as NamesContainer<WeekdayNamesV1, WeekdayNameLength>>::Container {
        other.into()
    }
    #[inline]
    fn map_day_period_names(
        other: <M::DayPeriodNames as NamesContainer<DayPeriodNamesV1, DayPeriodNameLength>>::Container,
    ) -> <Self::DayPeriodNames as NamesContainer<DayPeriodNamesV1, DayPeriodNameLength>>::Container
    {
        other.into()
    }
    #[inline]
    fn map_zone_essentials(
        other: <M::ZoneEssentials as NamesContainer<tz::EssentialsV1, ()>>::Container,
    ) -> <Self::ZoneEssentials as NamesContainer<tz::EssentialsV1, ()>>::Container {
        other.into()
    }
    #[inline]
    fn map_zone_locations(
        other: <M::ZoneLocations as NamesContainer<tz::LocationsV1, ()>>::Container,
    ) -> <Self::ZoneLocations as NamesContainer<tz::LocationsV1, ()>>::Container {
        other.into()
    }
    #[inline]
    fn map_zone_locations_root(
        other: <M::ZoneLocationsRoot as NamesContainer<tz::LocationsRootV1, ()>>::Container,
    ) -> <Self::ZoneLocationsRoot as NamesContainer<tz::LocationsRootV1, ()>>::Container {
        other.into()
    }
    #[inline]
    fn map_zone_exemplars(
        other: <M::ZoneExemplars as NamesContainer<tz::ExemplarCitiesV1, ()>>::Container,
    ) -> <Self::ZoneExemplars as NamesContainer<tz::ExemplarCitiesV1, ()>>::Container {
        other.into()
    }
    #[inline]
    fn map_zone_exemplars_root(
        other: <M::ZoneExemplarsRoot as NamesContainer<tz::ExemplarCitiesRootV1, ()>>::Container,
    ) -> <Self::ZoneExemplarsRoot as NamesContainer<tz::ExemplarCitiesRootV1, ()>>::Container {
        other.into()
    }
    #[inline]
    fn map_zone_generic_long(
        other: <M::ZoneGenericLong as NamesContainer<tz::MzGenericLongV1, ()>>::Container,
    ) -> <Self::ZoneGenericLong as NamesContainer<tz::MzGenericLongV1, ()>>::Container {
        other.into()
    }
    #[inline]
    fn map_zone_generic_short(
        other: <M::ZoneGenericShort as NamesContainer<tz::MzGenericShortV1, ()>>::Container,
    ) -> <Self::ZoneGenericShort as NamesContainer<tz::MzGenericShortV1, ()>>::Container {
        other.into()
    }
    #[inline]
    fn map_zone_standard_long(
        other: <M::ZoneStandardLong as NamesContainer<tz::MzStandardLongV1, ()>>::Container,
    ) -> <Self::ZoneStandardLong as NamesContainer<tz::MzStandardLongV1, ()>>::Container {
        other.into()
    }
    #[inline]
    fn map_zone_specific_long(
        other: <M::ZoneSpecificLong as NamesContainer<tz::MzSpecificLongV1, ()>>::Container,
    ) -> <Self::ZoneSpecificLong as NamesContainer<tz::MzSpecificLongV1, ()>>::Container {
        other.into()
    }
    #[inline]
    fn map_zone_specific_short(
        other: <M::ZoneSpecificShort as NamesContainer<tz::MzSpecificShortV1, ()>>::Container,
    ) -> <Self::ZoneSpecificShort as NamesContainer<tz::MzSpecificShortV1, ()>>::Container {
        other.into()
    }
    #[inline]
    fn map_metazone_lookup(
        other: <M::MetazoneLookup as NamesContainer<tz::MzPeriodV1, ()>>::Container,
    ) -> <Self::MetazoneLookup as NamesContainer<tz::MzPeriodV1, ()>>::Container {
        other.into()
    }
}
