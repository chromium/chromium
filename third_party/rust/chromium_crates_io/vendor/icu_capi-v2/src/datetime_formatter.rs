// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use alloc::sync::Arc;
    use icu_datetime::fieldsets::{T, YMD};
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use icu_datetime::options::Length;

    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::errors::ffi::DateTimeFormatterLoadError;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::locale_core::ffi::Locale;
    #[cfg(feature = "buffer_provider")]
    use crate::provider::ffi::DataProvider;
    use crate::{
        calendar::ffi::Calendar,
        date::ffi::{Date, IsoDate},
        errors::ffi::DateTimeFormatError,
        time::ffi::Time,
    };

    use writeable::Writeable;

    #[diplomat::opaque]
    /// An ICU4X NoCalendarFormatter object capable of formatting an [`Time`] type (and others) as a string
    #[diplomat::rust_link(icu::datetime::NoCalendarFormatter, Typedef)]
    #[diplomat::rust_link(icu::datetime::fieldsets::T, Struct, compact)]
    pub struct NoCalendarFormatter(pub icu_datetime::NoCalendarFormatter<T>);

    #[diplomat::enum_convert(icu_datetime::options::Length, needs_wildcard)]
    #[diplomat::rust_link(icu::datetime::Length, Enum)]
    pub enum DateTimeLength {
        Long,
        Medium,
        Short,
    }

    impl NoCalendarFormatter {
        /// Creates a new [`NoCalendarFormatter`] using compiled data.
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_length")]
        #[diplomat::demo(default_constructor)]
        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter::try_new, FnInStruct)]
        pub fn create_with_length(
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<NoCalendarFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = T::with_length(Length::from(length)).hm();

            Ok(Box::new(NoCalendarFormatter(
                icu_datetime::FixedCalendarDateTimeFormatter::try_new(prefs, options)?,
            )))
        }

        /// Creates a new [`NoCalendarFormatter`] using a particular data source.
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_length_and_provider")]
        #[cfg(feature = "buffer_provider")]
        #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter::try_new, FnInStruct)]
        pub fn create_with_length_and_provider(
            provider: &DataProvider,
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<NoCalendarFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = T::with_length(Length::from(length)).hm();

            Ok(Box::new(NoCalendarFormatter(
                icu_datetime::FixedCalendarDateTimeFormatter::try_new_with_buffer_provider(
                    provider.get()?,
                    prefs,
                    options,
                )?,
            )))
        }

        /// Formats a [`Time`] to a string.
        #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::datetime::FormattedDateTime, Struct, hidden)]
        pub fn format(&self, value: &Time, write: &mut diplomat_runtime::DiplomatWrite) {
            let _infallible = self.0.format(&value.0).write_to(write);
        }
    }

    #[diplomat::opaque]
    /// An ICU4X TypedDateFormatter object capable of formatting an [`IsoDate`] and a [`Time`] as a string,
    /// using the Gregorian Calendar.
    #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter, Struct)]
    #[diplomat::rust_link(icu::datetime::fieldsets::YMD, Struct, compact)]
    pub struct GregorianDateFormatter(
        pub icu_datetime::FixedCalendarDateTimeFormatter<icu_calendar::Gregorian, YMD>,
    );

    impl GregorianDateFormatter {
        /// Creates a new [`GregorianDateFormatter`] using compiled data.
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_length")]
        #[diplomat::demo(default_constructor)]
        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter::try_new, FnInStruct)]
        pub fn create_with_length(
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<GregorianDateFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMD::with_length(Length::from(length));

            Ok(Box::new(GregorianDateFormatter(
                icu_datetime::FixedCalendarDateTimeFormatter::try_new(prefs, options)?,
            )))
        }

        /// Creates a new [`GregorianDateFormatter`] using a particular data source.
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_length_and_provider")]
        #[cfg(feature = "buffer_provider")]
        #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter::try_new, FnInStruct)]
        pub fn create_with_length_and_provider(
            provider: &DataProvider,
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<GregorianDateFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMD::with_length(Length::from(length));

            Ok(Box::new(GregorianDateFormatter(
                icu_datetime::FixedCalendarDateTimeFormatter::try_new_with_buffer_provider(
                    provider.get()?,
                    prefs,
                    options,
                )?,
            )))
        }

        /// Formats a [`IsoDate`] to a string.
        #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::datetime::FormattedDateTime, Struct, hidden)]
        pub fn format_iso(&self, value: &IsoDate, write: &mut diplomat_runtime::DiplomatWrite) {
            let greg = icu_calendar::Date::new_from_iso(value.0, icu_calendar::Gregorian);
            let _infallible = self.0.format(&greg).write_to(write);
        }
    }

    #[diplomat::opaque]
    /// An ICU4X DateFormatter object capable of formatting a [`Date`] as a string,
    /// using some calendar specified at runtime in the locale.
    #[diplomat::rust_link(icu::datetime::DateTimeFormatter, Struct)]
    #[diplomat::rust_link(icu::datetime::fieldsets::YMD, Struct, compact)]
    pub struct DateFormatter(pub icu_datetime::DateTimeFormatter<YMD>);

    impl DateFormatter {
        /// Creates a new [`DateFormatter`] using compiled data.
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_length")]
        #[diplomat::demo(default_constructor)]
        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(icu::datetime::DateTimeFormatter::try_new, FnInStruct)]
        pub fn create_with_length(
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<DateFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMD::with_length(Length::from(length));

            Ok(Box::new(DateFormatter(
                icu_datetime::DateTimeFormatter::try_new(prefs, options)?,
            )))
        }

        /// Creates a new [`DateFormatter`] using a particular data source.
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_length_and_provider")]
        #[cfg(feature = "buffer_provider")]
        #[diplomat::rust_link(icu::datetime::DateTimeFormatter::try_new, FnInStruct)]
        pub fn create_with_length_and_provider(
            provider: &DataProvider,
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<DateFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMD::with_length(Length::from(length));

            Ok(Box::new(DateFormatter(
                icu_datetime::DateTimeFormatter::try_new_with_buffer_provider(
                    provider.get()?,
                    prefs,
                    options,
                )?,
            )))
        }

        /// Formats a [`Date`] to a string.
        #[diplomat::rust_link(icu::datetime::DateTimeFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::datetime::FormattedDateTime, Struct, hidden)]
        pub fn format(
            &self,
            value: &Date,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let _infallible = self.0.format(&value.0).write_to(write);
            Ok(())
        }

        /// Formats a [`IsoDate`] to a string.
        ///
        /// Will convert to this formatter's calendar first
        #[diplomat::rust_link(icu::datetime::DateTimeFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::datetime::FormattedDateTime, Struct, hidden)]
        pub fn format_iso(
            &self,
            value: &IsoDate,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let any = value.0.to_any();
            let _infallible = self.0.format(&any).write_to(write);
            Ok(())
        }

        /// Returns the calendar system used in this formatter.
        #[diplomat::rust_link(icu::datetime::DateTimeFormatter::calendar, FnInStruct)]
        pub fn calendar(&self) -> Box<Calendar> {
            Box::new(Calendar(Arc::new(self.0.calendar().0.clone())))
        }
    }
}
