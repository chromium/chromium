// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::{errors::ffi::DataError, locale_core::ffi::Locale};

    #[diplomat::opaque]
    /// A set of "exemplar characters" for a given locale.
    #[diplomat::rust_link(icu::locale, Mod)]
    #[diplomat::rust_link(icu::locale::exemplar_chars::ExemplarCharacters, Struct)]
    #[diplomat::rust_link(icu::locale::exemplar_chars::ExemplarCharactersBorrowed, Struct)]
    pub struct ExemplarCharacters(pub icu_locale::exemplar_chars::ExemplarCharacters);

    impl ExemplarCharacters {
        /// Checks whether the string is in the set.
        #[diplomat::rust_link(
            icu::collections::codepointinvliststringlist::CodePointInversionListAndStringList::contains_str,
            FnInStruct
        )]
        #[diplomat::attr(supports = method_overloading, rename = "contains")]
        pub fn contains_str(&self, s: &DiplomatStr) -> bool {
            self.0.as_borrowed().contains_utf8(s)
        }
        /// Checks whether the code point is in the set.
        #[diplomat::rust_link(
            icu::collections::codepointinvliststringlist::CodePointInversionListAndStringList::contains,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::collections::codepointinvliststringlist::CodePointInversionListAndStringList::contains32,
            FnInStruct,
            hidden
        )]
        pub fn contains(&self, cp: DiplomatChar) -> bool {
            self.0.as_borrowed().contains32(cp)
        }

        /// Create an [`ExemplarCharacters`] for the "main" set of exemplar characters for a given locale, using compiled data.
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharacters::try_new_main,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharactersBorrowed::try_new_main,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "main")]
        #[cfg(feature = "compiled_data")]
        #[diplomat::demo(default_constructor)]
        pub fn create_main(locale: &Locale) -> Result<Box<ExemplarCharacters>, DataError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ExemplarCharacters(
                icu_locale::exemplar_chars::ExemplarCharacters::try_new_main(&locale)?
                    .static_to_owned(),
            )))
        }

        /// Create an [`ExemplarCharacters`] for the "main" set of exemplar characters for a given locale, using a particular data source
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharacters::try_new_main,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "main_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_main_with_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<ExemplarCharacters>, DataError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ExemplarCharacters(
                icu_locale::exemplar_chars::ExemplarCharacters::try_new_main_unstable(
                    &provider.get_unstable()?,
                    &locale,
                )?,
            )))
        }

        /// Create an [`ExemplarCharacters`] for the "auxiliary" set of exemplar characters for a given locale, using compiled data.
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharacters::try_new_auxiliary,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharactersBorrowed::try_new_auxiliary,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "auxiliary")]
        #[cfg(feature = "compiled_data")]
        pub fn create_auxiliary(locale: &Locale) -> Result<Box<ExemplarCharacters>, DataError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ExemplarCharacters(
                icu_locale::exemplar_chars::ExemplarCharacters::try_new_auxiliary(&locale)?
                    .static_to_owned(),
            )))
        }
        /// Create an [`ExemplarCharacters`] for the "auxiliary" set of exemplar characters for a given locale, using compiled data.
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharacters::try_new_auxiliary,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "auxiliary_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_auxiliary_with_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<ExemplarCharacters>, DataError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ExemplarCharacters(
                icu_locale::exemplar_chars::ExemplarCharacters::try_new_auxiliary_unstable(
                    &provider.get_unstable()?,
                    &locale,
                )?,
            )))
        }

        /// Create an [`ExemplarCharacters`] for the "punctuation" set of exemplar characters for a given locale, using compiled data.
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharacters::try_new_punctuation,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharactersBorrowed::try_new_punctuation,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "punctuation")]
        #[cfg(feature = "compiled_data")]
        pub fn create_punctuation(locale: &Locale) -> Result<Box<ExemplarCharacters>, DataError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ExemplarCharacters(
                icu_locale::exemplar_chars::ExemplarCharacters::try_new_punctuation(&locale)?
                    .static_to_owned(),
            )))
        }
        /// Create an [`ExemplarCharacters`] for the "punctuation" set of exemplar characters for a given locale, using compiled data.
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharacters::try_new_punctuation,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "punctuation_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_punctuation_with_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<ExemplarCharacters>, DataError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ExemplarCharacters(
                icu_locale::exemplar_chars::ExemplarCharacters::try_new_punctuation_unstable(
                    &provider.get_unstable()?,
                    &locale,
                )?,
            )))
        }

        /// Create an [`ExemplarCharacters`] for the "numbers" set of exemplar characters for a given locale, using compiled data.
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharacters::try_new_numbers,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharactersBorrowed::try_new_numbers,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "numbers")]
        #[cfg(feature = "compiled_data")]
        pub fn create_numbers(locale: &Locale) -> Result<Box<ExemplarCharacters>, DataError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ExemplarCharacters(
                icu_locale::exemplar_chars::ExemplarCharacters::try_new_numbers(&locale)?
                    .static_to_owned(),
            )))
        }

        /// Create an [`ExemplarCharacters`] for the "numbers" set of exemplar characters for a given locale, using compiled data.
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharacters::try_new_numbers,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "numbers_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_numbers_with_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<ExemplarCharacters>, DataError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ExemplarCharacters(
                icu_locale::exemplar_chars::ExemplarCharacters::try_new_numbers_unstable(
                    &provider.get_unstable()?,
                    &locale,
                )?,
            )))
        }

        /// Create an [`ExemplarCharacters`] for the "index" set of exemplar characters for a given locale, using compiled data.
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharacters::try_new_index,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharactersBorrowed::try_new_index,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "index")]
        #[cfg(feature = "compiled_data")]
        pub fn create_index(locale: &Locale) -> Result<Box<ExemplarCharacters>, DataError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ExemplarCharacters(
                icu_locale::exemplar_chars::ExemplarCharacters::try_new_index(&locale)?
                    .static_to_owned(),
            )))
        }

        /// Create an [`ExemplarCharacters`] for the "index" set of exemplar characters for a given locale, using compiled data.
        #[diplomat::rust_link(
            icu::locale::exemplar_chars::ExemplarCharacters::try_new_index,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "index_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_index_with_provider(
            provider: &DataProvider,
            locale: &Locale,
        ) -> Result<Box<ExemplarCharacters>, DataError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ExemplarCharacters(
                icu_locale::exemplar_chars::ExemplarCharacters::try_new_index_unstable(
                    &provider.get_unstable()?,
                    &locale,
                )?,
            )))
        }
    }
}
