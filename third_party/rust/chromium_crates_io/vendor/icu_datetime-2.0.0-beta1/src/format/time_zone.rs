// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! A formatter specifically for the time zone.

use crate::pattern::TimeZoneDataPayloadsBorrowed;
use crate::provider::time_zones::MetazoneId;
use crate::{fields::FieldLength, input::ExtractedInput};
use core::fmt;
use fixed_decimal::FixedDecimal;
use icu_calendar::{Date, Iso, Time};
use icu_decimal::FixedDecimalFormatter;
use icu_timezone::provider::EPOCH;
use icu_timezone::{TimeZoneBcp47Id, UtcOffset, ZoneVariant};
use writeable::Writeable;

impl crate::provider::time_zones::MetazonePeriodV1<'_> {
    fn resolve(
        &self,
        time_zone_id: TimeZoneBcp47Id,
        (date, time): (Date<Iso>, Time),
    ) -> Option<MetazoneId> {
        let cursor = self.0.get0(&time_zone_id)?;
        let mut metazone_id = None;
        let minutes_since_epoch_walltime = (date.to_fixed() - EPOCH) as i32 * 24 * 60
            + (time.hour.number() as i32 * 60 + time.minute.number() as i32);
        for (minutes, id) in cursor.iter1() {
            if minutes_since_epoch_walltime
                >= <i32 as zerovec::ule::AsULE>::from_unaligned(*minutes)
            {
                metazone_id = id.get()
            } else {
                break;
            }
        }
        metazone_id
    }
}

// An enum for time zone format unit.
#[derive(Debug, Clone, Copy, PartialEq)]
pub(super) enum TimeZoneFormatterUnit {
    GenericNonLocation(FieldLength),
    SpecificNonLocation(FieldLength),
    GenericLocation,
    SpecificLocation,
    #[allow(dead_code)]
    GenericPartialLocation(FieldLength),
    LocalizedOffset(FieldLength),
    Iso8601(Iso8601Format),
    Bcp47Id,
}

#[derive(Debug)]
pub(super) enum FormatTimeZoneError {
    NamesNotLoaded,
    FixedDecimalFormatterNotLoaded,
    Fallback,
    MissingInputField(&'static str),
}

pub(super) trait FormatTimeZone {
    /// Tries to write the timezone to the sink. If a DateTimeError is returned, the sink
    /// has not been touched, so another format can be attempted.
    fn format<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        input: &ExtractedInput,
        data_payloads: TimeZoneDataPayloadsBorrowed,
        fdf: Option<&FixedDecimalFormatter>,
    ) -> Result<Result<(), FormatTimeZoneError>, fmt::Error>;
}

impl FormatTimeZone for TimeZoneFormatterUnit {
    fn format<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        input: &ExtractedInput,
        data_payloads: TimeZoneDataPayloadsBorrowed,
        fdf: Option<&FixedDecimalFormatter>,
    ) -> Result<Result<(), FormatTimeZoneError>, fmt::Error> {
        match *self {
            Self::GenericNonLocation(length) => {
                GenericNonLocationFormat(length).format(sink, input, data_payloads, fdf)
            }
            Self::SpecificNonLocation(length) => {
                SpecificNonLocationFormat(length).format(sink, input, data_payloads, fdf)
            }
            Self::GenericLocation => GenericLocationFormat.format(sink, input, data_payloads, fdf),
            Self::SpecificLocation => {
                SpecificLocationFormat.format(sink, input, data_payloads, fdf)
            }
            Self::GenericPartialLocation(length) => {
                GenericPartialLocationFormat(length).format(sink, input, data_payloads, fdf)
            }
            Self::LocalizedOffset(length) => {
                LocalizedOffsetFormat(length).format(sink, input, data_payloads, fdf)
            }
            Self::Iso8601(iso) => iso.format(sink, input, data_payloads, fdf),
            Self::Bcp47Id => Bcp47IdFormat.format(sink, input, data_payloads, fdf),
        }
    }
}

// PT / Pacific Time
struct GenericNonLocationFormat(FieldLength);

impl FormatTimeZone for GenericNonLocationFormat {
    /// Writes the time zone in generic non-location format as defined by the UTS-35 spec.
    /// <https://unicode.org/reports/tr35/tr35-dates.html#Time_Zone_Format_Terminology>
    fn format<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        input: &ExtractedInput,
        data_payloads: TimeZoneDataPayloadsBorrowed,
        _fdf: Option<&FixedDecimalFormatter>,
    ) -> Result<Result<(), FormatTimeZoneError>, fmt::Error> {
        let Some(time_zone_id) = input.time_zone_id else {
            return Ok(Err(FormatTimeZoneError::MissingInputField("time_zone_id")));
        };
        let Some(local_time) = input.local_time else {
            return Ok(Err(FormatTimeZoneError::MissingInputField("local_time")));
        };
        let Some(names) = (match self.0 {
            FieldLength::Four => data_payloads.mz_generic_long.as_ref(),
            _ => data_payloads.mz_generic_short.as_ref(),
        }) else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };
        let Some(metazone_period) = data_payloads.mz_periods else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };

        let Some(name) = names.overrides.get(&time_zone_id).or_else(|| {
            names
                .defaults
                .get(&metazone_period.resolve(time_zone_id, local_time)?)
        }) else {
            return Ok(Err(FormatTimeZoneError::Fallback));
        };

        sink.write_str(name)?;

        Ok(Ok(()))
    }
}

// PDT / Pacific Daylight Time
struct SpecificNonLocationFormat(FieldLength);

impl FormatTimeZone for SpecificNonLocationFormat {
    /// Writes the time zone in short specific non-location format as defined by the UTS-35 spec.
    /// <https://unicode.org/reports/tr35/tr35-dates.html#Time_Zone_Format_Terminology>
    fn format<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        input: &ExtractedInput,
        data_payloads: TimeZoneDataPayloadsBorrowed,
        _fdf: Option<&FixedDecimalFormatter>,
    ) -> Result<Result<(), FormatTimeZoneError>, fmt::Error> {
        let Some(time_zone_id) = input.time_zone_id else {
            return Ok(Err(FormatTimeZoneError::MissingInputField("time_zone_id")));
        };
        let Some(zone_variant) = input.zone_variant else {
            return Ok(Err(FormatTimeZoneError::MissingInputField("zone_variant")));
        };
        let Some(local_time) = input.local_time else {
            return Ok(Err(FormatTimeZoneError::MissingInputField("local_time")));
        };

        let Some(names) = (match self.0 {
            FieldLength::Four => data_payloads.mz_specific_long.as_ref(),
            _ => data_payloads.mz_specific_short.as_ref(),
        }) else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };
        let Some(metazone_period) = data_payloads.mz_periods else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };

        let Some(name) = names
            .overrides
            .get(&(time_zone_id, zone_variant))
            .or_else(|| {
                names.defaults.get(&(
                    metazone_period.resolve(time_zone_id, local_time)?,
                    zone_variant,
                ))
            })
        else {
            return Ok(Err(FormatTimeZoneError::Fallback));
        };

        sink.write_str(name)?;

        Ok(Ok(()))
    }
}

// UTC+7:00
struct LocalizedOffsetFormat(FieldLength);

impl FormatTimeZone for LocalizedOffsetFormat {
    /// Writes the time zone in localized offset format according to the CLDR localized hour format.
    /// This goes explicitly against the UTS-35 spec, which specifies long or short localized
    /// offset formats regardless of locale.
    ///
    /// You can see more information about our decision to resolve this conflict here:
    /// <https://docs.google.com/document/d/16GAqaDRS6hzL8jNYjus5MglSevGBflISM-BrIS7bd4A/edit?usp=sharing>
    fn format<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        input: &ExtractedInput,
        data_payloads: TimeZoneDataPayloadsBorrowed,
        fdf: Option<&FixedDecimalFormatter>,
    ) -> Result<Result<(), FormatTimeZoneError>, fmt::Error> {
        let Some(essentials) = data_payloads.essentials else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };
        let Some(fdf) = fdf else {
            return Ok(Err(FormatTimeZoneError::FixedDecimalFormatterNotLoaded));
        };
        let Some(offset) = input.offset else {
            sink.write_str(&essentials.offset_unknown)?;
            return Ok(Ok(()));
        };
        Ok(if offset.is_zero() {
            sink.write_str(&essentials.offset_zero)?;
            Ok(())
        } else {
            struct FormattedOffset<'a> {
                offset: UtcOffset,
                separator: &'a str,
                fdf: &'a FixedDecimalFormatter,
                length: FieldLength,
            }

            impl Writeable for FormattedOffset<'_> {
                fn write_to_parts<S: writeable::PartsWrite + ?Sized>(
                    &self,
                    sink: &mut S,
                ) -> fmt::Result {
                    self.fdf
                        .format(
                            &FixedDecimal::from(self.offset.hours_part())
                                .with_sign_display(fixed_decimal::SignDisplay::Always)
                                .padded_start(if self.length == FieldLength::Four {
                                    2
                                } else {
                                    0
                                }),
                        )
                        .write_to(sink)?;

                    if self.length == FieldLength::Four
                        || self.offset.minutes_part() != 0
                        || self.offset.seconds_part() != 0
                    {
                        sink.write_str(self.separator)?;
                        self.fdf
                            .format(&FixedDecimal::from(self.offset.minutes_part()).padded_start(2))
                            .write_to(sink)?;
                    }

                    if self.offset.seconds_part() != 0 {
                        sink.write_str(self.separator)?;
                        self.fdf
                            .format(&FixedDecimal::from(self.offset.seconds_part()).padded_start(2))
                            .write_to(sink)?;
                    }

                    Ok(())
                }
            }

            essentials
                .offset_pattern
                .interpolate([FormattedOffset {
                    offset,
                    separator: &essentials.offset_separator,
                    fdf,
                    length: self.0,
                }])
                .write_to(sink)?;

            Ok(())
        })
    }
}

// Los Angeles Time
struct GenericLocationFormat;

impl FormatTimeZone for GenericLocationFormat {
    /// Writes the time zone in generic location format as defined by the UTS-35 spec.
    /// e.g. France Time
    /// <https://unicode.org/reports/tr35/tr35-dates.html#Time_Zone_Format_Terminology>
    fn format<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        input: &ExtractedInput,
        data_payloads: TimeZoneDataPayloadsBorrowed,
        _fdf: Option<&FixedDecimalFormatter>,
    ) -> Result<Result<(), FormatTimeZoneError>, fmt::Error> {
        let Some(time_zone_id) = input.time_zone_id else {
            return Ok(Err(FormatTimeZoneError::MissingInputField("time_zone_id")));
        };

        let Some(locations) = data_payloads.locations else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };

        let Some(locations_root) = data_payloads.locations_root else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };

        let Some(location) = locations
            .locations
            .get(&time_zone_id)
            .or_else(|| locations_root.locations.get(&time_zone_id))
        else {
            return Ok(Err(FormatTimeZoneError::Fallback));
        };

        locations
            .pattern_generic
            .interpolate([location])
            .write_to(sink)?;

        Ok(Ok(()))
    }
}

// Los Angeles Daylight Time
struct SpecificLocationFormat;

impl FormatTimeZone for SpecificLocationFormat {
    /// Writes the time zone in a specific location format as defined by the UTS-35 spec.
    /// e.g. France Time
    /// <https://unicode.org/reports/tr35/tr35-dates.html#Time_Zone_Format_Terminology>
    fn format<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        input: &ExtractedInput,
        data_payloads: TimeZoneDataPayloadsBorrowed,
        _fdf: Option<&FixedDecimalFormatter>,
    ) -> Result<Result<(), FormatTimeZoneError>, fmt::Error> {
        let Some(time_zone_id) = input.time_zone_id else {
            return Ok(Err(FormatTimeZoneError::MissingInputField("time_zone_id")));
        };
        let Some(zone_variant) = input.zone_variant else {
            return Ok(Err(FormatTimeZoneError::MissingInputField("zone_variant")));
        };
        let Some(locations) = data_payloads.locations else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };
        let Some(locations_root) = data_payloads.locations_root else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };

        let Some(location) = locations
            .locations
            .get(&time_zone_id)
            .or_else(|| locations_root.locations.get(&time_zone_id))
        else {
            return Ok(Err(FormatTimeZoneError::Fallback));
        };

        match zone_variant {
            ZoneVariant::Standard => &locations.pattern_standard,
            ZoneVariant::Daylight => &locations.pattern_daylight,
            // Compiles out due to tilde dependency on `icu_timezone`
            _ => unreachable!(),
        }
        .interpolate([location])
        .write_to(sink)?;

        Ok(Ok(()))
    }
}

// Pacific Time (Los Angeles) / PT (Los Angeles)
struct GenericPartialLocationFormat(FieldLength);

impl FormatTimeZone for GenericPartialLocationFormat {
    /// Writes the time zone in a long generic partial location format as defined by the UTS-35 spec.
    /// <https://unicode.org/reports/tr35/tr35-dates.html#Time_Zone_Format_Terminology>
    fn format<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        input: &ExtractedInput,
        data_payloads: TimeZoneDataPayloadsBorrowed,
        _fdf: Option<&FixedDecimalFormatter>,
    ) -> Result<Result<(), FormatTimeZoneError>, fmt::Error> {
        let Some(time_zone_id) = input.time_zone_id else {
            return Ok(Err(FormatTimeZoneError::MissingInputField("time_zone_id")));
        };
        let Some(local_time) = input.local_time else {
            return Ok(Err(FormatTimeZoneError::MissingInputField("local_time")));
        };

        let Some(locations) = data_payloads.locations else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };
        let Some(locations_root) = data_payloads.locations_root else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };
        let Some(non_locations) = (match self.0 {
            FieldLength::Four => data_payloads.mz_generic_long.as_ref(),
            _ => data_payloads.mz_generic_short.as_ref(),
        }) else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };
        let Some(metazone_period) = data_payloads.mz_periods else {
            return Ok(Err(FormatTimeZoneError::NamesNotLoaded));
        };
        let Some(location) = locations
            .locations
            .get(&time_zone_id)
            .or_else(|| locations_root.locations.get(&time_zone_id))
        else {
            return Ok(Err(FormatTimeZoneError::Fallback));
        };
        let Some(non_location) = non_locations.overrides.get(&time_zone_id).or_else(|| {
            non_locations
                .defaults
                .get(&metazone_period.resolve(time_zone_id, local_time)?)
        }) else {
            return Ok(Err(FormatTimeZoneError::Fallback));
        };

        locations
            .pattern_partial_location
            .interpolate((location, non_location))
            .write_to(sink)?;

        Ok(Ok(()))
    }
}

/// Whether the minutes field should be optional or required in ISO-8601 format.
#[derive(Debug, Clone, Copy, PartialEq)]
enum IsoMinutes {
    /// Minutes are always displayed.
    Required,

    /// Minutes are displayed only if they are non-zero.
    Optional,
}

/// Whether the seconds field should be optional or excluded in ISO-8601 format.
#[derive(Debug, Clone, Copy, PartialEq)]
enum IsoSeconds {
    /// Seconds are displayed only if they are non-zero.
    Optional,

    /// Seconds are not displayed.
    Never,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub(crate) struct Iso8601Format {
    // 1000 vs 10:00
    extended: bool,
    // 00:00 vs Z
    z: bool,
    minutes: IsoMinutes,
    seconds: IsoSeconds,
}

impl Iso8601Format {
    pub(crate) fn with_z(length: FieldLength) -> Self {
        match length {
            FieldLength::One => Self {
                extended: false,
                z: true,
                minutes: IsoMinutes::Optional,
                seconds: IsoSeconds::Never,
            },
            FieldLength::Two => Self {
                extended: false,
                z: true,
                minutes: IsoMinutes::Required,
                seconds: IsoSeconds::Never,
            },
            FieldLength::Three => Self {
                extended: true,
                z: true,
                minutes: IsoMinutes::Required,
                seconds: IsoSeconds::Never,
            },
            FieldLength::Four => Self {
                extended: false,
                z: true,
                minutes: IsoMinutes::Required,
                seconds: IsoSeconds::Optional,
            },
            _ => Self {
                extended: true,
                z: true,
                minutes: IsoMinutes::Required,
                seconds: IsoSeconds::Optional,
            },
        }
    }

    pub(crate) fn without_z(length: FieldLength) -> Self {
        match length {
            FieldLength::One => Self {
                extended: false,
                z: false,
                minutes: IsoMinutes::Optional,
                seconds: IsoSeconds::Never,
            },
            FieldLength::Two => Self {
                extended: false,
                z: false,
                minutes: IsoMinutes::Required,
                seconds: IsoSeconds::Never,
            },
            FieldLength::Three => Self {
                extended: true,
                z: false,
                minutes: IsoMinutes::Required,
                seconds: IsoSeconds::Never,
            },
            FieldLength::Four => Self {
                extended: false,
                z: false,
                minutes: IsoMinutes::Required,
                seconds: IsoSeconds::Optional,
            },
            _ => Self {
                extended: true,
                z: false,
                minutes: IsoMinutes::Required,
                seconds: IsoSeconds::Optional,
            },
        }
    }
}

impl FormatTimeZone for Iso8601Format {
    /// Writes a [`UtcOffset`](crate::input::UtcOffset) in ISO-8601 format according to the
    /// given formatting options.
    ///
    /// [`IsoFormat`] determines whether the format should be Basic or Extended,
    /// and whether a zero-offset should be formatted numerically or with
    /// The UTC indicator: "Z"
    /// - Basic    e.g. +0800
    /// - Extended e.g. +08:00
    ///
    /// [`IsoMinutes`] can be required or optional.
    /// [`IsoSeconds`] can be optional or never.
    fn format<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        input: &ExtractedInput,
        _data_payloads: TimeZoneDataPayloadsBorrowed,
        _fdf: Option<&FixedDecimalFormatter>,
    ) -> Result<Result<(), FormatTimeZoneError>, fmt::Error> {
        let Some(offset) = input.offset else {
            sink.write_str("+?")?;
            return Ok(Ok(()));
        };
        self.format_infallible(sink, offset).map(|()| Ok(()))
    }
}

impl Iso8601Format {
    pub(crate) fn format_infallible<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        offset: UtcOffset,
    ) -> Result<(), fmt::Error> {
        if offset.is_zero() && self.z {
            return sink.write_char('Z');
        }

        // Always in latin digits according to spec
        FixedDecimal::from(offset.hours_part())
            .padded_start(2)
            .with_sign_display(fixed_decimal::SignDisplay::Always)
            .write_to(sink)?;

        if self.minutes == IsoMinutes::Required
            || (self.minutes == IsoMinutes::Optional && offset.minutes_part() != 0)
        {
            if self.extended {
                sink.write_char(':')?;
            }
            FixedDecimal::from(offset.minutes_part())
                .padded_start(2)
                .write_to(sink)?;
        }

        if self.seconds == IsoSeconds::Optional && offset.seconds_part() != 0 {
            if self.extended {
                sink.write_char(':')?;
            }
            FixedDecimal::from(offset.seconds_part())
                .padded_start(2)
                .write_to(sink)?;
        }

        Ok(())
    }
}

// It is only used for pattern in special case and not public to users.
struct Bcp47IdFormat;

impl FormatTimeZone for Bcp47IdFormat {
    fn format<W: writeable::PartsWrite + ?Sized>(
        &self,
        sink: &mut W,
        input: &ExtractedInput,
        _data_payloads: TimeZoneDataPayloadsBorrowed,
        _fdf: Option<&FixedDecimalFormatter>,
    ) -> Result<Result<(), FormatTimeZoneError>, fmt::Error> {
        let time_zone_id = input
            .time_zone_id
            .unwrap_or(TimeZoneBcp47Id(tinystr::tinystr!(8, "unk")));

        sink.write_str(&time_zone_id)?;

        Ok(Ok(()))
    }
}
