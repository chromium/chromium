// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use icu_properties::props::{
        BidiClass, CanonicalCombiningClass, EastAsianWidth, GeneralCategory, GraphemeClusterBreak,
        HangulSyllableType, IndicSyllabicCategory, JoiningType, LineBreak, Script, SentenceBreak,
        WordBreak,
    };

    use crate::properties_enums::ffi::GeneralCategoryGroup;
    use crate::properties_iter::ffi::CodePointRangeIterator;
    use crate::properties_sets::ffi::CodePointSetData;
    #[cfg(feature = "buffer_provider")]
    use crate::{errors::ffi::DataError, provider::ffi::DataProvider};

    #[diplomat::opaque]
    /// An ICU4X Unicode Map Property object, capable of querying whether a code point (key) to obtain the Unicode property value, for a specific Unicode property.
    ///
    /// For properties whose values fit into 8 bits.
    #[diplomat::rust_link(icu::properties, Mod)]
    #[diplomat::rust_link(icu::properties::CodePointMapData, Struct)]
    #[diplomat::rust_link(icu::properties::CodePointMapDataBorrowed, Struct)]
    #[diplomat::rust_link(icu::properties::CodePointMapData::new, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::properties::CodePointMapDataBorrowed::new, FnInStruct, hidden)]
    #[diplomat::rust_link(
        icu::properties::CodePointMapData::try_into_converted,
        FnInStruct,
        hidden
    )]
    pub struct CodePointMapData8(icu_properties::CodePointMapData<u8>);

    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    fn convert_8<P: icu_collections::codepointtrie::TrieValue>(
        data: icu_properties::CodePointMapData<P>,
    ) -> Box<CodePointMapData8> {
        #[allow(clippy::unwrap_used)] // infallible for the chosen properties
        Box::new(CodePointMapData8(
            data.try_into_converted().map_err(|_| ()).unwrap(),
        ))
    }

    impl CodePointMapData8 {
        /// Gets the value for a code point.
        #[diplomat::rust_link(icu::properties::CodePointMapDataBorrowed::get, FnInStruct)]
        #[diplomat::rust_link(icu::properties::CodePointMapDataBorrowed::get32, FnInStruct, hidden)]
        #[diplomat::attr(auto, indexer)]
        pub fn get(&self, cp: DiplomatChar) -> u8 {
            self.0.as_borrowed().get32(cp)
        }

        /// Produces an iterator over ranges of code points that map to `value`
        #[diplomat::rust_link(
            icu::properties::CodePointMapDataBorrowed::iter_ranges_for_value,
            FnInStruct
        )]
        pub fn iter_ranges_for_value<'a>(&'a self, value: u8) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0.as_borrowed().iter_ranges_for_value(value),
            )))
        }

        /// Produces an iterator over ranges of code points that do not map to `value`
        #[diplomat::rust_link(
            icu::properties::CodePointMapDataBorrowed::iter_ranges_for_value_complemented,
            FnInStruct
        )]
        pub fn iter_ranges_for_value_complemented<'a>(
            &'a self,
            value: u8,
        ) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0
                    .as_borrowed()
                    .iter_ranges_for_value_complemented(value),
            )))
        }

        /// Given a mask value (the nth bit marks property value = n), produce an iterator over ranges of code points
        /// whose property values are contained in the mask.
        ///
        /// The main mask property supported is that for General_Category, which can be obtained via `general_category_to_mask()` or
        /// by using `GeneralCategoryNameToMaskMapper`
        ///
        /// Should only be used on maps for properties with values less than 32 (like Generak_Category),
        /// other maps will have unpredictable results
        #[diplomat::rust_link(
            icu::properties::CodePointMapDataBorrowed::iter_ranges_for_group,
            FnInStruct
        )]
        pub fn iter_ranges_for_group<'a>(
            &'a self,
            group: GeneralCategoryGroup,
        ) -> Box<CodePointRangeIterator<'a>> {
            let ranges = self
                .0
                .as_borrowed()
                .iter_ranges_mapped(move |v| {
                    let val_mask = 1_u32.checked_shl(v.into()).unwrap_or(0);
                    val_mask & group.mask != 0
                })
                .filter(|v| v.value)
                .map(|v| v.range);
            Box::new(CodePointRangeIterator(Box::new(ranges)))
        }

        /// Gets a [`CodePointSetData`] representing all entries in this map that map to the given value
        #[diplomat::rust_link(
            icu::properties::CodePointMapDataBorrowed::get_set_for_value,
            FnInStruct
        )]
        pub fn get_set_for_value(&self, value: u8) -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                self.0.as_borrowed().get_set_for_value(value),
            ))
        }

        /// Create a map for the `General_Category` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::GeneralCategory, Enum)]
        #[diplomat::attr(auto, named_constructor = "general_category")]
        #[cfg(feature = "compiled_data")]
        pub fn create_general_category() -> Box<CodePointMapData8> {
            convert_8(icu_properties::CodePointMapData::<GeneralCategory>::new().static_to_owned())
        }

        /// Create a map for the `General_Category` property, using a particular data source
        #[diplomat::rust_link(icu::properties::props::GeneralCategory, Enum)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "general_category_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_general_category_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(icu_properties::CodePointMapData::<
                GeneralCategory,
            >::try_new_unstable(
                &provider.get_unstable()?
            )?))
        }

        /// Create a map for the `Bidi_Class` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::BidiClass, Struct)]
        #[diplomat::attr(auto, named_constructor = "bidi_class")]
        #[cfg(feature = "compiled_data")]
        pub fn create_bidi_class() -> Box<CodePointMapData8> {
            convert_8(icu_properties::CodePointMapData::<BidiClass>::new().static_to_owned())
        }

        /// Create a map for the `Bidi_Class` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::BidiClass, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "bidi_class_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_bidi_class_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(
                icu_properties::CodePointMapData::<BidiClass>::try_new_unstable(
                    &provider.get_unstable()?,
                )?,
            ))
        }
        /// Create a map for the `East_Asian_Width` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::EastAsianWidth, Struct)]
        #[diplomat::attr(auto, named_constructor = "east_asian_width")]
        #[cfg(feature = "compiled_data")]
        pub fn create_east_asian_width() -> Box<CodePointMapData8> {
            convert_8(icu_properties::CodePointMapData::<EastAsianWidth>::new().static_to_owned())
        }

        /// Create a map for the `East_Asian_Width` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::EastAsianWidth, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "east_asian_width_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_east_asian_width_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(
                icu_properties::CodePointMapData::<EastAsianWidth>::try_new_unstable(
                    &provider.get_unstable()?,
                )?,
            ))
        }
        /// Create a map for the `Hangul_Syllable_Type` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::HangulSyllableType, Struct)]
        #[diplomat::attr(auto, named_constructor = "hangul_syllable_type")]
        #[cfg(feature = "compiled_data")]
        pub fn create_hangul_syllable_type() -> Box<CodePointMapData8> {
            convert_8(
                icu_properties::CodePointMapData::<HangulSyllableType>::new().static_to_owned(),
            )
        }
        /// Create a map for the `Hangul_Syllable_Type` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::HangulSyllableType, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "hangul_syllable_type_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_hangul_syllable_type_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(icu_properties::CodePointMapData::<
                HangulSyllableType,
            >::try_new_unstable(
                &provider.get_unstable()?
            )?))
        }
        /// Create a map for the `Indic_Syllabic_Property` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory, Struct)]
        #[diplomat::attr(auto, named_constructor = "indic_syllabic_category")]
        #[cfg(feature = "compiled_data")]
        pub fn create_indic_syllabic_category() -> Box<CodePointMapData8> {
            convert_8(
                icu_properties::CodePointMapData::<IndicSyllabicCategory>::new().static_to_owned(),
            )
        }
        /// Create a map for the `Indic_Syllabic_Property` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "indic_syllabic_category_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_indic_syllabic_category_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(icu_properties::CodePointMapData::<
                IndicSyllabicCategory,
            >::try_new_unstable(
                &provider.get_unstable()?
            )?))
        }
        /// Create a map for the `Line_Break` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::LineBreak, Struct)]
        #[diplomat::attr(auto, named_constructor = "line_break")]
        #[cfg(feature = "compiled_data")]
        pub fn create_line_break() -> Box<CodePointMapData8> {
            convert_8(icu_properties::CodePointMapData::<LineBreak>::new().static_to_owned())
        }
        /// Create a map for the `Line_Break` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::LineBreak, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "line_break_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_line_break_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(
                icu_properties::CodePointMapData::<LineBreak>::try_new_unstable(
                    &provider.get_unstable()?,
                )?,
            ))
        }
        /// Create a map for the `Grapheme_Cluster_Break` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::GraphemeClusterBreak, Struct)]
        #[diplomat::attr(auto, named_constructor = "grapheme_cluster_break")]
        #[cfg(feature = "compiled_data")]
        pub fn create_grapheme_cluster_break() -> Box<CodePointMapData8> {
            convert_8(
                icu_properties::CodePointMapData::<GraphemeClusterBreak>::new().static_to_owned(),
            )
        }
        /// Create a map for the `Grapheme_Cluster_Break` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::GraphemeClusterBreak, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "grapheme_cluster_break_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_grapheme_cluster_break_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(icu_properties::CodePointMapData::<
                GraphemeClusterBreak,
            >::try_new_unstable(
                &provider.get_unstable()?
            )?))
        }
        /// Create a map for the `Word_Break` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::WordBreak, Struct)]
        #[diplomat::attr(auto, named_constructor = "word_break")]
        #[cfg(feature = "compiled_data")]
        pub fn create_word_break() -> Box<CodePointMapData8> {
            convert_8(icu_properties::CodePointMapData::<WordBreak>::new().static_to_owned())
        }
        /// Create a map for the `Word_Break` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::WordBreak, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "word_break_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_word_break_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(
                icu_properties::CodePointMapData::<WordBreak>::try_new_unstable(
                    &provider.get_unstable()?,
                )?,
            ))
        }
        /// Create a map for the `Sentence_Break` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::SentenceBreak, Struct)]
        #[diplomat::attr(auto, named_constructor = "sentence_break")]
        #[cfg(feature = "compiled_data")]
        pub fn create_sentence_break() -> Box<CodePointMapData8> {
            convert_8(icu_properties::CodePointMapData::<SentenceBreak>::new().static_to_owned())
        }
        /// Create a map for the `Sentence_Break` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::SentenceBreak, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "sentence_break_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_sentence_break_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(
                icu_properties::CodePointMapData::<SentenceBreak>::try_new_unstable(
                    &provider.get_unstable()?,
                )?,
            ))
        }
        /// Create a map for the `Joining_Type` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::JoiningType, Struct)]
        #[diplomat::attr(auto, named_constructor = "joining_type")]
        #[cfg(feature = "compiled_data")]
        pub fn create_joining_type() -> Box<CodePointMapData8> {
            convert_8(icu_properties::CodePointMapData::<JoiningType>::new().static_to_owned())
        }

        /// Create a map for the `Joining_Type` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::JoiningType, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "joining_type_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_joining_type_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(
                icu_properties::CodePointMapData::<JoiningType>::try_new_unstable(
                    &provider.get_unstable()?,
                )?,
            ))
        }
        /// Create a map for the `Canonical_Combining_Class` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass, Struct)]
        #[diplomat::attr(auto, named_constructor = "canonical_combining_class")]
        #[cfg(feature = "compiled_data")]
        pub fn create_canonical_combining_class() -> Box<CodePointMapData8> {
            convert_8(
                icu_properties::CodePointMapData::<CanonicalCombiningClass>::new()
                    .static_to_owned(),
            )
        }
        /// Create a map for the `Canonical_Combining_Class` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "canonical_combining_class_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_canonical_combining_class_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData8>, DataError> {
            Ok(convert_8(icu_properties::CodePointMapData::<
                CanonicalCombiningClass,
            >::try_new_unstable(
                &provider.get_unstable()?
            )?))
        }
    }

    #[diplomat::opaque]
    /// An ICU4X Unicode Map Property object, capable of querying whether a code point (key) to obtain the Unicode property value, for a specific Unicode property.
    ///
    /// For properties whose values fit into 16 bits.
    #[diplomat::rust_link(icu::properties, Mod)]
    #[diplomat::rust_link(icu::properties::CodePointMapData, Struct)]
    #[diplomat::rust_link(icu::properties::CodePointMapDataBorrowed, Struct)]
    pub struct CodePointMapData16(icu_properties::CodePointMapData<u16>);

    impl CodePointMapData16 {
        /// Gets the value for a code point.
        #[diplomat::rust_link(icu::properties::props::CodePointMapDataBorrowed::get, FnInStruct)]
        #[diplomat::rust_link(icu::properties::CodePointMapDataBorrowed::get32, FnInStruct, hidden)]
        #[diplomat::attr(auto, indexer)]
        pub fn get(&self, cp: DiplomatChar) -> u16 {
            self.0.as_borrowed().get32(cp)
        }

        /// Produces an iterator over ranges of code points that map to `value`
        #[diplomat::rust_link(
            icu::properties::CodePointMapDataBorrowed::iter_ranges_for_value,
            FnInStruct
        )]
        pub fn iter_ranges_for_value<'a>(&'a self, value: u16) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0.as_borrowed().iter_ranges_for_value(value),
            )))
        }

        /// Produces an iterator over ranges of code points that do not map to `value`
        #[diplomat::rust_link(
            icu::properties::CodePointMapDataBorrowed::iter_ranges_for_value_complemented,
            FnInStruct
        )]
        pub fn iter_ranges_for_value_complemented<'a>(
            &'a self,
            value: u16,
        ) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0
                    .as_borrowed()
                    .iter_ranges_for_value_complemented(value),
            )))
        }

        /// Gets a [`CodePointSetData`] representing all entries in this map that map to the given value
        #[diplomat::rust_link(
            icu::properties::CodePointMapDataBorrowed::get_set_for_value,
            FnInStruct
        )]
        pub fn get_set_for_value(&self, value: u16) -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                self.0.as_borrowed().get_set_for_value(value),
            ))
        }

        /// Create a map for the `Script` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Script, Struct)]
        #[diplomat::attr(auto, named_constructor = "script")]
        #[cfg(feature = "compiled_data")]
        pub fn create_script() -> Box<CodePointMapData16> {
            #[allow(clippy::unwrap_used)] // script is a 16-bit property
            let data = icu_properties::CodePointMapData::<Script>::new()
                .static_to_owned()
                .try_into_converted()
                .map_err(|_| ())
                .unwrap();
            Box::new(CodePointMapData16(data))
        }

        /// Create a map for the `Script` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Script, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "script_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_script_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointMapData16>, DataError> {
            #[allow(clippy::unwrap_used)] // script is a 16-bit property
            Ok(Box::new(CodePointMapData16(
                icu_properties::CodePointMapData::<Script>::try_new_unstable(
                    &provider.get_unstable()?,
                )?
                .try_into_converted()
                .map_err(|_| ())
                .unwrap(),
            )))
        }
    }
}
