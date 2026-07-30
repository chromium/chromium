// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::unstable::locale_core::ffi::Locale;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::{errors::ffi::DataError, provider::ffi::DataProvider};

    #[diplomat::rust_link(icu::locale::TransformResult, Enum)]
    #[diplomat::enum_convert(icu_locale::TransformResult)]
    #[non_exhaustive]
    pub enum TransformResult {
        Modified,
        // This is an output type, so the default mostly impacts deferred initialization.
        #[diplomat::attr(auto, default)]
        Unmodified,
    }

    /// A locale canonicalizer.
    #[diplomat::rust_link(icu::locale::LocaleCanonicalizer, Struct)]
    #[diplomat::opaque]
    #[diplomat::demo(custom_func = "../../../tools/web-demo/custom/LocaleCanonicalizer.mjs")]
    pub struct LocaleCanonicalizer(icu_locale::LocaleCanonicalizer);

    impl LocaleCanonicalizer {
        /// Create a new [`LocaleCanonicalizer`] using compiled data.
        #[diplomat::rust_link(icu::locale::LocaleCanonicalizer::new_common, FnInStruct)]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create_common() -> Box<LocaleCanonicalizer> {
            Box::new(LocaleCanonicalizer(
                icu_locale::LocaleCanonicalizer::new_common(),
            ))
        }
        /// Create a new [`LocaleCanonicalizer`].
        #[diplomat::rust_link(icu::locale::LocaleCanonicalizer::new_common, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_common_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<LocaleCanonicalizer>, DataError> {
            Ok(Box::new(LocaleCanonicalizer(
                icu_locale::LocaleCanonicalizer::try_new_common_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }
        /// Create a new [`LocaleCanonicalizer`] with extended data using compiled data.
        #[diplomat::rust_link(icu::locale::LocaleCanonicalizer::new_extended, FnInStruct)]
        #[diplomat::rust_link(
            icu::locale::LocaleCanonicalizer::new_with_expander,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, named_constructor = "extended")]
        #[cfg(feature = "compiled_data")]
        pub fn create_extended() -> Box<LocaleCanonicalizer> {
            Box::new(LocaleCanonicalizer(
                icu_locale::LocaleCanonicalizer::new_extended(),
            ))
        }
        /// Create a new [`LocaleCanonicalizer`] with extended data.
        #[diplomat::rust_link(icu::locale::LocaleCanonicalizer::new_extended, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "extended_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_extended_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<LocaleCanonicalizer>, DataError> {
            Ok(Box::new(LocaleCanonicalizer(
                icu_locale::LocaleCanonicalizer::try_new_extended_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }
        #[diplomat::rust_link(icu::locale::LocaleCanonicalizer::canonicalize, FnInStruct)]
        pub fn canonicalize(&self, locale: &mut Locale) -> TransformResult {
            self.0.canonicalize(&mut locale.0).into()
        }
    }

    /// A locale expander.
    #[diplomat::rust_link(icu::locale::LocaleExpander, Struct)]
    #[diplomat::opaque]
    pub struct LocaleExpander(pub icu_locale::LocaleExpander);

    impl LocaleExpander {
        /// Create a new [`LocaleExpander`] using compiled data.
        #[diplomat::rust_link(icu::locale::LocaleExpander::new_common, FnInStruct)]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create_common() -> Box<LocaleExpander> {
            Box::new(LocaleExpander(icu_locale::LocaleExpander::new_common()))
        }
        /// Create a new [`LocaleExpander`] using a `new_common` data source.
        #[diplomat::rust_link(icu::locale::LocaleExpander::new_common, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_common_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<LocaleExpander>, DataError> {
            Ok(Box::new(LocaleExpander(
                icu_locale::LocaleExpander::try_new_common_with_buffer_provider(provider.get()?)?,
            )))
        }
        /// Create a new [`LocaleExpander`] with extended data using compiled data.
        #[diplomat::rust_link(icu::locale::LocaleExpander::new_extended, FnInStruct)]
        #[diplomat::attr(auto, named_constructor = "extended")]
        #[cfg(feature = "compiled_data")]
        pub fn create_extended() -> Box<LocaleExpander> {
            Box::new(LocaleExpander(icu_locale::LocaleExpander::new_extended()))
        }
        /// Create a new [`LocaleExpander`] with extended data using a particular data source.
        #[diplomat::rust_link(icu::locale::LocaleExpander::new_extended, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "extended_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_extended_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<LocaleExpander>, DataError> {
            Ok(Box::new(LocaleExpander(
                icu_locale::LocaleExpander::try_new_extended_with_buffer_provider(provider.get()?)?,
            )))
        }
        #[diplomat::rust_link(icu::locale::LocaleExpander::maximize, FnInStruct)]
        pub fn maximize(&self, locale: &mut Locale) -> TransformResult {
            self.0.maximize(&mut locale.0.id).into()
        }

        #[diplomat::rust_link(icu::locale::LocaleExpander::minimize, FnInStruct)]
        pub fn minimize(&self, locale: &mut Locale) -> TransformResult {
            self.0.minimize(&mut locale.0.id).into()
        }

        #[diplomat::rust_link(icu::locale::LocaleExpander::minimize_favor_script, FnInStruct)]
        pub fn minimize_favor_script(&self, locale: &mut Locale) -> TransformResult {
            self.0.minimize_favor_script(&mut locale.0.id).into()
        }
    }
}
