// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::errors::ffi::DataError;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::locale_core::ffi::Locale;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use diplomat_runtime::DiplomatStr;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use writeable::Writeable;

    #[diplomat::rust_link(icu::locale::names::LanguageDisplay, Enum)]
    #[diplomat::enum_convert(icu_locale::names::LanguageDisplay, needs_wildcard)]
    #[non_exhaustive]
    pub enum LanguageDisplayUnstable {
        #[diplomat::attr(auto, default)]
        Dialect,
        Standard,
    }

    ///
    /// This struct holds free functions for loading display names for languages, scripts,
    /// regions, and language identifiers.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::locale::names::RegionDisplayName, Struct)]
    #[diplomat::rust_link(icu::locale::names::ScriptDisplayName, Struct)]
    #[diplomat::rust_link(icu::locale::names::VariantDisplayName, Struct)]
    #[diplomat::rust_link(icu::locale::names::LanguageIdentifierDisplayName, Struct)]
    pub struct LocaleNamesUnstable;

    impl LocaleNamesUnstable {
        // --- Region ---

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::RegionDisplayName::new_light_with_fallback,
            FnInStruct
        )]
        #[diplomat::rust_link(icu::locale::names::RegionDisplayName::write_to, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::locale::names::RegionDisplayName::to_string, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::locale::names::RegionDisplayNameBorrowed, Struct, hidden)]
        #[diplomat::rust_link(
            icu::locale::names::RegionDisplayNameBorrowed::write_to,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::names::RegionDisplayNameBorrowed::to_string,
            FnInStruct,
            hidden
        )]
        pub fn for_region_light(
            locale: &Locale,
            region: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let region = icu_locale_core::subtags::Region::try_from_utf8(region)
                .unwrap_or(icu_locale_core::subtags::region!("ZZ"));
            let display_name = icu_locale::names::RegionDisplayName::new_light_with_fallback(
                (&locale.0).into(),
                region,
            );
            let _infallible = display_name.write_to(write);
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::RegionDisplayName::new_light_with_fallback,
            FnInStruct
        )]
        pub fn for_region_light_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            region: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let region = icu_locale_core::subtags::Region::try_from_utf8(region)
                .unwrap_or(icu_locale_core::subtags::region!("ZZ"));
            let display_name =
                icu_locale::names::RegionDisplayName::try_new_light_with_buffer_provider(
                    provider.get()?,
                    (&locale.0).into(),
                    region,
                )?;
            let _infallible = display_name.write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::RegionDisplayName::new_tiny_with_fallback,
            FnInStruct
        )]
        pub fn for_region_tiny(
            locale: &Locale,
            region: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let region = icu_locale_core::subtags::Region::try_from_utf8(region)
                .unwrap_or(icu_locale_core::subtags::region!("ZZ"));
            let display_name = icu_locale::names::RegionDisplayName::new_tiny_with_fallback(
                (&locale.0).into(),
                region,
            );
            let _infallible = display_name.write_to(write);
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::RegionDisplayName::new_tiny_with_fallback,
            FnInStruct
        )]
        pub fn for_region_tiny_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            region: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let region = icu_locale_core::subtags::Region::try_from_utf8(region)
                .unwrap_or(icu_locale_core::subtags::region!("ZZ"));
            let display_name =
                icu_locale::names::RegionDisplayName::try_new_tiny_with_buffer_provider(
                    provider.get()?,
                    (&locale.0).into(),
                    region,
                )?;
            let _infallible = display_name.write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::RegionDisplayName::new_short_tiny_with_fallback,
            FnInStruct
        )]
        pub fn for_region_short_tiny(
            locale: &Locale,
            region: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let region = icu_locale_core::subtags::Region::try_from_utf8(region)
                .unwrap_or(icu_locale_core::subtags::region!("ZZ"));
            let display_name = icu_locale::names::RegionDisplayName::new_short_tiny_with_fallback(
                (&locale.0).into(),
                region,
            );
            let _infallible = display_name.write_to(write);
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::RegionDisplayName::new_short_tiny_with_fallback,
            FnInStruct
        )]
        pub fn for_region_short_tiny_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            region: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let region = icu_locale_core::subtags::Region::try_from_utf8(region)
                .unwrap_or(icu_locale_core::subtags::region!("ZZ"));
            let display_name =
                icu_locale::names::RegionDisplayName::try_new_short_tiny_with_buffer_provider(
                    provider.get()?,
                    (&locale.0).into(),
                    region,
                )?;
            let _infallible = display_name.write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::RegionDisplayName::new_short_light_with_fallback,
            FnInStruct
        )]
        pub fn for_region_short_light(
            locale: &Locale,
            region: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let region = icu_locale_core::subtags::Region::try_from_utf8(region)
                .unwrap_or(icu_locale_core::subtags::region!("ZZ"));
            let display_name = icu_locale::names::RegionDisplayName::new_short_light_with_fallback(
                (&locale.0).into(),
                region,
            );
            let _infallible = display_name.write_to(write);
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::RegionDisplayName::new_short_light_with_fallback,
            FnInStruct
        )]
        pub fn for_region_short_light_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            region: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let region = icu_locale_core::subtags::Region::try_from_utf8(region)
                .unwrap_or(icu_locale_core::subtags::region!("ZZ"));
            let display_name =
                icu_locale::names::RegionDisplayName::try_new_short_light_with_buffer_provider(
                    provider.get()?,
                    (&locale.0).into(),
                    region,
                )?;
            let _infallible = display_name.write_to(write);
            Ok(())
        }

        // --- Script ---

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::ScriptDisplayName::new_light_with_fallback,
            FnInStruct
        )]
        #[diplomat::rust_link(icu::locale::names::ScriptDisplayName::write_to, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::locale::names::ScriptDisplayName::to_string, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::locale::names::ScriptDisplayNameBorrowed, Struct, hidden)]
        #[diplomat::rust_link(
            icu::locale::names::ScriptDisplayNameBorrowed::write_to,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::names::ScriptDisplayNameBorrowed::to_string,
            FnInStruct,
            hidden
        )]
        pub fn for_script_light(
            locale: &Locale,
            script: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let script = icu_locale_core::subtags::Script::try_from_utf8(script)
                .unwrap_or(icu_locale_core::subtags::script!("Zzzz"));
            let display_name = icu_locale::names::ScriptDisplayName::new_light_with_fallback(
                (&locale.0).into(),
                script,
            );
            let _infallible = display_name.write_to(write);
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::ScriptDisplayName::new_light_with_fallback,
            FnInStruct
        )]
        pub fn for_script_light_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            script: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let script = icu_locale_core::subtags::Script::try_from_utf8(script)
                .unwrap_or(icu_locale_core::subtags::script!("Zzzz"));
            let display_name =
                icu_locale::names::ScriptDisplayName::try_new_light_with_buffer_provider(
                    provider.get()?,
                    (&locale.0).into(),
                    script,
                )?;
            let _infallible = display_name.write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::ScriptDisplayName::new_tiny_with_fallback,
            FnInStruct
        )]
        pub fn for_script_tiny(
            locale: &Locale,
            script: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let script = icu_locale_core::subtags::Script::try_from_utf8(script)
                .unwrap_or(icu_locale_core::subtags::script!("Zzzz"));
            let display_name = icu_locale::names::ScriptDisplayName::new_tiny_with_fallback(
                (&locale.0).into(),
                script,
            );
            let _infallible = display_name.write_to(write);
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::ScriptDisplayName::new_tiny_with_fallback,
            FnInStruct
        )]
        pub fn for_script_tiny_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            script: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let script = icu_locale_core::subtags::Script::try_from_utf8(script)
                .unwrap_or(icu_locale_core::subtags::script!("Zzzz"));
            let display_name =
                icu_locale::names::ScriptDisplayName::try_new_tiny_with_buffer_provider(
                    provider.get()?,
                    (&locale.0).into(),
                    script,
                )?;
            let _infallible = display_name.write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::ScriptDisplayName::new_heavy_with_fallback,
            FnInStruct
        )]
        pub fn for_script_heavy(
            locale: &Locale,
            script: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let script = icu_locale_core::subtags::Script::try_from_utf8(script)
                .unwrap_or(icu_locale_core::subtags::script!("Zzzz"));
            let display_name = icu_locale::names::ScriptDisplayName::new_heavy_with_fallback(
                (&locale.0).into(),
                script,
            );
            let _infallible = display_name.write_to(write);
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::ScriptDisplayName::new_heavy_with_fallback,
            FnInStruct
        )]
        pub fn for_script_heavy_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            script: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let script = icu_locale_core::subtags::Script::try_from_utf8(script)
                .unwrap_or(icu_locale_core::subtags::script!("Zzzz"));
            let display_name =
                icu_locale::names::ScriptDisplayName::try_new_heavy_with_buffer_provider(
                    provider.get()?,
                    (&locale.0).into(),
                    script,
                )?;
            let _infallible = display_name.write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::ScriptDisplayName::new_short_heavy_with_fallback,
            FnInStruct
        )]
        pub fn for_script_short_heavy(
            locale: &Locale,
            script: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let script = icu_locale_core::subtags::Script::try_from_utf8(script)
                .unwrap_or(icu_locale_core::subtags::script!("Zzzz"));
            let display_name = icu_locale::names::ScriptDisplayName::new_short_heavy_with_fallback(
                (&locale.0).into(),
                script,
            );
            let _infallible = display_name.write_to(write);
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::ScriptDisplayName::new_short_heavy_with_fallback,
            FnInStruct
        )]
        pub fn for_script_short_heavy_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            script: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let script = icu_locale_core::subtags::Script::try_from_utf8(script)
                .unwrap_or(icu_locale_core::subtags::script!("Zzzz"));
            let display_name =
                icu_locale::names::ScriptDisplayName::try_new_short_heavy_with_buffer_provider(
                    provider.get()?,
                    (&locale.0).into(),
                    script,
                )?;
            let _infallible = display_name.write_to(write);
            Ok(())
        }

        // --- Variant ---

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::VariantDisplayName::new_heavy_with_fallback,
            FnInStruct
        )]
        #[diplomat::rust_link(icu::locale::names::VariantDisplayName::write_to, FnInStruct, hidden)]
        #[diplomat::rust_link(
            icu::locale::names::VariantDisplayName::to_string,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(icu::locale::names::VariantDisplayNameBorrowed, Struct, hidden)]
        #[diplomat::rust_link(
            icu::locale::names::VariantDisplayNameBorrowed::write_to,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::names::VariantDisplayNameBorrowed::to_string,
            FnInStruct,
            hidden
        )]
        pub fn for_variant_heavy(
            locale: &Locale,
            variant: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let variant = icu_locale_core::subtags::Variant::try_from_utf8(variant)
                .unwrap_or(icu_locale_core::subtags::variant!("posix"));
            let display_name = icu_locale::names::VariantDisplayName::new_heavy_with_fallback(
                (&locale.0).into(),
                variant,
            );
            let _infallible = display_name.write_to(write);
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::VariantDisplayName::new_heavy_with_fallback,
            FnInStruct
        )]
        pub fn for_variant_heavy_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            variant: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            // TODO: Consider returning LocaleParseError or a dedicated DataError variant for invalid subtag syntax.
            let variant = icu_locale_core::subtags::Variant::try_from_utf8(variant)
                .unwrap_or(icu_locale_core::subtags::variant!("posix"));
            let display_name =
                icu_locale::names::VariantDisplayName::try_new_heavy_with_buffer_provider(
                    provider.get()?,
                    (&locale.0).into(),
                    variant,
                )?;
            let _infallible = display_name.write_to(write);
            Ok(())
        }

        // --- Language Identifier ---

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_light,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayNameOptions,
            Struct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierNameFallbackError,
            Struct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayNameBorrowed,
            Struct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayNameBorrowed::to_string,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayNameBorrowed::write_to,
            FnInStruct,
            hidden
        )]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayNameBorrowed::writeable_length_hint,
            FnInStruct,
            hidden
        )]
        pub fn for_language_identifier_light(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_light(
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_light,
            FnInStruct
        )]
        pub fn for_language_identifier_light_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_light_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_tiny,
            FnInStruct
        )]
        pub fn for_language_identifier_tiny(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_tiny(
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_tiny,
            FnInStruct
        )]
        pub fn for_language_identifier_tiny_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_tiny_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_short_light,
            FnInStruct
        )]
        pub fn for_language_identifier_short_light(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name =
                icu_locale::names::LanguageIdentifierDisplayName::try_new_short_light(
                    (&locale.0).into(),
                    langid,
                    options,
                )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_short_light,
            FnInStruct
        )]
        pub fn for_language_identifier_short_light_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_short_light_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_long_light,
            FnInStruct
        )]
        pub fn for_language_identifier_long_light(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name =
                icu_locale::names::LanguageIdentifierDisplayName::try_new_long_light(
                    (&locale.0).into(),
                    langid,
                    options,
                )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_long_light,
            FnInStruct
        )]
        pub fn for_language_identifier_long_light_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_long_light_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_menu_light,
            FnInStruct
        )]
        pub fn for_language_identifier_menu_light(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name =
                icu_locale::names::LanguageIdentifierDisplayName::try_new_menu_light(
                    (&locale.0).into(),
                    langid,
                    options,
                )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_menu_light,
            FnInStruct
        )]
        pub fn for_language_identifier_menu_light_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_menu_light_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_short_menu_light,
            FnInStruct
        )]
        pub fn for_language_identifier_short_menu_light(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name =
                icu_locale::names::LanguageIdentifierDisplayName::try_new_short_menu_light(
                    (&locale.0).into(),
                    langid,
                    options,
                )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_short_menu_light,
            FnInStruct
        )]
        pub fn for_language_identifier_short_menu_light_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_short_menu_light_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_heavy,
            FnInStruct
        )]
        pub fn for_language_identifier_heavy(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_heavy(
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_heavy,
            FnInStruct
        )]
        pub fn for_language_identifier_heavy_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_heavy_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_short_heavy,
            FnInStruct
        )]
        pub fn for_language_identifier_short_heavy(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name =
                icu_locale::names::LanguageIdentifierDisplayName::try_new_short_heavy(
                    (&locale.0).into(),
                    langid,
                    options,
                )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_short_heavy,
            FnInStruct
        )]
        pub fn for_language_identifier_short_heavy_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_short_heavy_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_long_heavy,
            FnInStruct
        )]
        pub fn for_language_identifier_long_heavy(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name =
                icu_locale::names::LanguageIdentifierDisplayName::try_new_long_heavy(
                    (&locale.0).into(),
                    langid,
                    options,
                )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_long_heavy,
            FnInStruct
        )]
        pub fn for_language_identifier_long_heavy_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_long_heavy_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_menu_heavy,
            FnInStruct
        )]
        pub fn for_language_identifier_menu_heavy(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name =
                icu_locale::names::LanguageIdentifierDisplayName::try_new_menu_heavy(
                    (&locale.0).into(),
                    langid,
                    options,
                )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_menu_heavy,
            FnInStruct
        )]
        pub fn for_language_identifier_menu_heavy_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_menu_heavy_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_short_menu_heavy,
            FnInStruct
        )]
        pub fn for_language_identifier_short_menu_heavy(
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name =
                icu_locale::names::LanguageIdentifierDisplayName::try_new_short_menu_heavy(
                    (&locale.0).into(),
                    langid,
                    options,
                )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }

        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(demo_gen, disable)]
        #[diplomat::rust_link(
            icu::locale::names::LanguageIdentifierDisplayName::try_new_short_menu_heavy,
            FnInStruct
        )]
        pub fn for_language_identifier_short_menu_heavy_with_provider(
            provider: &DataProvider,
            locale: &Locale,
            langid: &Locale,
            language_display: LanguageDisplayUnstable,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Result<(), DataError> {
            let langid = langid.0.id.clone();
            let mut options = icu_locale::names::LanguageIdentifierDisplayNameOptions::default();
            options.language_display = Some(language_display.into());
            let display_name = icu_locale::names::LanguageIdentifierDisplayName::try_new_short_menu_heavy_with_buffer_provider(
                provider.get()?,
                (&locale.0).into(),
                langid,
                options,
            )?;
            let _infallible = display_name.as_borrowed().write_to(write);
            Ok(())
        }
    }
}
