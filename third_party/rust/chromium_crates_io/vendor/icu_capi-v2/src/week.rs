// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::unstable::date::ffi::Weekday;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::{errors::ffi::DataError, locale_core::ffi::Locale};

    /// A Week calculator, useful to be passed in to `week_of_year()` on Date and `DateTime` types
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::calendar::week::WeekInformation, Struct)]
    pub struct WeekInformation(pub icu_calendar::week::WeekInformation);

    impl WeekInformation {
        /// Creates a new [`WeekInformation`] from locale data using compiled data.
        #[diplomat::rust_link(icu::calendar::week::WeekInformation::try_new, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create(locale: &Locale) -> Result<Box<WeekInformation>, DataError> {
            let prefs = (&locale.0).into();

            Ok(Box::new(WeekInformation(
                icu_calendar::week::WeekInformation::try_new(prefs)?,
            )))
        }
        /// Creates a new [`WeekInformation`] from locale data using a particular data source.
        #[diplomat::rust_link(icu::calendar::week::WeekInformation::try_new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<WeekInformation>, DataError> {
            let prefs = (&locale.0).into();

            Ok(Box::new(WeekInformation(
                icu_calendar::week::WeekInformation::try_new_with_buffer_provider(
                    provider.get()?,
                    prefs,
                )?,
            )))
        }

        /// Returns the weekday that starts the week for this object's locale
        #[diplomat::rust_link(icu::calendar::week::WeekInformation::first_weekday, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn first_weekday(&self) -> Weekday {
            self.0.first_weekday.into()
        }

        #[diplomat::rust_link(icu::calendar::week::WeekInformation::weekend, StructField)]
        #[diplomat::rust_link(icu::calendar::provider::WeekdaySet::contains, FnInStruct)]
        pub fn is_weekend(&self, day: Weekday) -> bool {
            self.0.weekend.contains(day.into())
        }

        #[diplomat::rust_link(icu::calendar::week::WeekInformation::weekend, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn weekend(&self) -> Box<WeekdaySetIterator> {
            Box::new(WeekdaySetIterator(self.0.weekend()))
        }
    }

    /// Documents which days of the week are considered to be a part of the weekend
    #[diplomat::rust_link(icu::calendar::week::WeekdaySetIterator, Struct)]
    #[diplomat::opaque_mut]
    pub struct WeekdaySetIterator(icu_calendar::week::WeekdaySetIterator);

    impl WeekdaySetIterator {
        #[diplomat::attr(auto, iterator)]
        #[diplomat::rust_link(icu::calendar::week::WeekdaySetIterator::next, FnInStruct)]
        pub fn next(&mut self) -> Option<Weekday> {
            self.0.next().map(Into::into)
        }
    }
}
