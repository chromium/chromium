#[cfg(feature = "compiled_data")]
use crate::error::ffi::TemporalError;
#[cfg(feature = "compiled_data")]
use temporal_rs::options::RelativeTo;

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
#[cfg(feature = "compiled_data")]
pub mod ffi {
    use crate::calendar::ffi::AnyCalendarKind;
    use crate::calendar::ffi::Calendar;
    use crate::duration::ffi::Duration;
    use crate::error::ffi::TemporalError;
    use crate::plain_date::ffi::{PartialDate, PlainDate};
    use crate::plain_date_time::ffi::PlainDateTime;
    use crate::plain_time::ffi::{PartialTime, PlainTime};
    use alloc::boxed::Box;

    use crate::instant::ffi::I128Nanoseconds;
    use crate::instant::ffi::Instant;
    use crate::options::ffi::{
        ArithmeticOverflow, DifferenceSettings, Disambiguation, DisplayCalendar, DisplayOffset,
        DisplayTimeZone, OffsetDisambiguation, RoundingOptions, ToStringRoundingOptions,
        TransitionDirection,
    };

    use crate::time_zone::ffi::TimeZone;

    use alloc::string::String;
    use core::fmt::Write;

    use diplomat_runtime::DiplomatOption;
    use diplomat_runtime::DiplomatStrSlice;
    use diplomat_runtime::DiplomatWrite;

    pub struct PartialZonedDateTime<'a> {
        pub date: PartialDate<'a>,
        pub time: PartialTime,
        pub offset: DiplomatOption<DiplomatStrSlice<'a>>,
        pub timezone: Option<&'a TimeZone>,
    }

    pub struct RelativeTo<'a> {
        pub date: Option<&'a PlainDate>,
        pub zoned: Option<&'a ZonedDateTime>,
    }

    /// GetTemporalRelativeToOption can create fresh PlainDate/ZonedDateTimes by parsing them,
    /// we need a way to produce that result.
    #[diplomat::out]
    pub struct OwnedRelativeTo {
        pub date: Option<Box<PlainDate>>,
        pub zoned: Option<Box<ZonedDateTime>>,
    }

    impl OwnedRelativeTo {
        // Retained for compatability
        // TODO remove in any version after 0.0.10
        pub fn try_from_str(s: &DiplomatStr) -> Result<Self, TemporalError> {
            Self::from_utf8(s)
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Self, TemporalError> {
            // TODO(#275) This should not need to check
            let s = core::str::from_utf8(s).map_err(|_| temporal_rs::TemporalError::range())?;

            super::RelativeTo::try_from_str(s)
                .map(Into::into)
                .map_err(Into::<TemporalError>::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Self, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            super::RelativeTo::try_from_str(&s)
                .map(Into::into)
                .map_err(Into::<TemporalError>::into)
        }

        pub fn empty() -> Self {
            Self {
                date: None,
                zoned: None,
            }
        }
    }

    #[diplomat::opaque]
    pub struct OwnedPartialZonedDateTime(temporal_rs::partial::PartialZonedDateTime);

    impl OwnedPartialZonedDateTime {
        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::partial::PartialZonedDateTime::try_from_utf8(s)
                .map(|x| Box::new(OwnedPartialZonedDateTime(x)))
                .map_err(Into::<TemporalError>::into)
        }
        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;

            temporal_rs::partial::PartialZonedDateTime::try_from_utf8(s.as_bytes())
                .map(|x| Box::new(OwnedPartialZonedDateTime(x)))
                .map_err(Into::<TemporalError>::into)
        }
    }

    #[diplomat::opaque]
    pub struct ZonedDateTime(pub(crate) temporal_rs::ZonedDateTime);

    impl ZonedDateTime {
        pub fn try_new(
            nanosecond: I128Nanoseconds,
            calendar: AnyCalendarKind,
            time_zone: &TimeZone,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::ZonedDateTime::try_new(
                nanosecond.into(),
                temporal_rs::Calendar::new(calendar.into()),
                time_zone.0.clone(),
            )
            .map(|x| Box::new(ZonedDateTime(x)))
            .map_err(Into::into)
        }

        pub fn from_partial(
            partial: PartialZonedDateTime,
            overflow: Option<ArithmeticOverflow>,
            disambiguation: Option<Disambiguation>,
            offset_option: Option<OffsetDisambiguation>,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::ZonedDateTime::from_partial(
                partial.try_into()?,
                overflow.map(Into::into),
                disambiguation.map(Into::into),
                offset_option.map(Into::into),
            )
            .map(|x| Box::new(ZonedDateTime(x)))
            .map_err(Into::into)
        }

        pub fn from_owned_partial(
            partial: &OwnedPartialZonedDateTime,
            overflow: Option<ArithmeticOverflow>,
            disambiguation: Option<Disambiguation>,
            offset_option: Option<OffsetDisambiguation>,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::ZonedDateTime::from_partial(
                partial.0.clone(),
                overflow.map(Into::into),
                disambiguation.map(Into::into),
                offset_option.map(Into::into),
            )
            .map(|x| Box::new(ZonedDateTime(x)))
            .map_err(Into::into)
        }

        pub fn from_utf8(
            s: &DiplomatStr,
            disambiguation: Disambiguation,
            offset_disambiguation: OffsetDisambiguation,
        ) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to check
            temporal_rs::ZonedDateTime::from_utf8(
                s,
                disambiguation.into(),
                offset_disambiguation.into(),
            )
            .map(|c| Box::new(Self(c)))
            .map_err(Into::into)
        }

        pub fn from_utf16(
            s: &DiplomatStr16,
            disambiguation: Disambiguation,
            offset_disambiguation: OffsetDisambiguation,
        ) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::ZonedDateTime::from_utf8(
                s.as_bytes(),
                disambiguation.into(),
                offset_disambiguation.into(),
            )
            .map(|c| Box::new(Self(c)))
            .map_err(Into::into)
        }

        pub fn epoch_milliseconds(&self) -> i64 {
            self.0.epoch_milliseconds()
        }

        pub fn from_epoch_milliseconds(ms: i64, tz: &TimeZone) -> Result<Box<Self>, TemporalError> {
            super::zdt_from_epoch_ms(ms, &tz.0).map(|c| Box::new(Self(c)))
        }

        pub fn epoch_nanoseconds(&self) -> I128Nanoseconds {
            self.0.epoch_nanoseconds().as_i128().into()
        }

        pub fn offset_nanoseconds(&self) -> Result<i64, TemporalError> {
            self.0.offset_nanoseconds().map_err(Into::into)
        }

        pub fn to_instant(&self) -> Box<Instant> {
            Box::new(Instant(self.0.to_instant()))
        }

        pub fn with(
            &self,
            partial: PartialZonedDateTime,
            disambiguation: Option<Disambiguation>,
            offset_option: Option<OffsetDisambiguation>,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .with(
                    partial.try_into()?,
                    disambiguation.map(Into::into),
                    offset_option.map(Into::into),
                    overflow.map(Into::into),
                )
                .map(|x| Box::new(ZonedDateTime(x)))
                .map_err(Into::into)
        }

        pub fn with_timezone(&self, zone: &TimeZone) -> Result<Box<Self>, TemporalError> {
            self.0
                .with_timezone(zone.0.clone())
                .map(|x| Box::new(ZonedDateTime(x)))
                .map_err(Into::into)
        }

        pub fn timezone<'a>(&'a self) -> &'a TimeZone {
            TimeZone::transparent_convert(self.0.timezone())
        }

        pub fn compare_instant(&self, other: &Self) -> core::cmp::Ordering {
            self.0.compare_instant(&other.0)
        }

        pub fn equals(&self, other: &Self) -> bool {
            self.0 == other.0
        }

        pub fn offset(&self, write: &mut DiplomatWrite) -> Result<(), TemporalError> {
            let string = self.0.offset()?;
            // throw away the error, this should always succeed
            let _ = write.write_str(&string);
            Ok(())
        }

        pub fn start_of_day(&self) -> Result<Box<ZonedDateTime>, TemporalError> {
            self.0
                .start_of_day()
                .map(|x| Box::new(ZonedDateTime(x)))
                .map_err(Into::into)
        }

        pub fn get_time_zone_transition(
            &self,
            direction: TransitionDirection,
        ) -> Result<Option<Box<Self>>, TemporalError> {
            self.0
                .get_time_zone_transition(direction.into())
                .map(|x| x.map(|y| Box::new(ZonedDateTime(y))))
                .map_err(Into::into)
        }

        pub fn hours_in_day(&self) -> Result<u8, TemporalError> {
            self.0.hours_in_day().map_err(Into::into)
        }

        pub fn to_plain_datetime(&self) -> Result<Box<PlainDateTime>, TemporalError> {
            self.0
                .to_plain_datetime()
                .map(|x| Box::new(PlainDateTime(x)))
                .map_err(Into::into)
        }

        pub fn to_plain_date(&self) -> Result<Box<PlainDate>, TemporalError> {
            self.0
                .to_plain_date()
                .map(|x| Box::new(PlainDate(x)))
                .map_err(Into::into)
        }

        pub fn to_plain_time(&self) -> Result<Box<PlainTime>, TemporalError> {
            self.0
                .to_plain_time()
                .map(|x| Box::new(PlainTime(x)))
                .map_err(Into::into)
        }

        pub fn to_ixdtf_string(
            &self,
            display_offset: DisplayOffset,
            display_timezone: DisplayTimeZone,
            display_calendar: DisplayCalendar,
            options: ToStringRoundingOptions,

            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            // TODO this double-allocates, an API returning a Writeable or impl Write would be better
            let string = self.0.to_ixdtf_string(
                display_offset.into(),
                display_timezone.into(),
                display_calendar.into(),
                options.into(),
            )?;
            // throw away the error, this should always succeed
            let _ = write.write_str(&string);
            Ok(())
        }

        // Same as PlainDateTime (non-getters)

        pub fn with_calendar(&self, calendar: AnyCalendarKind) -> Result<Box<Self>, TemporalError> {
            self.0
                .with_calendar(temporal_rs::Calendar::new(calendar.into()))
                .map(|x| Box::new(ZonedDateTime(x)))
                .map_err(Into::into)
        }

        pub fn with_plain_time(
            &self,
            time: Option<&PlainTime>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .with_plain_time(time.map(|t| t.0))
                .map(|x| Box::new(ZonedDateTime(x)))
                .map_err(Into::into)
        }

        pub fn add(
            &self,
            duration: &Duration,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .add(&duration.0, overflow.map(Into::into))
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn subtract(
            &self,
            duration: &Duration,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract(&duration.0, overflow.map(Into::into))
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn until(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .until(&other.0, settings.try_into()?)
                .map(|x| Box::new(Duration(x)))
                .map_err(Into::into)
        }
        pub fn since(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .since(&other.0, settings.try_into()?)
                .map(|x| Box::new(Duration(x)))
                .map_err(Into::into)
        }

        pub fn round(&self, options: RoundingOptions) -> Result<Box<Self>, TemporalError> {
            self.0
                .round(options.try_into()?)
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }

        // Same as PlainDateTime (getters)

        pub fn hour(&self) -> u8 {
            // unwrap_or_default because of
            // https://github.com/boa-dev/temporal/issues/328
            self.0.hour().unwrap_or_default()
        }

        pub fn minute(&self) -> u8 {
            self.0.minute().unwrap_or_default()
        }

        pub fn second(&self) -> u8 {
            self.0.second().unwrap_or_default()
        }

        pub fn millisecond(&self) -> u16 {
            self.0.millisecond().unwrap_or_default()
        }

        pub fn microsecond(&self) -> u16 {
            self.0.microsecond().unwrap_or_default()
        }

        pub fn nanosecond(&self) -> u16 {
            self.0.nanosecond().unwrap_or_default()
        }

        pub fn calendar<'a>(&'a self) -> &'a Calendar {
            Calendar::transparent_convert(self.0.calendar())
        }

        pub fn year(&self) -> i32 {
            self.0.year().unwrap_or_default()
        }

        pub fn month(&self) -> u8 {
            self.0.month().unwrap_or_default()
        }

        pub fn month_code(&self, write: &mut DiplomatWrite) {
            // https://github.com/boa-dev/temporal/issues/328 for the fallibility
            let Ok(code) = self.0.month_code() else {
                return;
            };
            // throw away the error, this should always succeed
            let _ = write.write_str(code.as_str());
        }

        pub fn day(&self) -> u8 {
            self.0.day().unwrap_or_default()
        }

        pub fn day_of_week(&self) -> Result<u16, TemporalError> {
            self.0.day_of_week().map_err(Into::into)
        }

        pub fn day_of_year(&self) -> u16 {
            self.0.day_of_year().unwrap_or_default()
        }

        pub fn week_of_year(&self) -> Option<u8> {
            self.0.week_of_year().unwrap_or_default()
        }

        pub fn year_of_week(&self) -> Option<i32> {
            self.0.year_of_week().unwrap_or_default()
        }

        pub fn days_in_week(&self) -> Result<u16, TemporalError> {
            self.0.days_in_week().map_err(Into::into)
        }

        pub fn days_in_month(&self) -> u16 {
            self.0.days_in_month().unwrap_or_default()
        }

        pub fn days_in_year(&self) -> u16 {
            self.0.days_in_year().unwrap_or_default()
        }

        pub fn months_in_year(&self) -> u16 {
            self.0.months_in_year().unwrap_or_default()
        }

        pub fn in_leap_year(&self) -> bool {
            self.0.in_leap_year().unwrap_or_default()
        }
        // Writes an empty string for no era

        pub fn era(&self, write: &mut DiplomatWrite) {
            let era = self.0.era().unwrap_or_default();
            if let Some(era) = era {
                // throw away the error, this should always succeed
                let _ = write.write_str(&era);
            }
        }

        pub fn era_year(&self) -> Option<i32> {
            self.0.era_year().unwrap_or_default()
        }
    }
}

#[cfg(feature = "compiled_data")]
pub(crate) fn zdt_from_epoch_ms(
    ms: i64,
    time_zone: &temporal_rs::TimeZone,
) -> Result<temporal_rs::ZonedDateTime, TemporalError> {
    let instant = temporal_rs::Instant::from_epoch_milliseconds(ms)?;
    Ok(instant.to_zoned_date_time_iso(time_zone.clone()))
}

#[cfg(feature = "compiled_data")]
impl TryFrom<ffi::PartialZonedDateTime<'_>> for temporal_rs::partial::PartialZonedDateTime {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialZonedDateTime<'_>) -> Result<Self, TemporalError> {
        let offset = match other.offset.into_option() {
            Some(o) => Some(temporal_rs::UtcOffset::from_utf8(o.into())?),
            None => None,
        };
        Ok(Self {
            date: other.date.try_into()?,
            time: other.time.into(),
            // This is only true when parsing
            has_utc_designator: false,
            offset,
            timezone: other.timezone.map(|x| x.0.clone()),
        })
    }
}

#[cfg(feature = "compiled_data")]
impl From<ffi::RelativeTo<'_>> for Option<temporal_rs::options::RelativeTo> {
    fn from(other: ffi::RelativeTo) -> Self {
        if let Some(pd) = other.date {
            Some(temporal_rs::options::RelativeTo::PlainDate(pd.0.clone()))
        } else {
            other
                .zoned
                .map(|z| temporal_rs::options::RelativeTo::ZonedDateTime(z.0.clone()))
        }
    }
}

#[cfg(feature = "compiled_data")]
impl From<RelativeTo> for ffi::OwnedRelativeTo {
    fn from(other: RelativeTo) -> Self {
        use alloc::boxed::Box;
        match other {
            RelativeTo::PlainDate(d) => Self {
                date: Some(Box::new(crate::plain_date::ffi::PlainDate(d))),
                zoned: None,
            },
            RelativeTo::ZonedDateTime(d) => Self {
                zoned: Some(Box::new(ffi::ZonedDateTime(d))),
                date: None,
            },
        }
    }
}
