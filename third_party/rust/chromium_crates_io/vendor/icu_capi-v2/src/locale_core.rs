// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::unstable::errors::ffi::LocaleParseError;

    use writeable::Writeable;

    #[diplomat::opaque_mut]
    /// An ICU4X Locale, capable of representing strings like `"en-US"`.
    #[diplomat::rust_link(icu::locale::Locale, Struct)]
    #[diplomat::rust_link(icu::locale::DataLocale, Struct, hidden)]
    #[diplomat::rust_link(icu::locale::DataLocale::into_locale, FnInStruct, hidden)]
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
        #[diplomat::rust_link(icu::locale::DataLocale::from_str, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::locale::DataLocale::try_from_str, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::locale::DataLocale::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn from_string(name: &DiplomatStr) -> Result<Box<Locale>, LocaleParseError> {
            Ok(Box::new(Locale(icu_locale_core::Locale::try_from_utf8(
                name,
            )?)))
        }

        /// Construct a unknown [`Locale`] "und".
        #[diplomat::rust_link(icu::locale::Locale::UNKNOWN, AssociatedConstantInStruct)]
        #[diplomat::rust_link(icu::locale::DataLocale::default, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::locale::DataLocale::is_unknown, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn unknown() -> Box<Locale> {
            Box::new(Locale(icu_locale_core::Locale::UNKNOWN))
        }

        /// Returns a borrowed unknown ("und") [`Locale`], without allocating.
        //
        // (Can potentially be exposed to other backends if they gain static support)
        #[diplomat::attr(not(cpp), disable)]
        #[diplomat::rust_link(icu::locale::Locale::UNKNOWN, AssociatedConstantInStruct)]
        pub fn unknown_ref() -> &'static Locale {
            static UND: Locale = Locale(icu_locale_core::Locale::UNKNOWN);
            &UND
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

        /// Set a Unicode extension.
        #[diplomat::rust_link(icu::locale::Locale::extensions, StructField)]
        pub fn set_unicode_extension(&mut self, k: &DiplomatStr, v: &DiplomatStr) -> Option<()> {
            let k = icu_locale_core::extensions::unicode::Key::try_from_utf8(k).ok()?;
            let v = icu_locale_core::extensions::unicode::Value::try_from_utf8(v).ok()?;
            self.0.extensions.unicode.keywords.set(k, v);
            Some(())
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
                icu_locale_core::subtags::Language::UNKNOWN
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

        // --- Variants ---

        /// Returns a string representation of the [`Locale`] variants.
        #[diplomat::rust_link(icu::locale::Variants, Struct)]
        pub fn variants(&self, write: &mut diplomat_runtime::DiplomatWrite) {
            let _infallible = self.0.id.variants.write_to(write);
        }

        /// Returns the number of variants in this [`Locale`].
        #[diplomat::rust_link(icu::locale::Variants, Struct)]
        #[diplomat::attr(auto, getter)]
        pub fn variant_count(&self) -> usize {
            self.0.id.variants.len()
        }

        /// Returns the variant at the given index, or nothing if the index is out of bounds.
        #[diplomat::rust_link(icu::locale::Variants, Struct)]
        pub fn variant_at(
            &self,
            index: usize,
            write: &mut diplomat_runtime::DiplomatWrite,
        ) -> Option<()> {
            let _infallible = self.0.id.variants.get(index)?.write_to(write);
            Some(())
        }

        /// Returns whether the [`Locale`] has a specific variant.
        #[diplomat::rust_link(icu::locale::Variants, Struct)]
        pub fn has_variant(&self, s: &DiplomatStr) -> bool {
            icu_locale_core::subtags::Variant::try_from_utf8(s)
                .map(|v| self.0.id.variants.contains(&v))
                .unwrap_or(false)
        }

        /// Adds a variant to the [`Locale`].
        ///
        /// Returns an error if the variant string is invalid.
        /// Returns `true` if the variant was added, `false` if already present.
        #[diplomat::rust_link(icu::locale::Variants::push, FnInStruct)]
        pub fn add_variant(&mut self, s: &DiplomatStr) -> Result<bool, LocaleParseError> {
            let variant = icu_locale_core::subtags::Variant::try_from_utf8(s)?;
            Ok(self.0.id.variants.push(variant))
        }

        /// Removes a variant from the [`Locale`].
        ///
        /// Returns `true` if the variant was removed, `false` if not present.
        /// Returns `false` for invalid variant strings (they cannot exist in the locale).
        #[diplomat::rust_link(icu::locale::Variants::remove, FnInStruct)]
        pub fn remove_variant(&mut self, s: &DiplomatStr) -> bool {
            icu_locale_core::subtags::Variant::try_from_utf8(s)
                .map(|v| self.0.id.variants.remove(&v))
                .unwrap_or(false)
        }

        /// Clears all variants from the [`Locale`].
        #[diplomat::rust_link(icu::locale::Variants::clear, FnInStruct)]
        pub fn clear_variants(&mut self) {
            self.0.id.variants.clear();
        }

        // --- Other ---

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
        #[diplomat::rust_link(icu::locale::DataLocale::to_string, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::locale::DataLocale::write_to, FnInStruct, hidden)]
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
        #[diplomat::rust_link(icu::locale::DataLocale::strict_cmp, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::locale::DataLocale::total_cmp, FnInStruct, hidden)]
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
