// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use ffi::IsoWeekOfYear;

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;
    use core::fmt::Write;
    use diplomat_runtime::DiplomatOption;
    use icu_calendar::Iso;

    use crate::unstable::calendar::ffi::Calendar;
    use crate::unstable::errors::ffi::{
        CalendarDateAddError, CalendarDateFromFieldsError, CalendarMismatchedCalendarError,
        DateDurationParseError,
    };
    use crate::unstable::errors::ffi::{CalendarError, Rfc9557ParseError};

    #[diplomat::enum_convert(icu_calendar::types::Weekday)]
    #[diplomat::rust_link(icu::calendar::types::Weekday, Enum)]
    #[non_exhaustive]
    pub enum Weekday {
        Monday = 1,
        Tuesday,
        Wednesday,
        Thursday,
        Friday,
        Saturday,
        Sunday,
    }

    #[diplomat::enum_convert(icu_calendar::options::DateDurationUnit, needs_wildcard)]
    #[diplomat::rust_link(icu::calendar::options::DateDurationUnit, Enum)]
    #[non_exhaustive]
    pub enum DateDurationUnit {
        Years,
        Months,
        Weeks,
        Days,
    }

    #[diplomat::rust_link(icu::calendar::types::DateDuration, Struct)]
    pub struct DateDuration {
        pub is_negative: bool,
        pub years: u32,
        pub months: u32,
        pub weeks: u32,
        pub days: u32,
    }

    impl DateDuration {
        /// Creates a new [`DateDuration`] from an ISO 8601 string.
        #[diplomat::rust_link(icu::calendar::types::DateDuration::try_from_str, FnInStruct)]
        #[diplomat::rust_link(
            icu::calendar::types::DateDuration::try_from_utf8,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(icu::calendar::types::DateDuration::from_str, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_string(v: &DiplomatStr) -> Result<DateDuration, DateDurationParseError> {
            Ok(icu_calendar::types::DateDuration::try_from_utf8(v)?.into())
        }

        /// Returns a new [`DateDuration`] representing a number of years.
        #[diplomat::rust_link(icu::calendar::types::DateDuration::for_years, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn for_years(years: i32) -> DateDuration {
            icu_calendar::types::DateDuration::for_years(years).into()
        }

        /// Returns a new [`DateDuration`] representing a number of months.
        #[diplomat::rust_link(icu::calendar::types::DateDuration::for_months, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn for_months(months: i32) -> DateDuration {
            icu_calendar::types::DateDuration::for_months(months).into()
        }

        /// Returns a new [`DateDuration`] representing a number of weeks.
        #[diplomat::rust_link(icu::calendar::types::DateDuration::for_weeks, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn for_weeks(weeks: i32) -> DateDuration {
            icu_calendar::types::DateDuration::for_weeks(weeks).into()
        }

        /// Returns a new [`DateDuration`] representing a number of days.
        #[diplomat::rust_link(icu::calendar::types::DateDuration::for_days, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn for_days(days: i32) -> DateDuration {
            icu_calendar::types::DateDuration::for_days(days).into()
        }
    }

    #[diplomat::rust_link(icu::calendar::options::DateAddOptions, Struct)]
    pub struct DateAddOptions {
        pub overflow: DiplomatOption<DateOverflow>,
    }

    #[diplomat::rust_link(icu::calendar::options::DateDifferenceOptions, Struct)]
    pub struct DateDifferenceOptions {
        pub largest_unit: DiplomatOption<DateDurationUnit>,
    }

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    /// An ICU4X Date object capable of containing a ISO-8601 date
    #[diplomat::rust_link(icu::calendar::Date, Struct)]
    pub struct IsoDate(pub icu_calendar::Date<icu_calendar::Iso>);

    impl IsoDate {
        /// Creates a new [`IsoDate`] from the specified date.
        #[diplomat::rust_link(icu::calendar::Date::try_new_iso, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        pub fn create(year: i32, month: u8, day: u8) -> Result<Box<IsoDate>, CalendarError> {
            Ok(Box::new(IsoDate(icu_calendar::Date::try_new_iso(
                year, month, day,
            )?)))
        }

        /// Creates a new [`IsoDate`] from the given Rata Die
        #[diplomat::rust_link(icu::calendar::Date::from_rata_die, FnInStruct)]
        #[diplomat::attr(all(supports = named_constructors), named_constructor)]
        #[diplomat::demo(default_constructor)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn from_rata_die(rd: i64) -> Box<IsoDate> {
            Box::new(IsoDate(icu_calendar::Date::from_rata_die(
                icu_calendar::types::RataDie::new(rd),
                Iso,
            )))
        }

        /// Creates a new [`IsoDate`] from an IXDTF string.
        #[diplomat::rust_link(icu::calendar::Date::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::Date::from_str, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn from_string(v: &DiplomatStr) -> Result<Box<IsoDate>, Rfc9557ParseError> {
            Ok(Box::new(IsoDate(icu_calendar::Date::try_from_utf8(
                v, Iso,
            )?)))
        }

        /// Convert this date to one in a different calendar
        #[diplomat::rust_link(icu::calendar::Date::to_calendar, FnInStruct)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn to_calendar(&self, calendar: &Calendar) -> Box<Date> {
            Box::new(Date(self.0.to_calendar(calendar.0.clone())))
        }

        #[diplomat::rust_link(icu::calendar::Date::to_any, FnInStruct)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn to_any(&self) -> Box<Date> {
            Box::new(Date(self.0.to_any()))
        }

        /// Returns this date's Rata Die
        #[diplomat::rust_link(icu::calendar::Date::to_rata_die, FnInStruct)]
        #[diplomat::attr(auto, getter = "rata_die")]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn to_rata_die(&self) -> i64 {
            self.0.to_rata_die().to_i64_date()
        }

        /// Returns the 1-indexed day in the year for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn day_of_year(&self) -> u16 {
            self.0.day_of_year().0
        }

        /// Returns the 1-indexed day in the month for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn day_of_month(&self) -> u8 {
            self.0.day_of_month().0
        }

        /// Returns the day in the week for this day
        ///
        /// This is *not* the day of the week, an ordinal number that is locale
        /// dependent.
        #[diplomat::rust_link(icu::calendar::Date::day_of_week, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        #[deprecated(note = "use `weekday`")]
        pub fn day_of_week(&self) -> Weekday {
            self.weekday()
        }

        /// Returns the day in the week for this day
        #[diplomat::rust_link(icu::calendar::Date::weekday, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn weekday(&self) -> Weekday {
            self.0.weekday().into()
        }

        /// Returns the week number in this year, using week data
        #[diplomat::rust_link(icu::calendar::Date::week_of_year, FnInStruct)]
        #[cfg(feature = "calendar")]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn week_of_year(&self) -> IsoWeekOfYear {
            self.0.week_of_year().into()
        }

        /// Returns 1-indexed number of the month of this date in its year
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::ordinal, StructField)]
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct, compact)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn month(&self) -> u8 {
            self.0.month().ordinal
        }

        /// Returns the year number in the current era for this date
        ///
        /// For calendars without an era, returns the extended year
        #[diplomat::rust_link(icu::calendar::types::YearInfo::extended_year, FnInEnum)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn year(&self) -> i32 {
            self.0.year().extended_year()
        }

        /// Returns if the year is a leap year for this date
        #[diplomat::rust_link(icu::calendar::Date::is_in_leap_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn is_in_leap_year(&self) -> bool {
            self.0.is_in_leap_year()
        }

        /// Returns the number of months in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::months_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn months_in_year(&self) -> u8 {
            self.0.months_in_year()
        }

        /// Returns the number of days in the month represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn days_in_month(&self) -> u8 {
            self.0.days_in_month()
        }

        /// Returns the number of days in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn days_in_year(&self) -> u16 {
            self.0.days_in_year()
        }

        /// Returns a new [`IsoDate`] with the given duration added to it.
        #[diplomat::rust_link(icu::calendar::Date::try_added_with_options, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::try_add_with_options, FnInStruct, hidden)]
        pub fn try_add_with_options(
            &self,
            duration: DateDuration,
            options: DateAddOptions,
        ) -> Result<Box<IsoDate>, CalendarDateAddError> {
            Ok(Box::new(IsoDate(self.0.try_added_with_options(
                duration.into(),
                options.into(),
            )?)))
        }

        /// Calculating the duration between `other - self`
        #[diplomat::rust_link(icu::calendar::Date::try_until_with_options, FnInStruct)]
        pub fn until_with_options(
            &self,
            other: &IsoDate,
            options: DateDifferenceOptions,
        ) -> DateDuration {
            let Ok(duration) = self.0.try_until_with_options(&other.0, options.into());
            duration.into()
        }
    }

    #[diplomat::rust_link(icu::calendar::options::DateFromFieldsOptions, Struct)]
    pub struct DateFromFieldsOptions {
        pub overflow: DiplomatOption<DateOverflow>,
        pub missing_fields_strategy: DiplomatOption<DateMissingFieldsStrategy>,
    }

    #[diplomat::rust_link(icu::calendar::types::DateFields, Struct)]
    pub struct DateFields<'a> {
        pub era: DiplomatOption<&'a DiplomatStr>,
        pub era_year: DiplomatOption<i32>,
        pub extended_year: DiplomatOption<i32>,
        pub month_code: DiplomatOption<&'a DiplomatStr>,
        pub ordinal_month: DiplomatOption<u8>,
        pub day: DiplomatOption<u8>,
    }

    #[diplomat::enum_convert(icu_calendar::options::Overflow, needs_wildcard)]
    #[diplomat::rust_link(icu::calendar::options::Overflow, Enum)]
    #[non_exhaustive]
    pub enum DateOverflow {
        Constrain,
        Reject,
    }

    #[diplomat::enum_convert(icu_calendar::options::MissingFieldsStrategy, needs_wildcard)]
    #[diplomat::rust_link(icu::calendar::options::MissingFieldsStrategy, Enum)]
    #[non_exhaustive]
    pub enum DateMissingFieldsStrategy {
        Reject,
        Ecma,
    }

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    /// An ICU4X Date object capable of containing a date for any calendar.
    #[diplomat::rust_link(icu::calendar::Date, Struct)]
    pub struct Date(pub icu_calendar::Date<icu_calendar::AnyCalendar>);

    impl Date {
        /// Creates a new [`Date`] representing the ISO date
        /// given but in a given calendar
        #[diplomat::rust_link(icu::calendar::Date::new_from_iso, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn from_iso_in_calendar(
            iso_year: i32,
            iso_month: u8,
            iso_day: u8,
            calendar: &Calendar,
        ) -> Result<Box<Date>, CalendarError> {
            let cal = calendar.0.clone();
            Ok(Box::new(Date(
                icu_calendar::Date::try_new_iso(iso_year, iso_month, iso_day)?.to_calendar(cal),
            )))
        }

        /// Creates a new [`Date`] from the given fields, which are interpreted in the given calendar system.
        #[diplomat::rust_link(icu::calendar::Date::try_from_fields, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_fields_in_calendar(
            fields: DateFields,
            options: DateFromFieldsOptions,
            calendar: &Calendar,
        ) -> Result<Box<Date>, CalendarDateFromFieldsError> {
            let cal = calendar.0.clone();
            Ok(Box::new(Date(icu_calendar::Date::try_from_fields(
                fields.into(),
                options.into(),
                cal,
            )?)))
        }

        /// Creates a new [`Date`] from the given codes, which are interpreted in the given calendar system
        ///
        /// An empty era code will treat the year as an extended year
        #[diplomat::rust_link(icu::calendar::Date::try_new, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::try_new_from_codes, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::Month::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::types::Month::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::Month::leap, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::Month::new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearInput, Enum, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_codes_in_calendar(
            era_code: &DiplomatStr,
            year: i32,
            month_code: &DiplomatStr,
            day: u8,
            calendar: &Calendar,
        ) -> Result<Box<Date>, CalendarError> {
            use icu_calendar::types::YearInput;
            let input_year = if !era_code.is_empty() {
                YearInput::EraYear(
                    core::str::from_utf8(era_code).map_err(|_| CalendarError::UnknownEra)?,
                    year,
                )
            } else {
                YearInput::Extended(year)
            };
            let month = icu_calendar::types::Month::try_from_utf8(month_code)?;
            let cal = calendar.0.clone();
            Ok(Box::new(Date(icu_calendar::Date::try_new(
                input_year, month, day, cal,
            )?)))
        }

        /// Creates a new [`Date`] from the given Rata Die
        #[diplomat::rust_link(icu::calendar::Date::from_rata_die, FnInStruct)]
        #[diplomat::attr(all(supports = named_constructors), named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn from_rata_die(rd: i64, calendar: &Calendar) -> Result<Box<Date>, CalendarError> {
            let cal = calendar.0.clone();
            Ok(Box::new(Date(icu_calendar::Date::from_rata_die(
                icu_calendar::types::RataDie::new(rd),
                cal,
            ))))
        }

        /// Creates a new [`Date`] from an IXDTF string.
        #[diplomat::rust_link(icu::calendar::Date::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::Date::from_str, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_string(
            v: &DiplomatStr,
            calendar: &Calendar,
        ) -> Result<Box<Date>, Rfc9557ParseError> {
            Ok(Box::new(Date(icu_calendar::Date::try_from_utf8(
                v,
                calendar.0.clone(),
            )?)))
        }

        /// Convert this date to one in a different calendar
        #[diplomat::rust_link(icu::calendar::Date::to_calendar, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::convert_any, FnInStruct, hidden)]
        pub fn to_calendar(&self, calendar: &Calendar) -> Box<Date> {
            Box::new(Date(self.0.to_calendar(calendar.0.clone())))
        }

        /// Converts this date to ISO
        #[diplomat::rust_link(icu::calendar::Date::to_iso, FnInStruct)]
        pub fn to_iso(&self) -> Box<IsoDate> {
            Box::new(IsoDate(self.0.to_calendar(Iso)))
        }

        /// Returns this date's Rata Die
        #[diplomat::rust_link(icu::calendar::Date::to_rata_die, FnInStruct)]
        #[diplomat::attr(auto, getter = "rata_die")]
        pub fn to_rata_die(&self) -> i64 {
            self.0.to_rata_die().to_i64_date()
        }

        /// Returns the 1-indexed day in the year for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_year(&self) -> u16 {
            self.0.day_of_year().0
        }

        /// Returns the 1-indexed day in the month for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_month(&self) -> u8 {
            self.0.day_of_month().0
        }

        /// Returns the day in the week for this day
        ///
        /// This is *not* the day of the week, an ordinal number that is locale
        /// dependent.
        #[diplomat::rust_link(icu::calendar::Date::day_of_week, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        #[deprecated(note = "use `weekday`")]
        pub fn day_of_week(&self) -> Weekday {
            self.weekday()
        }

        /// Returns the day in the week for this day
        #[diplomat::rust_link(icu::calendar::Date::weekday, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        #[diplomat::attr(demo_gen, disable)] // covered by Date
        pub fn weekday(&self) -> Weekday {
            self.0.weekday().into()
        }

        /// Returns 1-indexed number of the month of this date in its year
        ///
        /// Note that for lunar calendars this may not lead to the same month
        /// having the same ordinal month across years; use `month_code` if you care
        /// about month identity.
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::ordinal, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn ordinal_month(&self) -> u8 {
            self.0.month().ordinal
        }

        /// Returns the month code for this date. Typically something
        /// like "M01", "M02", but can be more complicated for lunar calendars.
        #[diplomat::rust_link(icu::calendar::types::Month::code, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::standard_code, StructField)]
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct, compact)]
        #[diplomat::rust_link(icu::calendar::types::Month::formatting_code, FnInStruct, hidden)]
        #[diplomat::rust_link(
            icu::calendar::types::MonthInfo::formatting_code,
            StructField,
            hidden
        )]
        #[diplomat::rust_link(icu::calendar::types::Month, Struct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo, Struct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::to_input, FnInStruct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn month_code(&self, write: &mut diplomat_runtime::DiplomatWrite) {
            let code = self.0.month().to_input().code();
            let _infallible = write.write_str(&code.0);
        }

        /// Returns the month number of this month.
        #[diplomat::rust_link(icu::calendar::types::Month::number, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::number, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::month_number, FnInStruct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn month_number(&self) -> u8 {
            self.0.month().number()
        }

        /// Returns whether the month is a leap month.
        #[diplomat::rust_link(icu::calendar::types::Month::is_leap, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::is_leap, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::Month::is_formatting_leap, FnInStruct, hidden)]
        #[diplomat::rust_link(
            icu::calendar::types::MonthInfo::is_formatting_leap,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, getter)]
        pub fn month_is_leap(&self) -> bool {
            self.0.month().to_input().is_leap()
        }

        /// Returns the year number in the current era for this date
        ///
        /// For calendars without an era, returns the related ISO year.
        #[diplomat::rust_link(icu::calendar::types::YearInfo::era_year_or_related_iso, FnInEnum)]
        #[diplomat::rust_link(icu::calendar::types::EraYear::year, StructField, compact)]
        #[diplomat::rust_link(icu::calendar::types::CyclicYear::related_iso, StructField, compact)]
        #[diplomat::rust_link(icu::calendar::Date::year, FnInStruct, compact)]
        #[diplomat::rust_link(icu::calendar::Date::era_year, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::Date::cyclic_year, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo, Enum, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo::era, FnInEnum, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo::cyclic, FnInEnum, hidden)]
        #[diplomat::rust_link(icu::calendar::types::EraYear, Struct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::CyclicYear, Struct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn era_year_or_related_iso(&self) -> i32 {
            self.0.year().era_year_or_related_iso()
        }

        /// Returns the extended year, which can be used for
        ///
        /// This year number can be used when you need a simple numeric representation
        /// of the year, and can be meaningfully compared with extended years from other
        /// eras or used in arithmetic.
        #[diplomat::rust_link(icu::calendar::types::YearInfo::extended_year, FnInEnum)]
        #[diplomat::rust_link(icu::calendar::Date::extended_year, FnInStruct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn extended_year(&self) -> i32 {
            self.0.year().extended_year()
        }

        /// Returns the era for this date, or an empty string
        #[diplomat::rust_link(icu::calendar::types::EraYear::era, StructField)]
        #[diplomat::rust_link(icu::calendar::Date::year, FnInStruct, compact)]
        #[diplomat::attr(auto, getter)]
        pub fn era(&self, write: &mut diplomat_runtime::DiplomatWrite) {
            if let Some(era) = self.0.year().era() {
                let _infallible = write.write_str(&era.era);
            }
        }

        /// Returns the number of months in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::months_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn months_in_year(&self) -> u8 {
            self.0.months_in_year()
        }

        /// Returns the number of days in the month represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn days_in_month(&self) -> u8 {
            self.0.days_in_month()
        }

        /// Returns the number of days in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn days_in_year(&self) -> u16 {
            self.0.days_in_year()
        }

        /// Returns if the year is a leap year for this date
        #[diplomat::rust_link(icu::calendar::Date::is_in_leap_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn is_in_leap_year(&self) -> bool {
            self.0.is_in_leap_year()
        }

        /// Returns the [`Calendar`] object backing this date
        #[diplomat::rust_link(icu::calendar::Date::calendar, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::calendar_wrapper, FnInStruct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn calendar(&self) -> Box<Calendar> {
            Box::new(Calendar(self.0.calendar().clone()))
        }

        /// Returns a new [`Date`] with the given duration added to it.
        #[diplomat::rust_link(icu::calendar::Date::try_added_with_options, FnInStruct)]
        pub fn try_add_with_options(
            &self,
            duration: DateDuration,
            options: DateAddOptions,
        ) -> Result<Box<Date>, CalendarDateAddError> {
            // This is a cheap clone: DateInner is Copy, and AnyCalendar
            // while not Copy does not contain any types with destructors.
            //
            // It *may* add calendars with non-cheap clones in the future, but that will
            // be weighted against the performance of this FFI code.
            Ok(Box::new(Date(self.0.clone().try_added_with_options(
                duration.into(),
                options.into(),
            )?)))
        }

        /// Calculating the duration between `other - self`
        #[diplomat::rust_link(icu::calendar::Date::try_until_with_options, FnInStruct)]
        pub fn try_until_with_options(
            &self,
            other: &Date,
            options: DateDifferenceOptions,
        ) -> Result<DateDuration, CalendarMismatchedCalendarError> {
            Ok(self
                .0
                .try_until_with_options(&other.0, options.into())
                .map_err(|_| CalendarMismatchedCalendarError)?
                .into())
        }
    }

    #[diplomat::rust_link(icu::calendar::types::IsoWeekOfYear, Struct)]
    pub struct IsoWeekOfYear {
        pub week_number: u8,
        pub iso_year: i32,
    }
}

impl From<icu_calendar::types::IsoWeekOfYear> for IsoWeekOfYear {
    fn from(
        icu_calendar::types::IsoWeekOfYear {
            week_number,
            iso_year,
        }: icu_calendar::types::IsoWeekOfYear,
    ) -> Self {
        Self {
            week_number,
            iso_year,
        }
    }
}

impl From<ffi::DateFromFieldsOptions> for icu_calendar::options::DateFromFieldsOptions {
    fn from(other: ffi::DateFromFieldsOptions) -> Self {
        let mut options = Self::default();

        options.overflow = other.overflow.into_converted_option();
        options.missing_fields_strategy = other.missing_fields_strategy.into_converted_option();

        options
    }
}

impl<'a> From<ffi::DateFields<'a>> for icu_calendar::types::DateFields<'a> {
    fn from(other: ffi::DateFields<'a>) -> Self {
        let mut fields = Self::default();
        fields.era = other.era.into_option();
        fields.era_year = other.era_year.into();
        fields.extended_year = other.extended_year.into();
        fields.month_code = other.month_code.into_option();
        fields.ordinal_month = other.ordinal_month.into();
        fields.day = other.day.into();

        fields
    }
}

impl From<icu_calendar::types::DateDuration> for ffi::DateDuration {
    fn from(other: icu_calendar::types::DateDuration) -> Self {
        Self {
            is_negative: other.is_negative,
            years: other.years,
            months: other.months,
            weeks: other.weeks,
            days: other.days,
        }
    }
}

impl From<ffi::DateDuration> for icu_calendar::types::DateDuration {
    fn from(other: ffi::DateDuration) -> Self {
        Self {
            is_negative: other.is_negative,
            years: other.years,
            months: other.months,
            weeks: other.weeks,
            days: other.days,
        }
    }
}

impl From<ffi::DateAddOptions> for icu_calendar::options::DateAddOptions {
    fn from(other: ffi::DateAddOptions) -> Self {
        let mut options = Self::default();
        options.overflow = other.overflow.into_converted_option();
        options
    }
}

impl From<ffi::DateDifferenceOptions> for icu_calendar::options::DateDifferenceOptions {
    fn from(other: ffi::DateDifferenceOptions) -> Self {
        let mut options = Self::default();
        options.largest_unit = other.largest_unit.into_converted_option();
        options
    }
}
