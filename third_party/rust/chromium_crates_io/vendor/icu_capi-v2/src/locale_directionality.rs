// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::unstable::errors::ffi::DataError;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;

    use crate::unstable::locale_core::ffi::Locale;

    #[diplomat::rust_link(icu::locale::Direction, Enum)]
    #[non_exhaustive]
    pub enum LocaleDirection {
        LeftToRight,
        RightToLeft,
        // This is an output type, so the default mostly impacts deferred initialization.
        #[diplomat::attr(auto, default)]
        Unknown,
    }

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::locale::LocaleDirectionality, Struct)]
    pub struct LocaleDirectionality(pub icu_locale::LocaleDirectionality);

    impl LocaleDirectionality {
        /// Construct a new `LocaleDirectionality` instance using compiled data.
        #[diplomat::rust_link(icu::locale::LocaleDirectionality::new_common, FnInStruct)]
        #[diplomat::attr(supports = constructors, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create_common() -> Box<LocaleDirectionality> {
            Box::new(LocaleDirectionality(
                icu_locale::LocaleDirectionality::new_common(),
            ))
        }

        /// Construct a new `LocaleDirectionality` instance using a particular data source.
        #[diplomat::rust_link(icu::locale::LocaleDirectionality::new_common, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_common_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<LocaleDirectionality>, DataError> {
            Ok(Box::new(LocaleDirectionality(
                icu_locale::LocaleDirectionality::try_new_common_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }
        /// Construct a new `LocaleDirectionality` instance using compiled data.
        #[diplomat::rust_link(icu::locale::LocaleDirectionality::new_extended, FnInStruct)]
        #[diplomat::rust_link(
            icu::locale::LocaleDirectionality::new_with_expander,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, named_constructor = "extended")]
        #[cfg(feature = "compiled_data")]
        pub fn create_extended() -> Box<LocaleDirectionality> {
            Box::new(LocaleDirectionality(
                icu_locale::LocaleDirectionality::new_extended(),
            ))
        }

        /// Construct a new `LocaleDirectionality` instance using a particular data source.
        #[diplomat::rust_link(icu::locale::LocaleDirectionality::new_extended, FnInStruct)]
        #[diplomat::rust_link(
            icu::locale::LocaleDirectionality::new_with_expander,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "extended_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_extended_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<LocaleDirectionality>, DataError> {
            Ok(Box::new(LocaleDirectionality(
                icu_locale::LocaleDirectionality::try_new_extended_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }
        #[diplomat::rust_link(icu::locale::LocaleDirectionality::get, FnInStruct)]
        #[diplomat::attr(all(supports = indexing, not(kotlin)), indexer)] // Kotlin doesn't support non-integral indexers
        pub fn get(&self, locale: &Locale) -> LocaleDirection {
            match self.0.get(&locale.0.id) {
                Some(icu_locale::Direction::LeftToRight) => LocaleDirection::LeftToRight,
                Some(icu_locale::Direction::RightToLeft) => LocaleDirection::RightToLeft,
                _ => LocaleDirection::Unknown,
            }
        }

        #[diplomat::rust_link(icu::locale::LocaleDirectionality::is_left_to_right, FnInStruct)]
        #[diplomat::attr(demo_gen, disable)] // covered by `get`
        pub fn is_left_to_right(&self, locale: &Locale) -> bool {
            self.0.is_left_to_right(&locale.0.id)
        }

        #[diplomat::rust_link(icu::locale::LocaleDirectionality::is_right_to_left, FnInStruct)]
        #[diplomat::attr(demo_gen, disable)] // covered by `get`
        pub fn is_right_to_left(&self, locale: &Locale) -> bool {
            self.0.is_right_to_left(&locale.0.id)
        }
    }
}
