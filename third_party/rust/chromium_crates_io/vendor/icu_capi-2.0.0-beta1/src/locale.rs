// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::errors::ffi::DataError;
    use crate::locale_core::ffi::Locale;
    use crate::provider::ffi::DataProvider;

    #[diplomat::rust_link(icu::locale::TransformResult, Enum)]
    #[diplomat::enum_convert(icu_locale::TransformResult)]
    pub enum TransformResult {
        Modified,
        Unmodified,
    }

    /// A locale canonicalizer.
    #[diplomat::rust_link(icu::locale::LocaleCanonicalizer, Struct)]
    #[diplomat::opaque]
    pub struct LocaleCanonicalizer(icu_locale::LocaleCanonicalizer);

    impl LocaleCanonicalizer {
        /// Create a new [`LocaleCanonicalizer`].
        #[diplomat::rust_link(icu::locale::LocaleCanonicalizer::new, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        pub fn create(provider: &DataProvider) -> Result<Box<LocaleCanonicalizer>, DataError> {
            Ok(Box::new(LocaleCanonicalizer(call_constructor!(
                icu_locale::LocaleCanonicalizer::new [r => Ok(r)],
                icu_locale::LocaleCanonicalizer::try_new_with_any_provider,
                icu_locale::LocaleCanonicalizer::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Create a new [`LocaleCanonicalizer`] with extended data.
        #[diplomat::rust_link(icu::locale::LocaleCanonicalizer::new_with_expander, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "extended")]
        pub fn create_extended(
            provider: &DataProvider,
        ) -> Result<Box<LocaleCanonicalizer>, DataError> {
            let expander = call_constructor!(
                icu_locale::LocaleExpander::new_extended [r => Ok(r)],
                icu_locale::LocaleExpander::try_new_with_any_provider,
                icu_locale::LocaleExpander::try_new_with_buffer_provider,
                provider,
            )?;
            Ok(Box::new(LocaleCanonicalizer(call_constructor!(
                icu_locale::LocaleCanonicalizer::new_with_expander [r => Ok(r)],
                icu_locale::LocaleCanonicalizer::try_new_with_expander_with_any_provider,
                icu_locale::LocaleCanonicalizer::try_new_with_expander_with_buffer_provider,
                provider,
                expander
            )?)))
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
        /// Create a new [`LocaleExpander`].
        #[diplomat::rust_link(icu::locale::LocaleExpander::new, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        pub fn create(provider: &DataProvider) -> Result<Box<LocaleExpander>, DataError> {
            Ok(Box::new(LocaleExpander(call_constructor!(
                icu_locale::LocaleExpander::new [r => Ok(r)],
                icu_locale::LocaleExpander::try_new_with_any_provider,
                icu_locale::LocaleExpander::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Create a new [`LocaleExpander`] with extended data.
        #[diplomat::rust_link(icu::locale::LocaleExpander::new_extended, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "extended")]
        pub fn create_extended(provider: &DataProvider) -> Result<Box<LocaleExpander>, DataError> {
            Ok(Box::new(LocaleExpander(call_constructor!(
                icu_locale::LocaleExpander::new_extended [r => Ok(r)],
                icu_locale::LocaleExpander::try_new_with_any_provider,
                icu_locale::LocaleExpander::try_new_with_buffer_provider,
                provider,
            )?)))
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
