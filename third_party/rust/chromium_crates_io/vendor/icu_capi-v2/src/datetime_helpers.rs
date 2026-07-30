// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![allow(dead_code)] // features in here are a mess

use alloc::boxed::Box;
use icu_calendar::Gregorian;
use icu_datetime::{
    fieldsets::builder::BuilderError, fieldsets::enums::*, fieldsets::Combo, pattern::*,
    scaffold::*, DateTimeFormatter, DateTimeFormatterLoadError, FixedCalendarDateTimeFormatter,
};

pub(crate) fn map_or_default<Input, Output>(input: Option<Input>) -> Output
where
    Output: From<Input> + Default,
{
    input.map(Output::from).unwrap_or_default()
}

pub(super) fn date_formatter_with_zone<Zone>(
    formatter: &DateTimeFormatter<DateFieldSet>,
    locale: &crate::unstable::locale_core::ffi::Locale,
    zone: Zone,
    load: impl FnOnce(
        &mut DateTimeNames<Combo<DateFieldSet, Zone>>,
    ) -> Result<(), crate::unstable::errors::ffi::DateTimeFormatterLoadError>,
    to_formatter: impl FnOnce(
        DateTimeNames<Combo<DateFieldSet, Zone>>,
        Combo<DateFieldSet, Zone>,
    ) -> Result<
        DateTimeFormatter<Combo<DateFieldSet, Zone>>,
        (
            DateTimeFormatterLoadError,
            DateTimeNames<Combo<DateFieldSet, Zone>>,
        ),
    >,
) -> Result<
    Box<crate::unstable::zoned_date_formatter::ffi::ZonedDateFormatter>,
    crate::unstable::errors::ffi::DateTimeFormatterLoadError,
>
where
    Zone: DateTimeMarkers + ZoneMarkers,
    <Zone as DateTimeMarkers>::Z: ZoneMarkers,
    Combo<DateFieldSet, Zone>: DateTimeNamesFrom<DateFieldSet>,
    ZonedDateFieldSet: DateTimeNamesFrom<Combo<DateFieldSet, Zone>>,
{
    let prefs = (&locale.0).into();
    let mut names = DateTimeNames::from_formatter(prefs, formatter.clone())
        .cast_into_fset::<Combo<DateFieldSet, Zone>>();
    load(&mut names)?;
    let field_set = formatter
        .to_field_set_builder()
        .build_date()
        .map_err(|e| match e {
            BuilderError::InvalidDateFields => {
                // This can fail if the date fields are for a calendar period
                crate::unstable::errors::ffi::DateTimeFormatterLoadError::InvalidDateFields
            }
            _ => {
                debug_assert!(false, "should be infallible, but got: {e:?}");
                crate::unstable::errors::ffi::DateTimeFormatterLoadError::Unknown
            }
        })?
        .with_zone(zone);
    let formatter = to_formatter(names, field_set)
        // This can fail if the locale doesn't match and the fields conflict
        .map_err(|(e, _)| e)?
        .cast_into_fset();
    Ok(Box::new(
        crate::unstable::zoned_date_formatter::ffi::ZonedDateFormatter(formatter),
    ))
}

pub(super) fn datetime_formatter_with_zone<Zone>(
    formatter: &DateTimeFormatter<CompositeDateTimeFieldSet>,
    locale: &crate::unstable::locale_core::ffi::Locale,
    zone: Zone,
    load: impl FnOnce(
        &mut DateTimeNames<Combo<DateAndTimeFieldSet, Zone>>,
    ) -> Result<(), crate::unstable::errors::ffi::DateTimeFormatterLoadError>,
    to_formatter: impl FnOnce(
        DateTimeNames<Combo<DateAndTimeFieldSet, Zone>>,
        Combo<DateAndTimeFieldSet, Zone>,
    ) -> Result<
        DateTimeFormatter<Combo<DateAndTimeFieldSet, Zone>>,
        (
            DateTimeFormatterLoadError,
            DateTimeNames<Combo<DateAndTimeFieldSet, Zone>>,
        ),
    >,
) -> Result<
    Box<crate::unstable::zoned_date_time_formatter::ffi::ZonedDateTimeFormatter>,
    crate::unstable::errors::ffi::DateTimeFormatterLoadError,
>
where
    Zone: DateTimeMarkers + ZoneMarkers,
    <Zone as DateTimeMarkers>::Z: ZoneMarkers,
    Combo<DateAndTimeFieldSet, Zone>: DateTimeNamesFrom<CompositeDateTimeFieldSet>,
    ZonedDateAndTimeFieldSet: DateTimeNamesFrom<Combo<DateAndTimeFieldSet, Zone>>,
{
    let prefs = (&locale.0).into();
    let mut names = DateTimeNames::from_formatter(prefs, formatter.clone())
        .cast_into_fset::<Combo<DateAndTimeFieldSet, Zone>>();
    load(&mut names)?;
    let field_set = formatter
        .to_field_set_builder()
        .build_date_and_time()
        .map_err(|e| match e {
            BuilderError::InvalidDateFields => {
                debug_assert!(false, "fields were already validated in DateTimeFormatter");
                crate::unstable::errors::ffi::DateTimeFormatterLoadError::InvalidDateFields
            }
            _ => {
                debug_assert!(false, "should be infallible, but got: {e:?}");
                crate::unstable::errors::ffi::DateTimeFormatterLoadError::Unknown
            }
        })?
        .with_zone(zone);
    let formatter = to_formatter(names, field_set)
        // This can fail if the locale doesn't match and the fields conflict
        .map_err(|(e, _)| e)?
        .cast_into_fset();
    Ok(Box::new(
        crate::unstable::zoned_date_time_formatter::ffi::ZonedDateTimeFormatter(formatter),
    ))
}

pub(super) fn date_formatter_gregorian_with_zone<Zone>(
    formatter: &FixedCalendarDateTimeFormatter<Gregorian, DateFieldSet>,
    locale: &crate::unstable::locale_core::ffi::Locale,
    zone: Zone,
    load: impl FnOnce(
        &mut FixedCalendarDateTimeNames<Gregorian, Combo<DateFieldSet, Zone>>,
    ) -> Result<(), crate::unstable::errors::ffi::DateTimeFormatterLoadError>,
    to_formatter: impl FnOnce(
        FixedCalendarDateTimeNames<Gregorian, Combo<DateFieldSet, Zone>>,
        Combo<DateFieldSet, Zone>,
    ) -> Result<
        FixedCalendarDateTimeFormatter<Gregorian, Combo<DateFieldSet, Zone>>,
        (
            DateTimeFormatterLoadError,
            FixedCalendarDateTimeNames<Gregorian, Combo<DateFieldSet, Zone>>,
        ),
    >,
) -> Result<
    Box<crate::unstable::zoned_date_formatter::ffi::ZonedDateFormatterGregorian>,
    crate::unstable::errors::ffi::DateTimeFormatterLoadError,
>
where
    Zone: DateTimeMarkers + ZoneMarkers,
    <Zone as DateTimeMarkers>::Z: ZoneMarkers,
    Combo<DateFieldSet, Zone>: DateTimeNamesFrom<DateFieldSet>,
    ZonedDateFieldSet: DateTimeNamesFrom<Combo<DateFieldSet, Zone>>,
{
    let prefs = (&locale.0).into();
    let mut names = FixedCalendarDateTimeNames::from_formatter(prefs, formatter.clone())
        .cast_into_fset::<Combo<DateFieldSet, Zone>>();
    load(&mut names)?;
    let field_set = formatter
        .to_field_set_builder()
        .build_date()
        .map_err(|e| match e {
            BuilderError::InvalidDateFields => {
                // This can fail if the date fields are for a calendar period
                crate::unstable::errors::ffi::DateTimeFormatterLoadError::InvalidDateFields
            }
            _ => {
                debug_assert!(false, "should be infallible, but got: {e:?}");
                crate::unstable::errors::ffi::DateTimeFormatterLoadError::Unknown
            }
        })?
        .with_zone(zone);
    let formatter = to_formatter(names, field_set)
        // This can fail if the locale doesn't match and the fields conflict
        .map_err(|(e, _)| e)?
        .cast_into_fset();
    Ok(Box::new(
        crate::unstable::zoned_date_formatter::ffi::ZonedDateFormatterGregorian(formatter),
    ))
}

pub(super) fn datetime_formatter_gregorian_with_zone<Zone>(
    formatter: &FixedCalendarDateTimeFormatter<Gregorian, CompositeDateTimeFieldSet>,
    locale: &crate::unstable::locale_core::ffi::Locale,
    zone: Zone,
    load: impl FnOnce(
        &mut FixedCalendarDateTimeNames<Gregorian, Combo<DateAndTimeFieldSet, Zone>>,
    ) -> Result<(), crate::unstable::errors::ffi::DateTimeFormatterLoadError>,
    to_formatter: impl FnOnce(
        FixedCalendarDateTimeNames<Gregorian, Combo<DateAndTimeFieldSet, Zone>>,
        Combo<DateAndTimeFieldSet, Zone>,
    ) -> Result<
        FixedCalendarDateTimeFormatter<Gregorian, Combo<DateAndTimeFieldSet, Zone>>,
        (
            DateTimeFormatterLoadError,
            FixedCalendarDateTimeNames<Gregorian, Combo<DateAndTimeFieldSet, Zone>>,
        ),
    >,
) -> Result<
    Box<crate::unstable::zoned_date_time_formatter::ffi::ZonedDateTimeFormatterGregorian>,
    crate::unstable::errors::ffi::DateTimeFormatterLoadError,
>
where
    Zone: DateTimeMarkers + ZoneMarkers,
    <Zone as DateTimeMarkers>::Z: ZoneMarkers,
    Combo<DateAndTimeFieldSet, Zone>: DateTimeNamesFrom<CompositeDateTimeFieldSet>,
    ZonedDateAndTimeFieldSet: DateTimeNamesFrom<Combo<DateAndTimeFieldSet, Zone>>,
{
    let prefs = (&locale.0).into();
    let mut names = FixedCalendarDateTimeNames::from_formatter(prefs, formatter.clone())
        .cast_into_fset::<Combo<DateAndTimeFieldSet, Zone>>();
    load(&mut names)?;
    let field_set = formatter
        .to_field_set_builder()
        .build_date_and_time()
        .map_err(|e| match e {
            BuilderError::InvalidDateFields => {
                debug_assert!(false, "fields were already validated in DateTimeFormatter");
                crate::unstable::errors::ffi::DateTimeFormatterLoadError::InvalidDateFields
            }
            _ => {
                debug_assert!(false, "should be infallible, but got: {e:?}");
                crate::unstable::errors::ffi::DateTimeFormatterLoadError::Unknown
            }
        })?
        .with_zone(zone);
    let formatter = to_formatter(names, field_set)
        // This can fail if the locale doesn't match and the fields conflict
        .map_err(|(e, _)| e)?
        .cast_into_fset();
    Ok(Box::new(
        crate::unstable::zoned_date_time_formatter::ffi::ZonedDateTimeFormatterGregorian(formatter),
    ))
}
