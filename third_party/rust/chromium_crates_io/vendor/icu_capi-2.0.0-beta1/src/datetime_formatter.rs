// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_datetime::{
        fieldsets::{T, YMD, YMDT},
        options::Length,
    };

    use crate::{
        date::ffi::{Date, IsoDate},
        datetime::ffi::{DateTime, IsoDateTime},
        errors::ffi::{DateTimeFormatError, DateTimeFormatterLoadError},
        locale_core::ffi::Locale,
        provider::ffi::DataProvider,
        time::ffi::Time,
    };

    use writeable::Writeable;

    #[diplomat::opaque]
    /// An ICU4X TimeFormatter object capable of formatting an [`Time`] type (and others) as a string
    #[diplomat::rust_link(icu::datetime, Mod)]
    pub struct TimeFormatter(pub icu_datetime::FixedCalendarDateTimeFormatter<(), T>);

    #[diplomat::enum_convert(icu_datetime::options::Length, needs_wildcard)]
    #[diplomat::rust_link(icu::datetime::options::Length, Enum)]
    pub enum DateTimeLength {
        Long,
        Medium,
        Short,
    }

    impl TimeFormatter {
        /// Creates a new [`TimeFormatter`] from locale data.
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "with_length")]
        #[diplomat::demo(default_constructor)]
        pub fn create_with_length(
            provider: &DataProvider,
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<TimeFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = T::with_length(Length::from(length)).hm();

            Ok(Box::new(TimeFormatter(call_constructor!(
                icu_datetime::FixedCalendarDateTimeFormatter::try_new,
                icu_datetime::FixedCalendarDateTimeFormatter::try_new_with_any_provider,
                icu_datetime::FixedCalendarDateTimeFormatter::try_new_with_buffer_provider,
                provider,
                prefs,
                options
            )?)))
        }

        /// Formats a [`Time`] to a string.
        pub fn format_time(&self, value: &Time, write: &mut diplomat_runtime::DiplomatWrite) {
            let _infallible = self.0.format(&value.0).write_to(write);
        }

        /// Formats a [`DateTime`] to a string.
        pub fn format_datetime(
            &self,
            value: &DateTime,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            let _infallible = self.0.format(&value.0.time).write_to(write);
        }

        /// Formats a [`IsoDateTime`] to a string.
        pub fn format_iso_datetime(
            &self,
            value: &IsoDateTime,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            let _infallible = self.0.format(&value.0.time).write_to(write);
        }
    }

    #[diplomat::opaque]
    /// An ICU4X TypedDateFormatter object capable of formatting a [`IsoDateTime`] as a string,
    /// using the Gregorian Calendar.
    #[diplomat::rust_link(icu::datetime, Mod)]
    pub struct GregorianDateFormatter(
        pub icu_datetime::FixedCalendarDateTimeFormatter<icu_calendar::Gregorian, YMD>,
    );

    impl GregorianDateFormatter {
        /// Creates a new [`GregorianDateFormatter`] from locale data.
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "with_length")]
        #[diplomat::demo(default_constructor)]
        pub fn create_with_length(
            provider: &DataProvider,
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<GregorianDateFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMD::with_length(Length::from(length));

            Ok(Box::new(GregorianDateFormatter(call_constructor!(
                icu_datetime::FixedCalendarDateTimeFormatter::try_new,
                icu_datetime::FixedCalendarDateTimeFormatter::try_new_with_any_provider,
                icu_datetime::FixedCalendarDateTimeFormatter::try_new_with_buffer_provider,
                provider,
                prefs,
                options
            )?)))
        }

        /// Formats a [`IsoDate`] to a string.
        pub fn format_iso_date(
            &self,
            value: &IsoDate,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            let greg = icu_calendar::Date::new_from_iso(value.0, icu_calendar::Gregorian);
            let _infallible = self.0.format(&greg).write_to(write);
        }
        /// Formats a [`IsoDateTime`] to a string.
        pub fn format_iso_datetime(
            &self,
            value: &IsoDateTime,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            let greg = icu_calendar::DateTime::new_from_iso(value.0, icu_calendar::Gregorian);
            let _infallible = self.0.format(&greg).write_to(write);
        }
    }

    #[diplomat::opaque]
    /// An ICU4X FixedCalendarDateTimeFormatter object capable of formatting a [`IsoDateTime`] as a string,
    /// using the Gregorian Calendar.
    #[diplomat::rust_link(icu::datetime, Mod)]
    pub struct GregorianDateTimeFormatter(
        pub icu_datetime::FixedCalendarDateTimeFormatter<icu_calendar::Gregorian, YMDT>,
    );

    impl GregorianDateTimeFormatter {
        /// Creates a new [`GregorianDateFormatter`] from locale data.
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "with_length")]
        #[diplomat::demo(default_constructor)]
        pub fn create_with_length(
            provider: &DataProvider,
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<GregorianDateTimeFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMDT::with_length(Length::from(length)).hm();

            Ok(Box::new(GregorianDateTimeFormatter(call_constructor!(
                icu_datetime::FixedCalendarDateTimeFormatter::try_new,
                icu_datetime::FixedCalendarDateTimeFormatter::try_new_with_any_provider,
                icu_datetime::FixedCalendarDateTimeFormatter::try_new_with_buffer_provider,
                provider,
                prefs,
                options
            )?)))
        }

        /// Formats a [`IsoDateTime`] to a string.
        pub fn format_iso_datetime(
            &self,
            value: &IsoDateTime,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            let greg = icu_calendar::DateTime::new_from_iso(value.0, icu_calendar::Gregorian);
            let _infallible = self.0.format(&greg).write_to(write);
        }
    }

    #[diplomat::opaque]
    /// An ICU4X DateFormatter object capable of formatting a [`Date`] as a string,
    /// using some calendar specified at runtime in the locale.
    #[diplomat::rust_link(icu::datetime, Mod)]
    pub struct DateFormatter(pub icu_datetime::DateTimeFormatter<YMD>);

    impl DateFormatter {
        /// Creates a new [`DateFormatter`] from locale data.
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "with_length")]
        #[diplomat::demo(default_constructor)]
        pub fn create_with_length(
            provider: &DataProvider,
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<DateFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMD::with_length(Length::from(length));

            Ok(Box::new(DateFormatter(call_constructor!(
                icu_datetime::DateTimeFormatter::try_new,
                icu_datetime::DateTimeFormatter::try_new_with_any_provider,
                icu_datetime::DateTimeFormatter::try_new_with_buffer_provider,
                provider,
                prefs,
                options
            )?)))
        }

        /// Formats a [`Date`] to a string.
        pub fn format_date(
            &self,
            value: &Date,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let _infallible = self.0.format_any_calendar(&value.0).write_to(write);
            Ok(())
        }

        /// Formats a [`IsoDate`] to a string.
        ///
        /// Will convert to this formatter's calendar first
        pub fn format_iso_date(
            &self,
            value: &IsoDate,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let any = value.0.to_any();
            let _infallible = self.0.format_any_calendar(&any).write_to(write);
            Ok(())
        }

        /// Formats a [`DateTime`] to a string.
        pub fn format_datetime(
            &self,
            value: &DateTime,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let _infallible = self.0.format_any_calendar(&value.0).write_to(write);
            Ok(())
        }

        /// Formats a [`IsoDateTime`] to a string.
        ///
        /// Will convert to this formatter's calendar first
        pub fn format_iso_datetime(
            &self,
            value: &IsoDateTime,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let any = value.0.to_any();
            let _infallible = self.0.format_any_calendar(&any).write_to(write);
            Ok(())
        }
    }

    #[diplomat::opaque]
    /// An ICU4X DateFormatter object capable of formatting a [`DateTime`] as a string,
    /// using some calendar specified at runtime in the locale.
    #[diplomat::rust_link(icu::datetime, Mod)]
    pub struct DateTimeFormatter(pub icu_datetime::DateTimeFormatter<YMDT>);

    impl DateTimeFormatter {
        /// Creates a new [`DateTimeFormatter`] from locale data.
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "with_length")]
        #[diplomat::demo(default_constructor)]
        pub fn create_with_length(
            provider: &DataProvider,
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<DateTimeFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMDT::with_length(Length::from(length)).hm();

            Ok(Box::new(DateTimeFormatter(call_constructor!(
                icu_datetime::DateTimeFormatter::try_new,
                icu_datetime::DateTimeFormatter::try_new_with_any_provider,
                icu_datetime::DateTimeFormatter::try_new_with_buffer_provider,
                provider,
                prefs,
                options,
            )?)))
        }

        /// Formats a [`DateTime`] to a string.
        pub fn format_datetime(
            &self,
            value: &DateTime,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let _infallible = self.0.format_any_calendar(&value.0).write_to(write);
            Ok(())
        }

        /// Formats a [`IsoDateTime`] to a string.
        ///
        /// Will convert to this formatter's calendar first
        pub fn format_iso_datetime(
            &self,
            value: &IsoDateTime,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let any = value.0.to_any();
            let _infallible = self.0.format_any_calendar(&any).write_to(write);
            Ok(())
        }
    }
}
