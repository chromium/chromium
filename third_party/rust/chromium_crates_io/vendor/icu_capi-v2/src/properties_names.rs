// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use crate::properties_enums::ffi::GeneralCategoryGroup;
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::{errors::ffi::DataError, provider::ffi::DataProvider};

    /// A type capable of looking up a property value from a string name.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::properties::PropertyParser, Struct)]
    #[diplomat::rust_link(icu::properties::PropertyParserBorrowed, Struct)]
    #[diplomat::rust_link(icu::properties::PropertyParser::new, FnInStruct)]
    #[diplomat::rust_link(icu::properties::PropertyParserBorrowed::new, FnInStruct, hidden)]
    #[diplomat::rust_link(
        icu::properties::props::NamedEnumeratedProperty::try_from_str,
        FnInTrait,
        hidden
    )]
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

        /// Create a name-to-enum mapper for the `General_Category` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::GeneralCategory, Enum)]
        #[diplomat::attr(auto, named_constructor = "general_category")]
        #[cfg(feature = "compiled_data")]
        pub fn create_general_category() -> Box<PropertyValueNameToEnumMapper> {
            Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<icu_properties::props::GeneralCategory>::new()
                    .static_to_owned()
                    .erase(),
            ))
        }

        /// Create a name-to-enum mapper for the `General_Category` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::GeneralCategory, Enum)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "general_category_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_general_category_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<
                                    icu_properties::props::GeneralCategory,
                                >::try_new_unstable(&provider.get_unstable()?)?
                    .erase(),
            )))
        }
        /// Create a name-to-enum mapper for the `Hangul_Syllable_Type` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::HangulSyllableType, Struct)]
        #[diplomat::attr(auto, named_constructor = "hangul_syllable_type")]
        #[cfg(feature = "compiled_data")]
        pub fn create_hangul_syllable_type() -> Box<PropertyValueNameToEnumMapper> {
            Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<icu_properties::props::HangulSyllableType>::new()
                    .static_to_owned()
                    .erase(),
            ))
        }
        /// Create a name-to-enum mapper for the `Hangul_Syllable_Type` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::HangulSyllableType, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "hangul_syllable_type_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_hangul_syllable_type_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                        icu_properties::PropertyParser::<
                                icu_properties::props::HangulSyllableType,
                            >::try_new_unstable(&provider.get_unstable()?)?
                    .erase(),
            )))
        }
        /// Create a name-to-enum mapper for the `East_Asian_Width` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::EastAsianWidth, Struct)]
        #[diplomat::attr(auto, named_constructor = "east_asian_width")]
        #[cfg(feature = "compiled_data")]
        pub fn create_east_asian_width() -> Box<PropertyValueNameToEnumMapper> {
            Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<icu_properties::props::EastAsianWidth>::new()
                    .static_to_owned()
                    .erase(),
            ))
        }
        /// Create a name-to-enum mapper for the `East_Asian_Width` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::EastAsianWidth, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "east_asian_width_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_east_asian_width_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<
                                    icu_properties::props::EastAsianWidth,
                                >::try_new_unstable(&provider.get_unstable()?
                    )?
                    .erase(),
            )))
        }
        /// Create a name-to-enum mapper for the `Bidi_Class` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::BidiClass, Struct)]
        #[diplomat::attr(auto, named_constructor = "bidi_class")]
        #[cfg(feature = "compiled_data")]
        pub fn create_bidi_class() -> Box<PropertyValueNameToEnumMapper> {
            Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<icu_properties::props::BidiClass>::new()
                    .static_to_owned()
                    .erase(),
            ))
        }
        /// Create a name-to-enum mapper for the `Bidi_Class` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::BidiClass, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "bidi_class_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_bidi_class_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                    icu_properties::PropertyParser::<icu_properties::props::BidiClass>::try_new_unstable(&provider.get_unstable()?)?
                .erase(),
            )))
        }
        /// Create a name-to-enum mapper for the `Indic_Syllabic_Category` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::IndicSyllabicCategory, Struct)]
        #[diplomat::attr(auto, named_constructor = "indic_syllabic_category")]
        #[cfg(feature = "compiled_data")]
        pub fn create_indic_syllabic_category() -> Box<PropertyValueNameToEnumMapper> {
            Box::new(PropertyValueNameToEnumMapper(icu_properties::PropertyParser::<icu_properties::props::IndicSyllabicCategory>::new().static_to_owned().erase()))
        }
        /// Create a name-to-enum mapper for the `Indic_Syllabic_Category` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::IndicSyllabicCategory, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "indic_syllabic_category_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_indic_syllabic_category_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(
                Box::new(
                    PropertyValueNameToEnumMapper(
                        icu_properties::PropertyParser::<
                            icu_properties::props::IndicSyllabicCategory,
                        >::try_new_unstable(&provider.get_unstable()?)?
                        .erase(),
                    ),
                ),
            )
        }
        /// Create a name-to-enum mapper for the `Line_Break` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::LineBreak, Struct)]
        #[diplomat::attr(auto, named_constructor = "line_break")]
        #[cfg(feature = "compiled_data")]
        pub fn create_line_break() -> Box<PropertyValueNameToEnumMapper> {
            Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<icu_properties::props::LineBreak>::new()
                    .static_to_owned()
                    .erase(),
            ))
        }
        /// Create a name-to-enum mapper for the `Line_Break` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::LineBreak, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "line_break_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_line_break_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                    icu_properties::PropertyParser::<icu_properties::props::LineBreak>::try_new_unstable(&provider.get_unstable()?
                )?
                .erase(),
            )))
        }
        /// Create a name-to-enum mapper for the `Grapheme_Cluster_Break` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::GraphemeClusterBreak, Struct)]
        #[diplomat::attr(auto, named_constructor = "grapheme_cluster_break")]
        #[cfg(feature = "compiled_data")]
        pub fn create_grapheme_cluster_break() -> Box<PropertyValueNameToEnumMapper> {
            Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<icu_properties::props::GraphemeClusterBreak>::new(
                )
                .static_to_owned()
                .erase(),
            ))
        }
        /// Create a name-to-enum mapper for the `Grapheme_Cluster_Break` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::GraphemeClusterBreak, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "grapheme_cluster_break_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_grapheme_cluster_break_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(
                Box::new(
                    PropertyValueNameToEnumMapper(
                        icu_properties::PropertyParser::<
                            icu_properties::props::GraphemeClusterBreak,
                        >::try_new_unstable(&provider.get_unstable()?)?
                        .erase(),
                    ),
                ),
            )
        }
        /// Create a name-to-enum mapper for the `Word_Break` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::WordBreak, Struct)]
        #[diplomat::attr(auto, named_constructor = "word_break")]
        #[cfg(feature = "compiled_data")]
        pub fn create_word_break() -> Box<PropertyValueNameToEnumMapper> {
            Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<icu_properties::props::WordBreak>::new()
                    .static_to_owned()
                    .erase(),
            ))
        }
        /// Create a name-to-enum mapper for the `Word_Break` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::WordBreak, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "word_break_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_word_break_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                    icu_properties::PropertyParser::<icu_properties::props::WordBreak>::try_new_unstable(&provider.get_unstable()?)?
                .erase(),
            )))
        }
        /// Create a name-to-enum mapper for the `Sentence_Break` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::SentenceBreak, Struct)]
        #[diplomat::attr(auto, named_constructor = "sentence_break")]
        #[cfg(feature = "compiled_data")]
        pub fn create_sentence_break() -> Box<PropertyValueNameToEnumMapper> {
            Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<icu_properties::props::SentenceBreak>::new()
                    .static_to_owned()
                    .erase(),
            ))
        }
        /// Create a name-to-enum mapper for the `Sentence_Break` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::SentenceBreak, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "sentence_break_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_sentence_break_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<
                                        icu_properties::props::SentenceBreak,
                                    >::try_new_unstable(&provider.get_unstable()?
                    )?
                    .erase(),
            )))
        }
        /// Create a name-to-enum mapper for the `Script` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::Script, Struct)]
        #[diplomat::attr(auto, named_constructor = "script")]
        #[cfg(feature = "compiled_data")]
        pub fn create_script() -> Box<PropertyValueNameToEnumMapper> {
            Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<icu_properties::props::Script>::new()
                    .static_to_owned()
                    .erase(),
            ))
        }
        /// Create a name-to-enum mapper for the `Script` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::Script, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "script_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_script_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<PropertyValueNameToEnumMapper>, DataError> {
            Ok(Box::new(PropertyValueNameToEnumMapper(
                icu_properties::PropertyParser::<icu_properties::props::Script>::try_new_unstable(
                    &provider.get_unstable()?,
                )?
                .erase(),
            )))
        }
    }

    /// A type capable of looking up General Category Group values from a string name.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::properties::PropertyParser, Struct)]
    #[diplomat::rust_link(icu::properties::props::GeneralCategory, Struct)]
    pub struct GeneralCategoryNameToGroupMapper(
        icu_properties::PropertyParser<icu_properties::props::GeneralCategoryGroup>,
    );

    impl GeneralCategoryNameToGroupMapper {
        /// Get the mask value matching the given name, using strict matching
        ///
        /// Returns 0 if the name is unknown for this property
        #[diplomat::rust_link(icu::properties::PropertyParserBorrowed::get_strict, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::PropertyParserBorrowed::get_strict_u16,
            FnInStruct,
            hidden
        )]
        pub fn get_strict(&self, name: &DiplomatStr) -> GeneralCategoryGroup {
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
        pub fn get_loose(&self, name: &DiplomatStr) -> GeneralCategoryGroup {
            if let Ok(name) = core::str::from_utf8(name) {
                self.0.as_borrowed().get_loose(name)
            } else {
                None
            }
            .map(Into::into)
            .unwrap_or_default()
        }
        /// Create a name-to-mask mapper for the `General_Category` property, using compiled data.
        #[diplomat::rust_link(icu_properties::props::GeneralCategoryGroup, Struct)]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<GeneralCategoryNameToGroupMapper> {
            Box::new(GeneralCategoryNameToGroupMapper(
                icu_properties::PropertyParser::<icu_properties::props::GeneralCategoryGroup>::new(
                )
                .static_to_owned(),
            ))
        }
        /// Create a name-to-mask mapper for the `General_Category` property, using a particular data source.
        #[diplomat::rust_link(icu_properties::props::GeneralCategoryGroup, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<GeneralCategoryNameToGroupMapper>, DataError> {
            Ok(Box::new(
                GeneralCategoryNameToGroupMapper(icu_properties::PropertyParser::<
                    icu_properties::props::GeneralCategoryGroup,
                >::try_new_unstable(
                    &provider.get_unstable()?
                )?),
            ))
        }
    }
}
