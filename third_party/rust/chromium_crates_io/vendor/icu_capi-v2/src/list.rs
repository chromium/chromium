// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;
    use diplomat_runtime::{DiplomatStr16Slice, DiplomatStrSlice};
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use icu_list::{options::ListFormatterOptions, ListFormatterPreferences};

    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::{errors::ffi::DataError, locale_core::ffi::Locale};

    use writeable::Writeable;

    #[diplomat::rust_link(icu::list::options::ListLength, Enum)]
    #[diplomat::enum_convert(icu_list::options::ListLength, needs_wildcard)]
    #[non_exhaustive]
    pub enum ListLength {
        #[diplomat::attr(auto, default)]
        Wide,
        Short,
        Narrow,
    }
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::list::ListFormatter, Struct)]
    pub struct ListFormatter(pub icu_list::ListFormatter);

    impl ListFormatter {
        /// Construct a new `ListFormatter` instance for And patterns from compiled data.
        #[diplomat::rust_link(icu::list::ListFormatter::try_new_and, FnInStruct)]
        #[diplomat::rust_link(icu::list::options::ListFormatterOptions, Struct, hidden)]
        #[diplomat::rust_link(
            icu::list::options::ListFormatterOptions::with_length,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::list::options::ListFormatterOptions::default,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "and_with_length")]
        #[cfg(feature = "compiled_data")]
        #[diplomat::demo(default_constructor)]
        pub fn create_and_with_length(
            locale: &Locale,
            length: ListLength,
        ) -> Result<Box<ListFormatter>, DataError> {
            let prefs = ListFormatterPreferences::from(&locale.0);
            let options = ListFormatterOptions::default().with_length(length.into());
            Ok(Box::new(ListFormatter(
                icu_list::ListFormatter::try_new_and(prefs, options)?,
            )))
        }

        /// Construct a new `ListFormatter` instance for And patterns
        #[diplomat::rust_link(icu::list::ListFormatter::try_new_and, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "and_with_length_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_and_with_length_and_provider(
            provider: &DataProvider,
            locale: &Locale,
            length: ListLength,
        ) -> Result<Box<ListFormatter>, DataError> {
            let prefs = ListFormatterPreferences::from(&locale.0);
            let options = ListFormatterOptions::default().with_length(length.into());
            Ok(Box::new(ListFormatter(
                icu_list::ListFormatter::try_new_and_with_buffer_provider(
                    provider.get()?,
                    prefs,
                    options,
                )?,
            )))
        }

        /// Construct a new `ListFormatter` instance for And patterns from compiled data.
        #[diplomat::rust_link(icu::list::ListFormatter::try_new_or, FnInStruct)]
        #[diplomat::rust_link(icu::list::options::ListFormatterOptions, Struct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "or_with_length")]
        #[cfg(feature = "compiled_data")]
        pub fn create_or_with_length(
            locale: &Locale,
            length: ListLength,
        ) -> Result<Box<ListFormatter>, DataError> {
            let prefs = ListFormatterPreferences::from(&locale.0);
            let options = ListFormatterOptions::default().with_length(length.into());
            Ok(Box::new(ListFormatter(
                icu_list::ListFormatter::try_new_or(prefs, options)?,
            )))
        }

        /// Construct a new `ListFormatter` instance for And patterns
        #[diplomat::rust_link(icu::list::ListFormatter::try_new_or, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "or_with_length_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_or_with_length_and_provider(
            provider: &DataProvider,
            locale: &Locale,
            length: ListLength,
        ) -> Result<Box<ListFormatter>, DataError> {
            let prefs = ListFormatterPreferences::from(&locale.0);
            let options = ListFormatterOptions::default().with_length(length.into());
            Ok(Box::new(ListFormatter(
                icu_list::ListFormatter::try_new_or_with_buffer_provider(
                    provider.get()?,
                    prefs,
                    options,
                )?,
            )))
        }

        /// Construct a new `ListFormatter` instance for And patterns from compiled data.
        #[diplomat::rust_link(icu::list::ListFormatter::try_new_unit, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "unit_with_length")]
        #[cfg(feature = "compiled_data")]
        pub fn create_unit_with_length(
            locale: &Locale,
            length: ListLength,
        ) -> Result<Box<ListFormatter>, DataError> {
            let prefs = ListFormatterPreferences::from(&locale.0);
            let options = ListFormatterOptions::default().with_length(length.into());
            Ok(Box::new(ListFormatter(
                icu_list::ListFormatter::try_new_unit(prefs, options)?,
            )))
        }

        /// Construct a new `ListFormatter` instance for And patterns
        #[diplomat::rust_link(icu::list::ListFormatter::try_new_unit, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "unit_with_length_and_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_unit_with_length_and_provider(
            provider: &DataProvider,
            locale: &Locale,
            length: ListLength,
        ) -> Result<Box<ListFormatter>, DataError> {
            let prefs = ListFormatterPreferences::from(&locale.0);
            let options = ListFormatterOptions::default().with_length(length.into());
            Ok(Box::new(ListFormatter(
                icu_list::ListFormatter::try_new_unit_with_buffer_provider(
                    provider.get()?,
                    prefs,
                    options,
                )?,
            )))
        }

        #[diplomat::rust_link(icu::list::ListFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::list::ListFormatter::format_to_string, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::list::FormattedList, Struct, hidden)]
        #[diplomat::attr(not(supports = utf8_strings), disable)]
        #[diplomat::attr(*, rename = "format")]
        pub fn format_utf8(&self, list: &[DiplomatStrSlice], write: &mut DiplomatWrite) {
            let _infallible = self
                .0
                .format(
                    list.iter()
                        .map(|a| potential_utf::PotentialUtf8::from_bytes(a))
                        .map(writeable::adapters::LossyWrap),
                )
                .write_to(write);
        }

        #[diplomat::rust_link(icu::list::ListFormatter::format, FnInStruct)]
        #[diplomat::rust_link(icu::list::ListFormatter::format_to_string, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::list::FormattedList, Struct, hidden)]
        #[diplomat::attr(not(supports = utf8_strings), rename = "format")]
        #[diplomat::attr(supports = utf8_strings, rename = "format16")]
        pub fn format_utf16(&self, list: &[DiplomatStr16Slice], write: &mut DiplomatWrite) {
            let _infallible = self
                .0
                .format(
                    list.iter()
                        .map(|a| potential_utf::PotentialUtf16::from_slice(a))
                        .map(writeable::adapters::LossyWrap),
                )
                .write_to(write);
        }
    }
}
