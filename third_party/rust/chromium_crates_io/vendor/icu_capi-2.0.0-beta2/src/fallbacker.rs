// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::locale_core::ffi::Locale;
    #[cfg(feature = "buffer_provider")]
    use crate::{errors::ffi::DataError, provider::ffi::DataProvider};

    /// An object that runs the ICU4X locale fallback algorithm.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::locale::fallback::LocaleFallbacker, Struct)]
    #[diplomat::rust_link(icu::locale::fallback::LocaleFallbackerBorrowed, Struct, hidden)]
    pub struct LocaleFallbacker(pub icu_locale::LocaleFallbacker);

    /// Priority mode for the ICU4X fallback algorithm.
    #[diplomat::enum_convert(icu_locale::fallback::LocaleFallbackPriority, needs_wildcard)]
    #[diplomat::rust_link(icu::locale::fallback::LocaleFallbackPriority, Enum)]
    #[diplomat::rust_link(
        icu::locale::fallback::LocaleFallbackPriority::default,
        FnInEnum,
        hidden
    )]
    pub enum LocaleFallbackPriority {
        Language = 0,
        Region = 1,
    }

    /// Collection of configurations for the ICU4X fallback algorithm.
    #[diplomat::rust_link(icu::locale::fallback::LocaleFallbackConfig, Struct)]
    #[diplomat::rust_link(
        icu::locale::fallback::LocaleFallbackConfig::default,
        FnInStruct,
        hidden
    )]
    pub struct LocaleFallbackConfig {
        /// Choice of priority mode.
        pub priority: LocaleFallbackPriority,
    }

    /// An object that runs the ICU4X locale fallback algorithm with specific configurations.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::locale::fallback::LocaleFallbacker, Struct)]
    #[diplomat::rust_link(icu::locale::fallback::LocaleFallbackerWithConfig, Struct)]
    pub struct LocaleFallbackerWithConfig<'a>(
        pub icu_locale::fallback::LocaleFallbackerWithConfig<'a>,
    );

    /// An iterator over the locale under fallback.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::locale::fallback::LocaleFallbackIterator, Struct)]
    pub struct LocaleFallbackIterator<'a>(pub icu_locale::fallback::LocaleFallbackIterator<'a>);

    impl LocaleFallbacker {
        /// Creates a new `LocaleFallbacker` from compiled data.
        #[diplomat::rust_link(icu::locale::fallback::LocaleFallbacker::new, FnInStruct)]
        #[diplomat::rust_link(
            icu::locale::fallback::LocaleFallbackerBorrowed::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<LocaleFallbacker> {
            Box::new(LocaleFallbacker(
                icu_locale::LocaleFallbacker::new().static_to_owned(),
            ))
        }

        /// Creates a new `LocaleFallbacker` from a data provider.
        #[diplomat::rust_link(icu::locale::fallback::LocaleFallbacker::new, FnInStruct)]
        #[diplomat::rust_link(
            icu::locale::fallback::LocaleFallbackerBorrowed::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<LocaleFallbacker>, DataError> {
            Ok(Box::new(LocaleFallbacker(
                icu_locale::LocaleFallbacker::try_new_with_buffer_provider(provider.get()?)?,
            )))
        }

        /// Creates a new `LocaleFallbacker` without data for limited functionality.
        #[diplomat::rust_link(
            icu::locale::fallback::LocaleFallbacker::new_without_data,
            FnInStruct
        )]
        #[diplomat::attr(auto, named_constructor)]
        pub fn without_data() -> Box<LocaleFallbacker> {
            Box::new(LocaleFallbacker(
                icu_locale::LocaleFallbacker::new_without_data(),
            ))
        }

        /// Associates this `LocaleFallbacker` with configuration options.
        #[diplomat::rust_link(icu::locale::fallback::LocaleFallbacker::for_config, FnInStruct)]
        #[diplomat::rust_link(
            icu::locale::fallback::LocaleFallbackerBorrowed::for_config,
            FnInStruct,
            hidden
        )]
        pub fn for_config<'a>(
            &'a self,
            config: LocaleFallbackConfig,
        ) -> Box<LocaleFallbackerWithConfig<'a>> {
            Box::new(LocaleFallbackerWithConfig(self.0.for_config({
                let mut c = icu_locale::fallback::LocaleFallbackConfig::default();
                c.priority = config.priority.into();
                c
            })))
        }
    }

    impl<'a> LocaleFallbackerWithConfig<'a> {
        /// Creates an iterator from a locale with each step of fallback.
        #[diplomat::rust_link(icu::locale::fallback::LocaleFallbacker::fallback_for, FnInStruct)]
        #[diplomat::rust_link(
            icu::locale::fallback::LocaleFallbackerBorrowed::fallback_for,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::fallback::LocaleFallbackerWithConfig::fallback_for,
            FnInStruct,
            hidden
        )]
        pub fn fallback_for_locale<'b: 'a, 'temp>(
            &'b self,
            locale: &'temp Locale,
        ) -> Box<LocaleFallbackIterator<'a>> {
            Box::new(LocaleFallbackIterator(
                self.0.fallback_for((&locale.0).into()),
            ))
        }
    }

    impl<'a> LocaleFallbackIterator<'a> {
        #[diplomat::attr(auto, iterator)]
        #[diplomat::rust_link(
            icu::locale::fallback::LocaleFallbackIterator::get,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::fallback::LocaleFallbackIterator::step,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::fallback::LocaleFallbackIterator::take,
            FnInStruct,
            hidden
        )]
        pub fn next(&mut self) -> Option<Box<Locale>> {
            let current = self.0.get();
            if current.is_default() {
                None
            } else {
                let current = *current;
                self.0.step();
                Some(Box::new(Locale(current.into_locale())))
            }
        }
    }
}
