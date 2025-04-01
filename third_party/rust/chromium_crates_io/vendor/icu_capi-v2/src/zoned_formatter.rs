// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_datetime::fieldsets::{zone::GenericShort, Combo, YMDT};
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use icu_datetime::options::Length;

    #[cfg(feature = "buffer_provider")]
    use crate::provider::ffi::DataProvider;
    use crate::{
        date::ffi::{Date, IsoDate},
        errors::ffi::DateTimeFormatError,
        time::ffi::Time,
        timezone::ffi::TimeZoneInfo,
    };

    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::{
        datetime_formatter::ffi::DateTimeLength, errors::ffi::DateTimeFormatterLoadError,
        locale_core::ffi::Locale,
    };

    use writeable::Writeable;

    #[diplomat::opaque]
    /// An object capable of formatting a date time with time zone to a string.
    #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter, Struct)]
    #[diplomat::rust_link(icu::datetime::fieldsets::YMDT, Struct, compact)]
    #[diplomat::rust_link(icu::datetime::fieldsets::zone::GenericShort, Struct, compact)]
    pub struct GregorianZonedDateTimeFormatter(
        pub  icu_datetime::FixedCalendarDateTimeFormatter<
            icu_calendar::Gregorian,
            Combo<YMDT, GenericShort>,
        >,
    );

    impl GregorianZonedDateTimeFormatter {
        /// Creates a new [`GregorianZonedDateTimeFormatter`] from locale data using compiled data.
        ///
        /// This function has `date_length` and `time_length` arguments and uses default options
        /// for the time zone.
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_length")]
        #[diplomat::demo(default_constructor)]
        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter::try_new, FnInStruct)]
        pub fn create_with_length(
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<GregorianZonedDateTimeFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMDT::with_length(Length::from(length)).zone(GenericShort);

            Ok(Box::new(GregorianZonedDateTimeFormatter(
                icu_datetime::FixedCalendarDateTimeFormatter::try_new(prefs, options)?,
            )))
        }
        /// Creates a new [`GregorianZonedDateTimeFormatter`] from locale data using a particular data source.
        ///
        /// This function has `date_length` and `time_length` arguments and uses default options
        /// for the time zone.
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_length_and_provider")]
        #[cfg(feature = "buffer_provider")]
        #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter::try_new, FnInStruct)]
        pub fn create_with_length_and_provider(
            provider: &DataProvider,
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<GregorianZonedDateTimeFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMDT::with_length(Length::from(length)).zone(GenericShort);

            Ok(Box::new(GregorianZonedDateTimeFormatter(
                icu_datetime::FixedCalendarDateTimeFormatter::try_new_with_buffer_provider(
                    provider.get()?,
                    prefs,
                    options,
                )?,
            )))
        }
        /// Formats an [`IsoDate`] a [`Time`], and a [`TimeZoneInfo`] to a string.
        #[diplomat::rust_link(icu::datetime::FixedCalendarDateTimeFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::datetime::FormattedDateTime, Struct, hidden)]
        pub fn format_iso(
            &self,
            date: &IsoDate,
            time: &Time,
            zone: &TimeZoneInfo,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let zdt = icu_time::ZonedDateTime {
                date: icu_calendar::Date::new_from_iso(date.0, icu_calendar::Gregorian),
                time: time.0,
                zone: zone
                    .time_zone_id
                    .with_offset(zone.offset)
                    .at_time((date.0, time.0))
                    .with_zone_variant(
                        zone.zone_variant
                            .ok_or(DateTimeFormatError::ZoneInfoMissingFields)?,
                    ),
            };
            let _infallible = self.0.format(&zdt).write_to(write);
            Ok(())
        }
    }

    #[diplomat::opaque]
    /// An object capable of formatting a date time with time zone to a string.
    #[diplomat::rust_link(icu::datetime::DateTimeFormatter, Struct)]
    #[diplomat::rust_link(icu::datetime::fieldsets::YMDT, Struct, compact)]
    #[diplomat::rust_link(icu::datetime::fieldsets::zone::GenericShort, Struct, compact)]
    pub struct ZonedDateTimeFormatter(
        pub icu_datetime::DateTimeFormatter<Combo<YMDT, GenericShort>>,
    );

    impl ZonedDateTimeFormatter {
        /// Creates a new [`ZonedDateTimeFormatter`] from locale data using compiled data.
        ///
        /// This function has `date_length` and `time_length` arguments and uses default options
        /// for the time zone.
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_length")]
        #[diplomat::demo(default_constructor)]
        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(icu::datetime::DateTimeFormatter::try_new, FnInStruct)]
        pub fn create_with_length(
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<ZonedDateTimeFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMDT::with_length(Length::from(length)).zone(GenericShort);

            Ok(Box::new(ZonedDateTimeFormatter(
                icu_datetime::DateTimeFormatter::try_new(prefs, options)?,
            )))
        }
        /// Creates a new [`ZonedDateTimeFormatter`] from locale data using a particular data source.
        ///
        /// This function has `date_length` and `time_length` arguments and uses default options
        /// for the time zone.
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_length_and_provider")]
        #[cfg(feature = "buffer_provider")]
        #[diplomat::rust_link(icu::datetime::DateTimeFormatter::try_new, FnInStruct)]
        pub fn create_with_length_and_provider(
            provider: &DataProvider,
            locale: &Locale,
            length: DateTimeLength,
        ) -> Result<Box<ZonedDateTimeFormatter>, DateTimeFormatterLoadError> {
            let prefs = (&locale.0).into();
            let options = YMDT::with_length(Length::from(length)).zone(GenericShort);

            Ok(Box::new(ZonedDateTimeFormatter(
                icu_datetime::DateTimeFormatter::try_new_with_buffer_provider(
                    provider.get()?,
                    prefs,
                    options,
                )?,
            )))
        }
        /// Formats a [`Date`] a [`Time`], and a [`TimeZoneInfo`] to a string.
        #[diplomat::rust_link(icu::datetime::DateTimeFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::datetime::FormattedDateTime, Struct, hidden)]
        pub fn format(
            &self,
            date: &Date,
            time: &Time,
            zone: &TimeZoneInfo,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let zdt = icu_time::ZonedDateTime {
                date: date.0.wrap_calendar_in_ref(),
                time: time.0,
                zone: zone
                    .time_zone_id
                    .with_offset(zone.offset)
                    .at_time((date.0.to_iso(), time.0))
                    .with_zone_variant(
                        zone.zone_variant
                            .ok_or(DateTimeFormatError::ZoneInfoMissingFields)?,
                    ),
            };
            let _infallible = self.0.format(&zdt).write_to(write);
            Ok(())
        }

        /// Formats an [`IsoDate`] a [`Time`], and a [`TimeZoneInfo`] to a string.
        #[diplomat::rust_link(icu::datetime::DateTimeFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::datetime::FormattedDateTime, Struct, hidden)]
        pub fn format_iso(
            &self,
            date: &IsoDate,
            time: &Time,
            zone: &TimeZoneInfo,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DateTimeFormatError> {
            let zdt = icu_time::ZonedDateTime {
                date: date.0,
                time: time.0,
                zone: zone
                    .time_zone_id
                    .with_offset(zone.offset)
                    .at_time((date.0, time.0))
                    .with_zone_variant(
                        zone.zone_variant
                            .ok_or(DateTimeFormatError::ZoneInfoMissingFields)?,
                    ),
            };
            let _infallible = self.0.format(&zdt).write_to(write);
            Ok(())
        }
    }
}
