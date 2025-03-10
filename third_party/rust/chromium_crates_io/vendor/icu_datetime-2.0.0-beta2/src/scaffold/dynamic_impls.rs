// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::*;
use crate::fieldsets::enums::*;
use crate::provider::{neo::*, time_zones::tz, *};
use icu_calendar::{
    types::{DayOfMonth, DayOfYearInfo, MonthInfo, Weekday, YearInfo},
    Date, Iso,
};
use icu_provider::marker::NeverMarker;
use icu_time::{
    zone::{TimeZoneVariant, UtcOffset},
    Hour, Minute, Nanosecond, Second, Time, TimeZone,
};

impl UnstableSealed for DateFieldSet {}

impl DateTimeNamesMarker for DateFieldSet {
    type YearNames = datetime_marker_helper!(@names/year, yes);
    type MonthNames = datetime_marker_helper!(@names/month, yes);
    type WeekdayNames = datetime_marker_helper!(@names/weekday, yes);
    type DayPeriodNames = datetime_marker_helper!(@names/dayperiod,);
    type ZoneEssentials = datetime_marker_helper!(@names/zone/essentials,);
    type ZoneLocations = datetime_marker_helper!(@names/zone/locations,);
    type ZoneLocationsRoot = datetime_marker_helper!(@names/zone/locations_root,);
    type ZoneExemplars = datetime_marker_helper!(@names/zone/exemplars,);
    type ZoneExemplarsRoot = datetime_marker_helper!(@names/zone/exemplars_root,);
    type ZoneGenericLong = datetime_marker_helper!(@names/zone/generic_long,);
    type ZoneGenericShort = datetime_marker_helper!(@names/zone/generic_short,);
    type ZoneStandardLong = datetime_marker_helper!(@names/zone/standard_long,);
    type ZoneSpecificLong = datetime_marker_helper!(@names/zone/specific_long,);
    type ZoneSpecificShort = datetime_marker_helper!(@names/zone/specific_short,);
    type MetazoneLookup = datetime_marker_helper!(@names/zone/metazone_periods,);
}

impl DateInputMarkers for DateFieldSet {
    type YearInput = datetime_marker_helper!(@input/year, yes);
    type MonthInput = datetime_marker_helper!(@input/month, yes);
    type DayOfMonthInput = datetime_marker_helper!(@input/day_of_month, yes);
    type DayOfYearInput = datetime_marker_helper!(@input/day_of_year, yes);
    type DayOfWeekInput = datetime_marker_helper!(@input/day_of_week, yes);
}

impl<C: CldrCalendar> TypedDateDataMarkers<C> for DateFieldSet {
    type DateSkeletonPatternsV1 = datetime_marker_helper!(@dates/typed, yes);
    type YearNamesV1 = datetime_marker_helper!(@years/typed, yes);
    type MonthNamesV1 = datetime_marker_helper!(@months/typed, yes);
    type WeekdayNamesV1 = datetime_marker_helper!(@weekdays, yes);
}

impl DateDataMarkers for DateFieldSet {
    type Skel = datetime_marker_helper!(@calmarkers, yes);
    type Year = datetime_marker_helper!(@calmarkers, yes);
    type Month = datetime_marker_helper!(@calmarkers, yes);
    type WeekdayNamesV1 = datetime_marker_helper!(@weekdays, yes);
}

impl DateTimeMarkers for DateFieldSet {
    type D = Self;
    type T = ();
    type Z = ();
    type GluePatternV1 = datetime_marker_helper!(@glue,);
}

impl UnstableSealed for CalendarPeriodFieldSet {}

impl DateTimeNamesMarker for CalendarPeriodFieldSet {
    type YearNames = datetime_marker_helper!(@names/year, yes);
    type MonthNames = datetime_marker_helper!(@names/month, yes);
    type WeekdayNames = datetime_marker_helper!(@names/weekday,);
    type DayPeriodNames = datetime_marker_helper!(@names/dayperiod,);
    type ZoneEssentials = datetime_marker_helper!(@names/zone/essentials,);
    type ZoneLocations = datetime_marker_helper!(@names/zone/locations,);
    type ZoneLocationsRoot = datetime_marker_helper!(@names/zone/locations_root,);
    type ZoneExemplars = datetime_marker_helper!(@names/zone/exemplars,);
    type ZoneExemplarsRoot = datetime_marker_helper!(@names/zone/exemplars_root,);
    type ZoneGenericLong = datetime_marker_helper!(@names/zone/generic_long,);
    type ZoneGenericShort = datetime_marker_helper!(@names/zone/generic_short,);
    type ZoneStandardLong = datetime_marker_helper!(@names/zone/standard_long,);
    type ZoneSpecificLong = datetime_marker_helper!(@names/zone/specific_long,);
    type ZoneSpecificShort = datetime_marker_helper!(@names/zone/specific_short,);
    type MetazoneLookup = datetime_marker_helper!(@names/zone/metazone_periods,);
}

impl DateInputMarkers for CalendarPeriodFieldSet {
    type YearInput = datetime_marker_helper!(@input/year, yes);
    type MonthInput = datetime_marker_helper!(@input/month, yes);
    type DayOfMonthInput = datetime_marker_helper!(@input/day_of_month,);
    type DayOfWeekInput = datetime_marker_helper!(@input/day_of_week,);
    type DayOfYearInput = datetime_marker_helper!(@input/day_of_year,);
}

impl<C: CldrCalendar> TypedDateDataMarkers<C> for CalendarPeriodFieldSet {
    type DateSkeletonPatternsV1 = datetime_marker_helper!(@dates/typed, yes);
    type YearNamesV1 = datetime_marker_helper!(@years/typed, yes);
    type MonthNamesV1 = datetime_marker_helper!(@months/typed, yes);
    type WeekdayNamesV1 = datetime_marker_helper!(@weekdays,);
}

impl DateDataMarkers for CalendarPeriodFieldSet {
    type Skel = datetime_marker_helper!(@calmarkers, yes);
    type Year = datetime_marker_helper!(@calmarkers, yes);
    type Month = datetime_marker_helper!(@calmarkers, yes);
    type WeekdayNamesV1 = datetime_marker_helper!(@weekdays,);
}

impl DateTimeMarkers for CalendarPeriodFieldSet {
    type D = Self;
    type T = ();
    type Z = ();
    type GluePatternV1 = datetime_marker_helper!(@glue,);
}

impl UnstableSealed for TimeFieldSet {}

impl DateTimeNamesMarker for TimeFieldSet {
    type YearNames = datetime_marker_helper!(@names/year,);
    type MonthNames = datetime_marker_helper!(@names/month,);
    type WeekdayNames = datetime_marker_helper!(@names/weekday,);
    type DayPeriodNames = datetime_marker_helper!(@names/dayperiod, yes);
    type ZoneEssentials = datetime_marker_helper!(@names/zone/essentials,);
    type ZoneLocations = datetime_marker_helper!(@names/zone/locations,);
    type ZoneLocationsRoot = datetime_marker_helper!(@names/zone/locations_root,);
    type ZoneExemplars = datetime_marker_helper!(@names/zone/exemplars,);
    type ZoneExemplarsRoot = datetime_marker_helper!(@names/zone/exemplars_root,);
    type ZoneGenericLong = datetime_marker_helper!(@names/zone/generic_long,);
    type ZoneGenericShort = datetime_marker_helper!(@names/zone/generic_short,);
    type ZoneStandardLong = datetime_marker_helper!(@names/zone/standard_long,);
    type ZoneSpecificLong = datetime_marker_helper!(@names/zone/specific_long,);
    type ZoneSpecificShort = datetime_marker_helper!(@names/zone/specific_short,);
    type MetazoneLookup = datetime_marker_helper!(@names/zone/metazone_periods,);
}

impl TimeMarkers for TimeFieldSet {
    type DayPeriodNamesV1 = datetime_marker_helper!(@dayperiods, yes);
    type TimeSkeletonPatternsV1 = datetime_marker_helper!(@times, yes);
    type HourInput = datetime_marker_helper!(@input/hour, yes);
    type MinuteInput = datetime_marker_helper!(@input/minute, yes);
    type SecondInput = datetime_marker_helper!(@input/second, yes);
    type NanosecondInput = datetime_marker_helper!(@input/Nanosecond, yes);
}

impl DateTimeMarkers for TimeFieldSet {
    type D = ();
    type T = Self;
    type Z = ();
    type GluePatternV1 = datetime_marker_helper!(@glue,);
}

impl UnstableSealed for DateAndTimeFieldSet {}

impl UnstableSealed for ZoneFieldSet {}

impl DateTimeNamesMarker for ZoneFieldSet {
    type YearNames = datetime_marker_helper!(@names/year,);
    type MonthNames = datetime_marker_helper!(@names/month,);
    type WeekdayNames = datetime_marker_helper!(@names/weekday,);
    type DayPeriodNames = datetime_marker_helper!(@names/dayperiod,);
    type ZoneEssentials = datetime_marker_helper!(@names/zone/essentials, yes);
    type ZoneLocations = datetime_marker_helper!(@names/zone/locations, yes);
    type ZoneLocationsRoot = datetime_marker_helper!(@names/zone/locations_root, yes);
    type ZoneExemplars = datetime_marker_helper!(@names/zone/exemplars, yes);
    type ZoneExemplarsRoot = datetime_marker_helper!(@names/zone/exemplars_root, yes);
    type ZoneGenericLong = datetime_marker_helper!(@names/zone/generic_long, yes);
    type ZoneStandardLong = datetime_marker_helper!(@names/zone/standard_long, yes);
    type ZoneGenericShort = datetime_marker_helper!(@names/zone/generic_short, yes);
    type ZoneSpecificLong = datetime_marker_helper!(@names/zone/specific_long, yes);
    type ZoneSpecificShort = datetime_marker_helper!(@names/zone/specific_short, yes);
    type MetazoneLookup = datetime_marker_helper!(@names/zone/metazone_periods, yes);
}

impl ZoneMarkers for ZoneFieldSet {
    type TimeZoneIdInput = datetime_marker_helper!(@input/timezone/id, yes);
    type TimeZoneOffsetInput = datetime_marker_helper!(@input/timezone/offset, yes);
    type TimeZoneVariantInput = datetime_marker_helper!(@input/timezone/variant, yes);
    type TimeZoneLocalTimeInput = datetime_marker_helper!(@input/timezone/local_time, yes);
    type EssentialsV1 = datetime_marker_helper!(@data/zone/essentials, yes);
    type LocationsV1 = datetime_marker_helper!(@data/zone/locations, yes);
    type LocationsRootV1 = datetime_marker_helper!(@data/zone/locations_root, yes);
    type ExemplarCitiesV1 = datetime_marker_helper!(@data/zone/exemplars, yes);
    type ExemplarCitiesRootV1 = datetime_marker_helper!(@data/zone/exemplars_root, yes);
    type GenericLongV1 = datetime_marker_helper!(@data/zone/generic_long, yes);
    type GenericShortV1 = datetime_marker_helper!(@data/zone/generic_short, yes);
    type StandardLongV1 = datetime_marker_helper!(@data/zone/standard_long, yes);
    type SpecificLongV1 = datetime_marker_helper!(@data/zone/specific_long, yes);
    type SpecificShortV1 = datetime_marker_helper!(@data/zone/specific_short, yes);
    type MetazonePeriodV1 = datetime_marker_helper!(@data/zone/metazone_periods, yes);
}

impl DateTimeMarkers for ZoneFieldSet {
    type D = ();
    type T = ();
    type Z = Self;
    type GluePatternV1 = datetime_marker_helper!(@glue,);
}

impl UnstableSealed for CompositeDateTimeFieldSet {}

impl DateTimeNamesMarker for CompositeDateTimeFieldSet {
    type YearNames = datetime_marker_helper!(@names/year, yes);
    type MonthNames = datetime_marker_helper!(@names/month, yes);
    type WeekdayNames = datetime_marker_helper!(@names/weekday, yes);
    type DayPeriodNames = datetime_marker_helper!(@names/dayperiod, yes);
    type ZoneEssentials = datetime_marker_helper!(@names/zone/essentials,);
    type ZoneLocations = datetime_marker_helper!(@names/zone/locations,);
    type ZoneLocationsRoot = datetime_marker_helper!(@names/zone/locations_root,);
    type ZoneExemplars = datetime_marker_helper!(@names/zone/exemplars,);
    type ZoneExemplarsRoot = datetime_marker_helper!(@names/zone/exemplars_root,);
    type ZoneGenericLong = datetime_marker_helper!(@names/zone/generic_long,);
    type ZoneGenericShort = datetime_marker_helper!(@names/zone/generic_short,);
    type ZoneStandardLong = datetime_marker_helper!(@names/zone/standard_long,);
    type ZoneSpecificLong = datetime_marker_helper!(@names/zone/specific_long,);
    type ZoneSpecificShort = datetime_marker_helper!(@names/zone/specific_short,);
    type MetazoneLookup = datetime_marker_helper!(@names/zone/metazone_periods,);
}

impl DateTimeMarkers for CompositeDateTimeFieldSet {
    type D = DateFieldSet;
    type T = TimeFieldSet;
    type Z = ();
    type GluePatternV1 = datetime_marker_helper!(@glue, yes);
}

impl UnstableSealed for CompositeFieldSet {}

impl DateTimeNamesMarker for CompositeFieldSet {
    type YearNames = datetime_marker_helper!(@names/year, yes);
    type MonthNames = datetime_marker_helper!(@names/month, yes);
    type WeekdayNames = datetime_marker_helper!(@names/weekday, yes);
    type DayPeriodNames = datetime_marker_helper!(@names/dayperiod, yes);
    type ZoneEssentials = datetime_marker_helper!(@names/zone/essentials, yes);
    type ZoneLocations = datetime_marker_helper!(@names/zone/locations, yes);
    type ZoneLocationsRoot = datetime_marker_helper!(@names/zone/locations_root, yes);
    type ZoneExemplars = datetime_marker_helper!(@names/zone/exemplars, yes);
    type ZoneExemplarsRoot = datetime_marker_helper!(@names/zone/exemplars_root, yes);
    type ZoneGenericLong = datetime_marker_helper!(@names/zone/generic_long, yes);
    type ZoneGenericShort = datetime_marker_helper!(@names/zone/generic_short, yes);
    type ZoneStandardLong = datetime_marker_helper!(@names/zone/standard_long, yes);
    type ZoneSpecificLong = datetime_marker_helper!(@names/zone/specific_long, yes);
    type ZoneSpecificShort = datetime_marker_helper!(@names/zone/specific_short, yes);
    type MetazoneLookup = datetime_marker_helper!(@names/zone/metazone_periods, yes);
}

impl DateTimeMarkers for CompositeFieldSet {
    type D = DateFieldSet;
    type T = TimeFieldSet;
    type Z = ZoneFieldSet;
    type GluePatternV1 = datetime_marker_helper!(@glue, yes);
}
