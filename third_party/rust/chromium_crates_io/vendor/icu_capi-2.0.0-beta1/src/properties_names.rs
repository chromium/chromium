// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use crate::errors::ffi::DataError;
    use crate::provider::ffi::DataProvider;

    /// A type capable of looking up a property value from a string name.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::properties::PropertyParser, Struct)]
    #[diplomat::rust_link(icu::properties::PropertyParserBorrowed, Struct)]
    #[diplomat::rust_link(icu::properties::PropertyParser::new, FnInStruct)]
    #[diplomat::rust_link(icu::properties::PropertyParserBorrowed::new, FnInStruct, hidden)]
    pub struct PropertyValueNameToEnumMapper(icu_properties::PropertyParser<u16>);

    impl PropertyValueNameToEnumMapper {
        /// Get the property value matching the given name, using strict matching
        ///
        /// Returns -1 if the name is unknown for this property
        #[diplomat::rust_link(icu::properties::PropertyParserBorrowed::get_strict, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::PropertyParserBorrowed::get_strict_u16,
            FnInStruct,
            hidden
        )]
        pub fn get_strict(&self, name: &DiplomatStr) -> i16 {
            if let Ok(name) = core::str::from_utf8(name) {
                self.0.as_borrowed().get_strict(name)
            } else {
                None
            }
            .map(|u_16| u_16 as i16)
            .unwrap_or(-1)
        }

        /// Get the property value matching the given name, using loose matching
        ///
        /// Returns -1 if the name is unknown for this property
        #[diplomat::rust_link(icu::properties::PropertyParserBorrowed::get_loose, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::PropertyParserBorrowed::get_loose_u16,
            FnInStruct,
            hidden
        )]
        pub fn get_loose(&self, name: &DiplomatStr) -> i16 {
            if let Ok(name) = core::str::from_utf8(name) {
                self.0.as_borrowed().get_loose(name)
            } else {
                None
            }
            .map(|u_16| u_16 as i16)
            .unwrap_or(-1)
        }

        #[diplomat::rust_link(icu_properties::props::GeneralCategory, Enum)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "general_category")]
        pub fn load_general_category(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::GeneralCategory>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::GeneralCategory>::try_new_unstable,
                    provider,
                )?
                .erase(),
            )))
        }

        #[diplomat::rust_link(icu_properties::props::HangulSyllableType, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "hangul_syllable_type")]
        pub fn load_hangul_syllable_type(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::HangulSyllableType>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::HangulSyllableType>::try_new_unstable,
                    provider,
                )?
                .erase(),
            )))
        }

        #[diplomat::rust_link(icu_properties::props::EastAsianWidth, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "east_asian_width")]
        pub fn load_east_asian_width(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::EastAsianWidth>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::EastAsianWidth>::try_new_unstable,
                    provider,
                )?
                .erase(),
            )))
        }

        #[diplomat::rust_link(icu_properties::props::BidiClass, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "bidi_class")]
        pub fn load_bidi_class(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::BidiClass>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::BidiClass>::try_new_unstable,
                    provider,
                )?
                .erase(),
            )))
        }

        #[diplomat::rust_link(icu_properties::props::IndicSyllabicCategory, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "indic_syllabic_category")]
        pub fn load_indic_syllabic_category(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::IndicSyllabicCategory>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::IndicSyllabicCategory>::try_new_unstable,
                    provider,
                )?
                .erase(),
            )))
        }

        #[diplomat::rust_link(icu_properties::props::LineBreak, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "line_break")]
        pub fn load_line_break(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::LineBreak>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::LineBreak>::try_new_unstable,
                    provider,
                )?
                .erase(),
            )))
        }

        #[diplomat::rust_link(icu_properties::props::GraphemeClusterBreak, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "grapheme_cluster_break")]
        pub fn load_grapheme_cluster_break(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::GraphemeClusterBreak>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::GraphemeClusterBreak>::try_new_unstable,
                    provider,
                )?
                .erase(),
            )))
        }

        #[diplomat::rust_link(icu_properties::props::WordBreak, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "word_break")]
        pub fn load_word_break(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::WordBreak>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::WordBreak>::try_new_unstable,
                    provider,
                )?
                .erase(),
            )))
        }

        #[diplomat::rust_link(icu_properties::props::SentenceBreak, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "sentence_break")]
        pub fn load_sentence_break(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::SentenceBreak>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::SentenceBreak>::try_new_unstable,
                    provider,
                )?
                .erase(),
            )))
        }

        #[diplomat::rust_link(icu_properties::props::Script, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "script")]
        pub fn load_script(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::Script>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::Script>::try_new_unstable,
                    provider,
                )?
                .erase(),
            )))
        }
    }

    /// A type capable of looking up General Category mask values from a string name.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::properties::PropertyParser, Struct)]
    #[diplomat::rust_link(icu::properties::props::GeneralCategory, Struct)]
    pub struct GeneralCategoryNameToMaskMapper(
        icu_properties::PropertyParser<icu_properties::props::GeneralCategoryGroup>,
    );

    impl GeneralCategoryNameToMaskMapper {
        /// Get the mask value matching the given name, using strict matching
        ///
        /// Returns 0 if the name is unknown for this property
        #[diplomat::rust_link(icu::properties::PropertyParserBorrowed::get_strict, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::PropertyParserBorrowed::get_strict_u16,
            FnInStruct,
            hidden
        )]
        pub fn get_strict(&self, name: &DiplomatStr) -> u32 {
            if let Ok(name) = core::str::from_utf8(name) {
                self.0.as_borrowed().get_strict(name)
            } else {
                None
            }
            .map(Into::into)
            .unwrap_or_default()
        }

        /// Get the mask value matching the given name, using loose matching
        ///
        /// Returns 0 if the name is unknown for this property
        #[diplomat::rust_link(icu::properties::PropertyParserBorrowed::get_loose, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::PropertyParserBorrowed::get_loose_u16,
            FnInStruct,
            hidden
        )]
        pub fn get_loose(&self, name: &DiplomatStr) -> u32 {
            if let Ok(name) = core::str::from_utf8(name) {
                self.0.as_borrowed().get_loose(name)
            } else {
                None
            }
            .map(Into::into)
            .unwrap_or_default()
        }

        #[diplomat::rust_link(icu_properties::props::GeneralCategoryGroup, Struct)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        pub fn load(
            provider: &DataProvider,
        ) -> Result<Box<GeneralCategoryNameToMaskMapper>, DataError> {
            Ok(Box::new(GeneralCategoryNameToMaskMapper(
                call_constructor_unstable!(
                    icu_properties::PropertyParser::<icu_properties::props::GeneralCategoryGroup>::new [r => Ok(r.static_to_owned())],
                    icu_properties::PropertyParser::<icu_properties::props::GeneralCategoryGroup>::try_new_unstable,
                    provider,
                )?,
            )))
        }
    }
}
