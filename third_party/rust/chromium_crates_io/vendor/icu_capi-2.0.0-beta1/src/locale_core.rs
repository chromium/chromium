// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::errors::ffi::LocaleParseError;

    use writeable::Writeable;

    #[diplomat::opaque]
    /// An ICU4X Locale, capable of representing strings like `"en-US"`.
    #[diplomat::rust_link(icu::locale::Locale, Struct)]
    pub struct Locale(pub icu_locale_core::Locale);

    impl Locale {
        /// Construct an [`Locale`] from an locale identifier.
        ///
        /// This will run the complete locale parsing algorithm. If code size and
        /// performance are critical and the locale is of a known shape (such as
        /// `aa-BB`) use `create_und`, `set_language`, `set_script`, and `set_region`.
        #[diplomat::rust_link(icu::locale::Locale::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::locale::Locale::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::locale::Locale::from_str, FnInStruct, hidden)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn from_string(name: &DiplomatStr) -> Result<Box<Locale>, LocaleParseError> {
            Ok(Box::new(Locale(icu_locale_core::Locale::try_from_utf8(
                name,
            )?)))
        }

        /// Construct a default undefined [`Locale`] "und".
        #[diplomat::rust_link(icu::locale::Locale::default, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn und() -> Box<Locale> {
            Box::new(Locale(icu_locale_core::Locale::default()))
        }

        /// Clones the [`Locale`].
        #[diplomat::rust_link(icu::locale::Locale, Struct)]
        pub fn clone(&self) -> Box<Locale> {
            Box::new(Locale(self.0.clone()))
        }

        /// Returns a string representation of the `LanguageIdentifier` part of
        /// [`Locale`].
        #[diplomat::rust_link(icu::locale::Locale::id, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn basename(&self, write: &mut diplomat_runtime::DiplomatWrite) {
            let _infallible = self.0.id.write_to(write);
        }

        /// Returns a string representation of the unicode extension.
        #[diplomat::rust_link(icu::locale::Locale::extensions, StructField)]
        pub fn get_unicode_extension(
            &self,
            s: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Option<()> {
            icu_locale_core::extensions::unicode::Key::try_from_utf8(s)
                .ok()
                .and_then(|k| self.0.extensions.unicode.keywords.get(&k))
                .map(|v| {
                    let _infallible = v.write_to(write);
                })
        }

        /// Returns a string representation of [`Locale`] language.
        #[diplomat::rust_link(icu::locale::Locale::id, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn language(&self, write: &mut diplomat_runtime::DiplomatWrite) {
            let _infallible = self.0.id.language.write_to(write);
        }

        /// Set the language part of the [`Locale`].
        #[diplomat::rust_link(icu::locale::Locale::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::locale::Locale::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::attr(auto, setter = "language")]
        pub fn set_language(&mut self, s: &DiplomatStr) -> Result<(), LocaleParseError> {
            self.0.id.language = if s.is_empty() {
                icu_locale_core::subtags::Language::UND
            } else {
                icu_locale_core::subtags::Language::try_from_utf8(s)?
            };
            Ok(())
        }

        /// Returns a string representation of [`Locale`] region.
        #[diplomat::rust_link(icu::locale::Locale::id, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn region(&self, write: &mut diplomat_runtime::DiplomatWrite) -> Option<()> {
            self.0.id.region.map(|region| {
                let _infallible = region.write_to(write);
            })
        }

        /// Set the region part of the [`Locale`].
        #[diplomat::rust_link(icu::locale::Locale::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::locale::Locale::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = accessors, not(dart)), setter = "region")]
        pub fn set_region(&mut self, s: &DiplomatStr) -> Result<(), LocaleParseError> {
            self.0.id.region = if s.is_empty() {
                None
            } else {
                Some(icu_locale_core::subtags::Region::try_from_utf8(s)?)
            };
            Ok(())
        }

        /// Returns a string representation of [`Locale`] script.
        #[diplomat::rust_link(icu::locale::Locale::id, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn script(&self, write: &mut diplomat_runtime::DiplomatWrite) -> Option<()> {
            self.0.id.script.map(|script| {
                let _infallible = script.write_to(write);
            })
        }

        /// Set the script part of the [`Locale`]. Pass an empty string to remove the script.
        #[diplomat::rust_link(icu::locale::Locale::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::locale::Locale::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = accessors, not(dart)), setter = "script")]
        pub fn set_script(&mut self, s: &DiplomatStr) -> Result<(), LocaleParseError> {
            self.0.id.script = if s.is_empty() {
                None
            } else {
                Some(icu_locale_core::subtags::Script::try_from_utf8(s)?)
            };
            Ok(())
        }

        /// Normalizes a locale string.
        #[diplomat::rust_link(icu::locale::Locale::normalize, FnInStruct)]
        #[diplomat::rust_link(icu::locale::Locale::normalize_utf8, FnInStruct, hidden)]
        pub fn normalize(
            s: &DiplomatStr,
            write: &mut DiplomatWrite,
        ) -> Result<(), LocaleParseError> {
            let _infallible = icu_locale_core::Locale::normalize_utf8(s)?.write_to(write);
            Ok(())
        }
        /// Returns a string representation of [`Locale`].
        #[diplomat::rust_link(icu::locale::Locale::write_to, FnInStruct)]
        #[diplomat::rust_link(icu::locale::Locale::to_string, FnInStruct, hidden)]
        #[diplomat::attr(auto, stringifier)]
        pub fn to_string(&self, write: &mut diplomat_runtime::DiplomatWrite) {
            let _infallible = self.0.write_to(write);
        }

        #[diplomat::rust_link(icu::locale::Locale::normalizing_eq, FnInStruct)]
        pub fn normalizing_eq(&self, other: &DiplomatStr) -> bool {
            if let Ok(other) = core::str::from_utf8(other) {
                self.0.normalizing_eq(other)
            } else {
                // invalid UTF8 won't be allowed in locales anyway
                false
            }
        }

        #[diplomat::rust_link(icu::locale::Locale::strict_cmp, FnInStruct)]
        pub fn compare_to_string(&self, other: &DiplomatStr) -> core::cmp::Ordering {
            self.0.strict_cmp(other)
        }

        #[diplomat::rust_link(icu::locale::Locale::total_cmp, FnInStruct)]
        #[diplomat::attr(auto, comparison)]
        pub fn compare_to(&self, other: &Self) -> core::cmp::Ordering {
            self.0.total_cmp(&other.0)
        }
    }
}

impl ffi::Locale {
    pub fn to_datalocale(&self) -> icu_provider::DataLocale {
        (&self.0).into()
    }
}
