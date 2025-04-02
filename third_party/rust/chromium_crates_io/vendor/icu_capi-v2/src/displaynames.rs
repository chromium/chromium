// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::errors::ffi::DataError;
    use crate::errors::ffi::LocaleParseError;
    use crate::locale_core::ffi::Locale;
    #[cfg(feature = "buffer_provider")]
    use crate::provider::ffi::DataProvider;
    use diplomat_runtime::DiplomatOption;

    use writeable::Writeable;

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::displaynames::LocaleDisplayNamesFormatter, Struct)]
    pub struct LocaleDisplayNamesFormatter(
        pub icu_experimental::displaynames::LocaleDisplayNamesFormatter,
    );

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::displaynames::RegionDisplayNames, Struct)]
    pub struct RegionDisplayNames(pub icu_experimental::displaynames::RegionDisplayNames);

    #[diplomat::rust_link(icu::displaynames::options::DisplayNamesOptions, Struct)]
    #[diplomat::attr(supports = non_exhaustive_structs, rename = "DisplayNamesOptions")]
    pub struct DisplayNamesOptionsV1 {
        /// The optional formatting style to use for display name.
        pub style: DiplomatOption<DisplayNamesStyle>,
        /// The fallback return when the system does not have the
        /// requested display name, defaults to "code".
        pub fallback: DiplomatOption<DisplayNamesFallback>,
        /// The language display kind, defaults to "dialect".
        pub language_display: DiplomatOption<LanguageDisplay>,
    }

    #[diplomat::rust_link(icu::displaynames::options::Style, Enum)]
    #[diplomat::enum_convert(icu_experimental::displaynames::Style, needs_wildcard)]
    pub enum DisplayNamesStyle {
        Narrow,
        Short,
        Long,
        Menu,
    }

    #[diplomat::rust_link(icu::displaynames::options::Fallback, Enum)]
    #[diplomat::enum_convert(icu_experimental::displaynames::Fallback, needs_wildcard)]
    pub enum DisplayNamesFallback {
        Code,
        None,
    }

    #[diplomat::rust_link(icu::displaynames::options::LanguageDisplay, Enum)]
    #[diplomat::enum_convert(icu_experimental::displaynames::LanguageDisplay, needs_wildcard)]
    pub enum LanguageDisplay {
        Dialect,
        Standard,
    }

    impl LocaleDisplayNamesFormatter {
        /// Creates a new `LocaleDisplayNamesFormatter` from locale data and an options bag using compiled data.
        #[diplomat::rust_link(icu::displaynames::LocaleDisplayNamesFormatter::try_new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = non_exhaustive_structs), constructor)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors, not(supports = non_exhaustive_structs)), named_constructor = "v1")]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "create")]
        #[cfg(feature = "compiled_data")]
        pub fn create_v1(
            locale: &Locale,
            options: DisplayNamesOptionsV1,
        ) -> Result<Box<LocaleDisplayNamesFormatter>, DataError> {
            let prefs = (&locale.0).into();
            let options = icu_experimental::displaynames::DisplayNamesOptions::from(options);

            Ok(Box::new(LocaleDisplayNamesFormatter(
                icu_experimental::displaynames::LocaleDisplayNamesFormatter::try_new(
                    prefs, options,
                )?,
            )))
        }

        /// Creates a new `LocaleDisplayNamesFormatter` from locale data and an options bag using a particular data source.
        #[diplomat::rust_link(icu::displaynames::LocaleDisplayNamesFormatter::try_new, FnInStruct)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "create_with_provider")]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors, supports = non_exhaustive_structs), named_constructor = "with_provider")]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors, not(supports = non_exhaustive_structs)), named_constructor = "v1_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_v1_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            options: DisplayNamesOptionsV1,
        ) -> Result<Box<LocaleDisplayNamesFormatter>, DataError> {
            let prefs = (&locale.0).into();
            let options = icu_experimental::displaynames::DisplayNamesOptions::from(options);

            Ok(Box::new(LocaleDisplayNamesFormatter(
                icu_experimental::displaynames::LocaleDisplayNamesFormatter::try_new_with_buffer_provider(provider.get()?, prefs,
                    options,
                )?,
            )))
        }

        /// Returns the locale-specific display name of a locale.
        #[diplomat::rust_link(icu::displaynames::LocaleDisplayNamesFormatter::of, FnInStruct)]
        // Experimental, do not generate in demo:
        #[diplomat::attr(demo_gen, disable)]
        pub fn of(&self, locale: &Locale, write: &mut DiplomatWrite) {
            let _infallible = self.0.of(&locale.0).write_to(write);
        }
    }

    impl RegionDisplayNames {
        /// Creates a new `RegionDisplayNames` from locale data and an options bag using compiled data.
        #[diplomat::rust_link(icu::displaynames::RegionDisplayNames::try_new, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = non_exhaustive_structs), constructor)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors, not(supports = non_exhaustive_structs)), named_constructor = "v1")]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "create")]
        #[cfg(feature = "compiled_data")]
        pub fn create_v1(
            locale: &Locale,
            options: DisplayNamesOptionsV1,
        ) -> Result<Box<RegionDisplayNames>, DataError> {
            let prefs = (&locale.0).into();
            let options = icu_experimental::displaynames::DisplayNamesOptions::from(options);
            Ok(Box::new(RegionDisplayNames(
                icu_experimental::displaynames::RegionDisplayNames::try_new(prefs, options)?,
            )))
        }

        /// Creates a new `RegionDisplayNames` from locale data and an options bag using a particular data source.
        #[diplomat::rust_link(icu::displaynames::RegionDisplayNames::try_new, FnInStruct)]
        #[diplomat::attr(supports = non_exhaustive_structs, rename = "create_with_provider")]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors, supports = non_exhaustive_structs), named_constructor = "with_provider")]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors, not(supports = non_exhaustive_structs)), named_constructor = "v1_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_v1_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            options: DisplayNamesOptionsV1,
        ) -> Result<Box<RegionDisplayNames>, DataError> {
            let prefs = (&locale.0).into();
            let options = icu_experimental::displaynames::DisplayNamesOptions::from(options);
            Ok(Box::new(RegionDisplayNames(
                icu_experimental::displaynames::RegionDisplayNames::try_new_with_buffer_provider(
                    provider.get()?,
                    prefs,
                    options,
                )?,
            )))
        }

        /// Returns the locale specific display name of a region.
        /// Note that the function returns an empty string in case the display name for a given
        /// region code is not found.
        #[diplomat::rust_link(icu::displaynames::RegionDisplayNames::of, FnInStruct)]
        // Experimental, do not generate in demo:
        #[diplomat::attr(demo_gen, disable)]
        pub fn of(
            &self,
            region: &DiplomatStr,
            write: &mut DiplomatWrite,
        ) -> Result<(), LocaleParseError> {
            let _infallible = self
                .0
                .of(icu_locale_core::subtags::Region::try_from_utf8(region)?)
                .unwrap_or("")
                .write_to(write);
            Ok(())
        }
    }
}

impl From<ffi::DisplayNamesOptionsV1> for icu_experimental::displaynames::DisplayNamesOptions {
    fn from(
        other: ffi::DisplayNamesOptionsV1,
    ) -> icu_experimental::displaynames::DisplayNamesOptions {
        let mut options = icu_experimental::displaynames::DisplayNamesOptions::default();
        options.style = other.style.into_converted_option();
        options.fallback = other
            .fallback
            .into_converted_option()
            .unwrap_or(options.fallback);
        options.language_display = other
            .language_display
            .into_converted_option()
            .unwrap_or(options.language_display);
        options
    }
}
