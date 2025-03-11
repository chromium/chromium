// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider structs for time zones.

use alloc::borrow::Cow;
use icu_pattern::{DoublePlaceholderPattern, SinglePlaceholderPattern};
use icu_provider::prelude::*;
use zerovec::{ule::NichedOption, ZeroMap, ZeroMap2d, ZeroVec};

use icu_time::{provider::MinutesSinceEpoch, zone::TimeZoneVariant, TimeZone};

/// Time zone type aliases for cleaner code
pub(crate) mod tz {
    pub(crate) use super::ExemplarCities;
    pub(crate) use super::ExemplarCitiesRootV1;
    pub(crate) use super::ExemplarCitiesV1;
    pub(crate) use super::Locations;
    pub(crate) use super::LocationsRootV1;
    pub(crate) use super::LocationsV1;
    pub(crate) use super::MetazoneGenericNames as MzGeneric;
    pub(crate) use super::MetazoneGenericNamesLongV1 as MzGenericLongV1;
    pub(crate) use super::MetazoneGenericNamesShortV1 as MzGenericShortV1;
    pub(crate) use super::MetazonePeriod as MzPeriod;
    pub(crate) use super::MetazonePeriodV1 as MzPeriodV1;
    pub(crate) use super::MetazoneSpecificNames as MzSpecific;
    pub(crate) use super::MetazoneSpecificNamesLongV1 as MzSpecificLongV1;
    pub(crate) use super::MetazoneSpecificNamesShortV1 as MzSpecificShortV1;
    pub(crate) use super::MetazoneStandardNamesLongV1 as MzStandardLongV1;
    pub(crate) use super::TimeZoneEssentials as Essentials;
    pub(crate) use super::TimeZoneEssentialsV1 as EssentialsV1;
}

icu_provider::data_marker!(
    /// `TimeZoneEssentialsV1`
    TimeZoneEssentialsV1,
    TimeZoneEssentials<'static>
);
icu_provider::data_marker!(
    /// `LocationsV1`
    LocationsV1,
    Locations<'static>
);
icu_provider::data_marker!(
    /// `LocationsRootV1`
    LocationsRootV1,
    Locations<'static>
);
icu_provider::data_marker!(
    /// `ExemplarCitiesV1`
    ExemplarCitiesV1,
    ExemplarCities<'static>
);
icu_provider::data_marker!(
    /// `ExemplarCitiesRootV1`
    ExemplarCitiesRootV1,
    ExemplarCities<'static>
);

icu_provider::data_marker!(
    /// `MetazoneGenericNamesLongV1`
    ///
    /// Checksumed to ensure consistency with [`MetazonePeriodV1`].
    MetazoneGenericNamesLongV1,
    MetazoneGenericNames<'static>,
    has_checksum = true
);
icu_provider::data_marker!(
    /// `MetazoneGenericNamesShortV1`
    ///
    /// Checksumed to ensure consistency with [`MetazonePeriodV1`].
    MetazoneGenericNamesShortV1,
    MetazoneGenericNames<'static>,
    has_checksum = true
);
icu_provider::data_marker!(
    /// `MetazoneStandardNamesLongV1`
    ///
    /// Checksumed to ensure consistency with [`MetazonePeriodV1`].
    MetazoneStandardNamesLongV1,
    MetazoneGenericNames<'static>,
    has_checksum = true
);
icu_provider::data_marker!(
    /// `MetazoneSpecificNamesLongV1`
    ///
    /// Checksumed to ensure consistency with [`MetazonePeriodV1`].
    MetazoneSpecificNamesLongV1,
    MetazoneSpecificNames<'static>,
    has_checksum = true
);
icu_provider::data_marker!(
    /// `MetazoneSpecificNamesShortV1`
    ///
    /// Checksumed to ensure consistency with [`MetazonePeriodV1`].
    MetazoneSpecificNamesShortV1,
    MetazoneSpecificNames<'static>,
    has_checksum = true,
);
icu_provider::data_marker!(
    /// `MetazonePeriodV1`
    MetazonePeriodV1,
    MetazonePeriod<'static>,
    is_singleton = true,
    has_checksum = true
);

/// An ICU4X mapping to the CLDR timeZoneNames format strings.
/// See CLDR-JSON timeZoneNames.json and <https://cldr.unicode.org/translation/time-zones-and-city-names>
/// for more context.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(PartialEq, Debug, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::time_zones))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct TimeZoneEssentials<'data> {
    /// The separator sign
    #[cfg_attr(feature = "serde", serde(borrow,))]
    pub offset_separator: Cow<'data, str>,
    /// The localized offset format.
    #[cfg_attr(
        feature = "serde",
        serde(
            borrow,
            deserialize_with = "icu_pattern::deserialize_borrowed_cow::<icu_pattern::SinglePlaceholder, _>"
        )
    )]
    pub offset_pattern: Cow<'data, SinglePlaceholderPattern>,
    /// The localized zero-offset format.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub offset_zero: Cow<'data, str>,
    /// The localized unknown-offset format.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub offset_unknown: Cow<'data, str>,
}

icu_provider::data_struct!(
    TimeZoneEssentials<'_>,
    #[cfg(feature = "datagen")]
);

/// An ICU4X mapping to the CLDR timeZoneNames exemplar cities.
/// See CLDR-JSON timeZoneNames.json for more context.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(PartialEq, Debug, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::time_zones))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct Locations<'data> {
    /// Per-zone location display name
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub locations: ZeroMap<'data, TimeZone, str>,
    /// The format string for a region's generic time.
    #[cfg_attr(
        feature = "serde",
        serde(
            borrow,
            deserialize_with = "icu_pattern::deserialize_borrowed_cow::<icu_pattern::SinglePlaceholder, _>"
        )
    )]
    pub pattern_generic: Cow<'data, SinglePlaceholderPattern>,
    /// The format string for a region's standard time.
    #[cfg_attr(
        feature = "serde",
        serde(
            borrow,
            deserialize_with = "icu_pattern::deserialize_borrowed_cow::<icu_pattern::SinglePlaceholder, _>"
        )
    )]
    pub pattern_standard: Cow<'data, SinglePlaceholderPattern>,
    /// The format string for a region's daylight time.
    #[cfg_attr(
        feature = "serde",
        serde(
            borrow,
            deserialize_with = "icu_pattern::deserialize_borrowed_cow::<icu_pattern::SinglePlaceholder, _>"
        )
    )]
    pub pattern_daylight: Cow<'data, SinglePlaceholderPattern>,
    /// Metazone Name with Location Pattern.
    #[cfg_attr(
        feature = "serde",
        serde(
            borrow,
            deserialize_with = "icu_pattern::deserialize_borrowed_cow::<icu_pattern::DoublePlaceholder, _>"
        )
    )]
    pub pattern_partial_location: Cow<'data, DoublePlaceholderPattern>,
}

icu_provider::data_struct!(
    Locations<'_>,
    #[cfg(feature = "datagen")]
);

/// An ICU4X mapping to the CLDR timeZoneNames exemplar cities.
/// See CLDR-JSON timeZoneNames.json for more context.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(PartialEq, Debug, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::time_zones))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct ExemplarCities<'data> {
    /// Per-zone exemplar city name. This is deduplicated against `Locations.locations`, so it
    /// only contains time zones that don't use the exemplar city in the location format.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub exemplars: ZeroMap<'data, TimeZone, str>,
}

icu_provider::data_struct!(
    ExemplarCities<'_>,
    #[cfg(feature = "datagen")]
);

/// An ICU4X mapping to generic metazone names.
/// See CLDR-JSON timeZoneNames.json for more context.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(PartialEq, Debug, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::time_zones))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct MetazoneGenericNames<'data> {
    /// The default mapping between metazone id and localized metazone name.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub defaults: ZeroMap<'data, MetazoneId, str>,
    /// The override mapping between timezone id and localized metazone name.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub overrides: ZeroMap<'data, TimeZone, str>,
}

icu_provider::data_struct!(
    MetazoneGenericNames<'_>,
    #[cfg(feature = "datagen")]
);

/// An ICU4X mapping to specific metazone names.
/// Specific names include time variants such as "daylight."
/// See CLDR-JSON timeZoneNames.json for more context.
///
/// These markers use a checksum to ensure consistency with [`MetazonePeriod`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(PartialEq, Debug, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::time_zones))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct MetazoneSpecificNames<'data> {
    /// The default mapping between metazone id and localized metazone name.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub defaults: ZeroMap<'data, (MetazoneId, TimeZoneVariant), str>,
    /// The override mapping between timezone id and localized metazone name.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub overrides: ZeroMap<'data, (TimeZone, TimeZoneVariant), str>,
    /// The metazones for which the standard name is in `MetazoneGenericStandardNames*V1`
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub use_standard: ZeroVec<'data, MetazoneId>,
}

icu_provider::data_struct!(
    MetazoneSpecificNames<'_>,
    #[cfg(feature = "datagen")]
);

/// Metazone ID in a compact format
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
pub type MetazoneId = core::num::NonZeroU8;

/// An ICU4X mapping to the metazones at a given period.
/// See CLDR-JSON metaZones.json for more context.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(PartialEq, Debug, Clone, Default, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_datetime::provider::time_zones))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct MetazonePeriod<'data> {
    /// The default mapping between period and offsets. The second level key is a wall-clock time encoded as
    /// [`MinutesSinceEpoch`]. It represents when the metazone started to be used.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub list: ZeroMap2d<'data, TimeZone, MinutesSinceEpoch, NichedOption<MetazoneId, 1>>,
}

icu_provider::data_struct!(
    MetazonePeriod<'_>,
    #[cfg(feature = "datagen")]
);
