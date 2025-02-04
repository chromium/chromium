// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use alloc::sync::Arc;
    use core::fmt::Write;

    use crate::calendar::ffi::Calendar;
    use crate::date::ffi::{Date, IsoDate, IsoWeekday};
    use crate::errors::ffi::{CalendarError, CalendarParseError};
    use crate::time::ffi::Time;

    use tinystr::TinyAsciiStr;

    #[cfg(feature = "calendar")]
    use crate::week::ffi::WeekCalculator;

    #[diplomat::opaque]
    /// An ICU4X DateTime object capable of containing a ISO-8601 date and time.
    #[diplomat::rust_link(icu::calendar::DateTime, Struct)]
    pub struct IsoDateTime(pub icu_calendar::DateTime<icu_calendar::Iso>);

    impl IsoDateTime {
        /// Creates a new [`IsoDateTime`] from the specified date and time.
        #[diplomat::rust_link(icu::calendar::DateTime::try_new_iso, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        pub fn create(
            year: i32,
            month: u8,
            day: u8,
            hour: u8,
            minute: u8,
            second: u8,
            nanosecond: u32,
        ) -> Result<Box<IsoDateTime>, CalendarError> {
            let mut dt =
                icu_calendar::DateTime::try_new_iso(year, month, day, hour, minute, second)?;
            dt.time.nanosecond = nanosecond.try_into()?;
            Ok(Box::new(IsoDateTime(dt)))
        }

        /// Creates a new [`IsoDateTime`] from an [`IsoDate`] and [`Time`] object
        #[diplomat::rust_link(icu::calendar::DateTime::new, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_date_and_time(date: &IsoDate, time: &Time) -> Box<IsoDateTime> {
            let dt = icu_calendar::DateTime::new(date.0, time.0);
            Box::new(IsoDateTime(dt))
        }

        /// Creates a new [`IsoDateTime`] from an IXDTF string.
        #[diplomat::rust_link(icu::calendar::DateTime::try_iso_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::DateTime::try_iso_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::DateTime::from_str, FnInStruct, hidden)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_string(v: &DiplomatStr) -> Result<Box<IsoDateTime>, CalendarParseError> {
            Ok(Box::new(IsoDateTime(
                icu_calendar::DateTime::try_iso_from_utf8(v)?,
            )))
        }

        /// Gets the date contained in this object
        #[diplomat::rust_link(icu::calendar::DateTime::date, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn date(&self) -> Box<IsoDate> {
            Box::new(IsoDate(self.0.date))
        }

        /// Gets the time contained in this object
        #[diplomat::rust_link(icu::calendar::DateTime::time, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn time(&self) -> Box<Time> {
            Box::new(Time(self.0.time))
        }

        /// Converts this to an [`DateTime`] capable of being mixed with dates of
        /// other calendars
        #[diplomat::rust_link(icu::calendar::DateTime::to_any, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::DateTime::new_from_iso, FnInStruct, hidden)]
        pub fn to_any(&self) -> Box<DateTime> {
            Box::new(DateTime(self.0.to_any().wrap_calendar_in_arc()))
        }

        /// Convert this datetime to one in a different calendar
        #[diplomat::rust_link(icu::calendar::DateTime::to_calendar, FnInStruct)]
        pub fn to_calendar(&self, calendar: &Calendar) -> Box<DateTime> {
            Box::new(DateTime(self.0.to_calendar(calendar.0.clone())))
        }

        /// Returns the hour in this time
        #[diplomat::rust_link(icu::calendar::Time::hour, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn hour(&self) -> u8 {
            self.0.time.hour.into()
        }
        /// Returns the minute in this time
        #[diplomat::rust_link(icu::calendar::Time::minute, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn minute(&self) -> u8 {
            self.0.time.minute.into()
        }
        /// Returns the second in this time
        #[diplomat::rust_link(icu::calendar::Time::second, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn second(&self) -> u8 {
            self.0.time.second.into()
        }
        /// Returns the nanosecond in this time
        #[diplomat::rust_link(icu::calendar::Time::nanosecond, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn nanosecond(&self) -> u32 {
            self.0.time.nanosecond.into()
        }

        /// Returns the 1-indexed day in the year for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_year_info, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_year(&self) -> u16 {
            self.0.date.day_of_year_info().day_of_year
        }

        /// Returns the 1-indexed day in the month for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_month(&self) -> u8 {
            self.0.date.day_of_month().0
        }

        /// Returns the day in the week for this day
        #[diplomat::rust_link(icu::calendar::Date::day_of_week, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_week(&self) -> IsoWeekday {
            self.0.date.day_of_week().into()
        }

        /// Returns the week number in this month, 1-indexed, based on what
        /// is considered the first day of the week (often a locale preference).
        ///
        /// `first_weekday` can be obtained via `first_weekday()` on [`WeekCalculator`]
        #[diplomat::rust_link(icu::calendar::Date::week_of_month, FnInStruct)]
        #[diplomat::rust_link(
            icu::calendar::week::WeekCalculator::week_of_month,
            FnInStruct,
            hidden
        )]
        pub fn week_of_month(&self, first_weekday: IsoWeekday) -> u8 {
            self.0.date.week_of_month(first_weekday.into()).0
        }

        /// Returns the week number in this year, using week data
        #[diplomat::rust_link(icu::calendar::Date::week_of_year, FnInStruct)]
        #[diplomat::rust_link(
            icu::calendar::week::WeekCalculator::week_of_year,
            FnInStruct,
            hidden
        )]
        #[cfg(feature = "calendar")]
        pub fn week_of_year(&self, calculator: &WeekCalculator) -> crate::week::ffi::WeekOf {
            self.0.date.week_of_year(&calculator.0).into()
        }

        /// Returns 1-indexed number of the month of this date in its year
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::ordinal, StructField)]
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct, compact)]
        #[diplomat::attr(auto, getter)]
        pub fn month(&self) -> u8 {
            self.0.date.month().ordinal
        }

        /// Returns the year number for this date
        #[diplomat::rust_link(icu::calendar::Date::year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn year(&self) -> i32 {
            self.0.date.year().extended_year
        }

        /// Returns whether this date is in a leap year
        #[diplomat::rust_link(icu::calendar::Date::is_in_leap_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn is_in_leap_year(&self) -> bool {
            self.0.date.is_in_leap_year()
        }

        /// Returns the number of months in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::months_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn months_in_year(&self) -> u8 {
            self.0.date.months_in_year()
        }

        /// Returns the number of days in the month represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn days_in_month(&self) -> u8 {
            self.0.date.days_in_month()
        }

        /// Returns the number of days in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn days_in_year(&self) -> u16 {
            self.0.date.days_in_year()
        }
    }

    #[diplomat::opaque]
    /// An ICU4X DateTime object capable of containing a date and time for any calendar.
    #[diplomat::rust_link(icu::calendar::DateTime, Struct)]
    pub struct DateTime(pub icu_calendar::DateTime<Arc<icu_calendar::AnyCalendar>>);

    impl DateTime {
        /// Creates a new [`DateTime`] representing the ISO date and time
        /// given but in a given calendar
        #[diplomat::rust_link(icu::DateTime::new_from_iso, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        #[diplomat::demo(default_constructor)]
        #[allow(clippy::too_many_arguments)]
        pub fn from_iso_in_calendar(
            year: i32,
            month: u8,
            day: u8,
            hour: u8,
            minute: u8,
            second: u8,
            nanosecond: u32,
            calendar: &Calendar,
        ) -> Result<Box<DateTime>, CalendarError> {
            let cal = calendar.0.clone();
            let mut dt =
                icu_calendar::DateTime::try_new_iso(year, month, day, hour, minute, second)?;
            dt.time.nanosecond = nanosecond.try_into()?;
            Ok(Box::new(DateTime(dt.to_calendar(cal))))
        }
        /// Creates a new [`DateTime`] from the given codes, which are interpreted in the given calendar system
        ///
        /// An empty era code will treat the year as an extended year
        #[diplomat::rust_link(icu::calendar::DateTime::try_new_from_codes, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        #[allow(clippy::too_many_arguments)]
        pub fn from_codes_in_calendar(
            era_code: &DiplomatStr,
            year: i32,
            month_code: &DiplomatStr,
            day: u8,
            hour: u8,
            minute: u8,
            second: u8,
            nanosecond: u32,
            calendar: &Calendar,
        ) -> Result<Box<DateTime>, CalendarError> {
            let era = if era_code.is_empty() {
                Some(icu_calendar::types::Era(
                    TinyAsciiStr::try_from_utf8(era_code).map_err(|_| CalendarError::UnknownEra)?,
                ))
            } else {
                None
            };
            let month = icu_calendar::types::MonthCode(
                TinyAsciiStr::try_from_utf8(month_code)
                    .map_err(|_| CalendarError::UnknownMonthCode)?,
            );
            let cal = calendar.0.clone();
            let hour = hour.try_into()?;
            let minute = minute.try_into()?;
            let second = second.try_into()?;
            let nanosecond = nanosecond.try_into()?;
            let time = icu_calendar::Time {
                hour,
                minute,
                second,
                nanosecond,
            };
            Ok(Box::new(DateTime(
                icu_calendar::DateTime::try_new_from_codes(era, year, month, day, time, cal)?,
            )))
        }
        /// Creates a new [`DateTime`] from an [`Date`] and [`Time`] object
        #[diplomat::rust_link(icu::calendar::DateTime::new, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_date_and_time(date: &Date, time: &Time) -> Box<DateTime> {
            let dt = icu_calendar::DateTime::new(date.0.clone(), time.0);
            Box::new(DateTime(dt))
        }

        /// Creates a new [`DateTime`] from an IXDTF string.
        #[diplomat::rust_link(icu::calendar::DateTime::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::DateTime::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::DateTime::from_str, FnInStruct, hidden)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn from_string(v: &DiplomatStr) -> Result<Box<DateTime>, CalendarParseError> {
            Ok(Box::new(DateTime(
                icu_calendar::DateTime::try_from_utf8(v)?.wrap_calendar_in_arc(),
            )))
        }

        /// Gets a copy of the date contained in this object
        #[diplomat::rust_link(icu::calendar::DateTime::date, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn date(&self) -> Box<Date> {
            Box::new(Date(self.0.date.clone()))
        }

        /// Gets the time contained in this object
        #[diplomat::rust_link(icu::calendar::DateTime::time, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn time(&self) -> Box<Time> {
            Box::new(Time(self.0.time))
        }

        /// Converts this date to ISO
        #[diplomat::rust_link(icu::calendar::DateTime::to_iso, FnInStruct)]
        pub fn to_iso(&self) -> Box<IsoDateTime> {
            Box::new(IsoDateTime(self.0.to_iso()))
        }

        /// Convert this datetime to one in a different calendar
        #[diplomat::rust_link(icu::calendar::DateTime::to_calendar, FnInStruct)]
        pub fn to_calendar(&self, calendar: &Calendar) -> Box<DateTime> {
            Box::new(DateTime(self.0.to_calendar(calendar.0.clone())))
        }

        /// Returns the hour in this time
        #[diplomat::rust_link(icu::calendar::Time::hour, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn hour(&self) -> u8 {
            self.0.time.hour.into()
        }
        /// Returns the minute in this time
        #[diplomat::rust_link(icu::calendar::Time::minute, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn minute(&self) -> u8 {
            self.0.time.minute.into()
        }
        /// Returns the second in this time
        #[diplomat::rust_link(icu::calendar::Time::second, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn second(&self) -> u8 {
            self.0.time.second.into()
        }
        /// Returns the nanosecond in this time
        #[diplomat::rust_link(icu::calendar::Time::nanosecond, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn nanosecond(&self) -> u32 {
            self.0.time.nanosecond.into()
        }

        /// Returns the 1-indexed day in the year for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_year_info, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_year(&self) -> u16 {
            self.0.date.day_of_year_info().day_of_year
        }

        /// Returns the 1-indexed day in the month for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_month(&self) -> u8 {
            self.0.date.day_of_month().0
        }

        /// Returns the day in the week for this day
        #[diplomat::rust_link(icu::calendar::Date::day_of_week, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_week(&self) -> IsoWeekday {
            self.0.date.day_of_week().into()
        }

        /// Returns the week number in this month, 1-indexed, based on what
        /// is considered the first day of the week (often a locale preference).
        ///
        /// `first_weekday` can be obtained via `first_weekday()` on [`WeekCalculator`]
        #[diplomat::rust_link(icu::calendar::Date::week_of_month, FnInStruct)]
        #[diplomat::rust_link(
            icu::calendar::week::WeekCalculator::week_of_month,
            FnInStruct,
            hidden
        )]
        pub fn week_of_month(&self, first_weekday: IsoWeekday) -> u8 {
            self.0.date.week_of_month(first_weekday.into()).0
        }

        /// Returns the week number in this year, using week data
        #[diplomat::rust_link(icu::calendar::Date::week_of_year, FnInStruct)]
        #[diplomat::rust_link(
            icu::calendar::week::WeekCalculator::week_of_year,
            FnInStruct,
            hidden
        )]
        #[cfg(feature = "calendar")]
        pub fn week_of_year(&self, calculator: &WeekCalculator) -> crate::week::ffi::WeekOf {
            self.0.date.week_of_year(&calculator.0).into()
        }

        /// Returns 1-indexed number of the month of this date in its year
        ///
        /// Note that for lunar calendars this may not lead to the same month
        /// having the same ordinal month across years; use month_code if you care
        /// about month identity.
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::ordinal, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn ordinal_month(&self) -> u8 {
            self.0.date.month().ordinal
        }

        /// Returns the month code for this date. Typically something
        /// like "M01", "M02", but can be more complicated for lunar calendars.
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::standard_code, StructField)]
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct, compact)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo, Struct, hidden)]
        #[diplomat::rust_link(
            icu::calendar::types::MonthInfo::formatting_code,
            StructField,
            hidden
        )]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo, Struct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn month_code(&self, write: &mut diplomat_runtime::DiplomatWrite) {
            let code = self.0.date.month().standard_code;
            let _infallible = write.write_str(&code.0);
        }

        /// Returns the month number of this month.
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::month_number, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn month_number(&self) -> u8 {
            self.0.date.month().month_number()
        }

        /// Returns whether the month is a leap month.
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::is_leap, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn month_is_leap(&self) -> bool {
            self.0.date.month().is_leap()
        }

        /// Returns the year number in the current era for this date
        ///
        /// For calendars without an era, returns the extended year
        #[diplomat::rust_link(icu::calendar::types::YearInfo::era_year_or_extended, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::types::EraYear::era_year, StructField, compact)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo::era_year, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::Date::year, FnInStruct, compact)]
        #[diplomat::rust_link(icu::calendar::types::EraYear, Struct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearKind, Enum, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo, Struct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn year_in_era(&self) -> i32 {
            self.0.date.year().era_year_or_extended()
        }

        /// Returns the extended year in the Date
        #[diplomat::rust_link(icu::calendar::types::YearInfo::extended_year, StructField)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo, Struct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn extended_year(&self) -> i32 {
            self.0.date.year().extended_year
        }

        /// Returns the era for this date, or an empty string
        #[diplomat::rust_link(icu::calendar::types::EraYear::standard_era, StructField)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo::standard_era, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::Date::year, FnInStruct, compact)]
        #[diplomat::rust_link(icu::calendar::types::EraYear, Struct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearKind, Enum, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo, Struct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::EraYear::formatting_era, StructField, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo::formatting_era, FnInStruct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn era(&self, write: &mut diplomat_runtime::DiplomatWrite) {
            if let Some(era) = self.0.date.year().standard_era() {
                let _infallible = write.write_str(&era.0);
            }
        }

        /// Returns the number of months in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::months_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn months_in_year(&self) -> u8 {
            self.0.date.months_in_year()
        }

        /// Returns the number of days in the month represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn days_in_month(&self) -> u8 {
            self.0.date.days_in_month()
        }

        /// Returns the number of days in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn days_in_year(&self) -> u16 {
            self.0.date.days_in_year()
        }

        /// Returns the [`Calendar`] object backing this date
        #[diplomat::rust_link(icu::calendar::Date::calendar, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::calendar_wrapper, FnInStruct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn calendar(&self) -> Box<Calendar> {
            Box::new(Calendar(self.0.date.calendar_wrapper().clone()))
        }
    }
}
