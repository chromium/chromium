// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    use alloc::boxed::Box;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use icu_properties::props::GeneralCategory;
    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use icu_properties::props::{
        Alnum, Alphabetic, AsciiHexDigit, BidiControl, BidiMirrored, Blank, CaseIgnorable,
        CaseSensitive, Cased, ChangesWhenCasefolded, ChangesWhenCasemapped, ChangesWhenLowercased,
        ChangesWhenNfkcCasefolded, ChangesWhenTitlecased, ChangesWhenUppercased, Dash,
        DefaultIgnorableCodePoint, Deprecated, Diacritic, Emoji, EmojiComponent, EmojiModifier,
        EmojiModifierBase, EmojiPresentation, ExtendedPictographic, Extender,
        FullCompositionExclusion, Graph, GraphemeBase, GraphemeExtend, GraphemeLink, HexDigit,
        Hyphen, IdCompatMathContinue, IdCompatMathStart, IdContinue, IdStart, Ideographic,
        IdsBinaryOperator, IdsTrinaryOperator, IdsUnaryOperator, JoinControl,
        LogicalOrderException, Lowercase, Math, ModifierCombiningMark, NfcInert, NfdInert,
        NfkcInert, NfkdInert, NoncharacterCodePoint, PatternSyntax, PatternWhiteSpace,
        PrependedConcatenationMark, Print, QuotationMark, Radical, RegionalIndicator,
        SegmentStarter, SentenceTerminal, SoftDotted, TerminalPunctuation, UnifiedIdeograph,
        Uppercase, VariationSelector, WhiteSpace, Xdigit, XidContinue, XidStart,
    };

    #[cfg(any(feature = "compiled_data", feature = "buffer_provider"))]
    use crate::unstable::errors::ffi::DataError;
    use crate::unstable::properties_iter::ffi::CodePointRangeIterator;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;

    #[cfg(feature = "compiled_data")]
    use diplomat_runtime::DiplomatChar;

    #[diplomat::opaque]
    /// An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.
    #[diplomat::rust_link(icu::properties, Mod)]
    #[diplomat::rust_link(icu::properties::CodePointSetData, Struct)]
    #[diplomat::rust_link(icu::properties::CodePointSetData::new, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::properties::CodePointSetDataBorrowed::new, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::properties::CodePointSetDataBorrowed, Struct)]
    #[diplomat::attr(demo_gen, disable)] // TODO needs custom page
    pub struct CodePointSetData(pub icu_properties::CodePointSetData);

    impl CodePointSetData {
        /// Checks whether the code point is in the set.
        #[diplomat::rust_link(icu::properties::CodePointSetDataBorrowed::contains, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::CodePointSetDataBorrowed::contains32,
            FnInStruct,
            hidden
        )]
        pub fn contains(&self, cp: DiplomatChar) -> bool {
            self.0.as_borrowed().contains32(cp)
        }

        /// Produces an iterator over ranges of code points contained in this set
        #[diplomat::rust_link(icu::properties::CodePointSetDataBorrowed::iter_ranges, FnInStruct)]
        pub fn iter_ranges<'a>(&'a self) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0.as_borrowed().iter_ranges(),
            )))
        }

        /// Produces an iterator over ranges of code points not contained in this set
        #[diplomat::rust_link(
            icu::properties::CodePointSetDataBorrowed::iter_ranges_complemented,
            FnInStruct
        )]
        pub fn iter_ranges_complemented<'a>(&'a self) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0.as_borrowed().iter_ranges_complemented(),
            )))
        }

        /// Produces a set for obtaining General Category Group values
        /// which is a mask with the same format as the `U_GC_XX_MASK` mask in ICU4C, using compiled data.
        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup, Struct)]
        #[diplomat::rust_link(
            icu::properties::CodePointMapDataBorrowed::get_set_for_value_group,
            FnInStruct
        )]
        #[diplomat::attr(auto, named_constructor = "general_category_group")]
        #[cfg(feature = "compiled_data")]
        pub fn create_general_category_group(
            group: crate::unstable::properties_gcg::ffi::GeneralCategoryGroup,
        ) -> Box<CodePointSetData> {
            let data = icu_properties::CodePointMapData::<GeneralCategory>::new().static_to_owned();

            Box::new(CodePointSetData(
                data.as_borrowed()
                    .get_set_for_value_group(group.into_props_group()),
            ))
        }

        /// Produces a set for obtaining General Category Group values
        /// which is a mask with the same format as the `U_GC_XX_MASK` mask in ICU4C, using a provided data source.
        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup, Struct)]
        #[diplomat::rust_link(
            icu::properties::CodePointMapDataBorrowed::get_set_for_value_group,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "general_category_group_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_general_category_group_with_provider(
            provider: &DataProvider,
            group: u32,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointMapData::<GeneralCategory>::try_new_unstable(
                    &provider.get_unstable()?,
                )?
                .as_borrowed()
                .get_set_for_value_group(group.into()),
            )))
        }

        /// Get the `Ascii_Hex_Digit` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn ascii_hex_digit_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<AsciiHexDigit>().contains32(ch)
        }
        /// Create a set for the `Ascii_Hex_Digit` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::AsciiHexDigit, Struct)]
        #[diplomat::attr(auto, named_constructor = "ascii_hex_digit")]
        #[cfg(feature = "compiled_data")]
        pub fn create_ascii_hex_digit() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<AsciiHexDigit>().static_to_owned(),
            ))
        }

        /// Create a set for the `Ascii_Hex_Digit` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::AsciiHexDigit, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "ascii_hex_digit_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_ascii_hex_digit_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<AsciiHexDigit>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Alnum` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn alnum_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Alnum>().contains32(ch)
        }
        /// Create a set for the `Alnum` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Alnum, Struct)]
        #[diplomat::attr(auto, named_constructor = "alnum")]
        #[cfg(feature = "compiled_data")]
        pub fn create_alnum() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Alnum>().static_to_owned(),
            ))
        }

        /// Create a set for the `Alnum` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Alnum, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "alnum_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_alnum_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Alnum>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Alphabetic` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn alphabetic_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Alphabetic>().contains32(ch)
        }
        /// Create a set for the `Alphabetic` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Alphabetic, Struct)]
        #[diplomat::attr(auto, named_constructor = "alphabetic")]
        #[cfg(feature = "compiled_data")]
        pub fn create_alphabetic() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Alphabetic>().static_to_owned(),
            ))
        }

        /// Create a set for the `Alphabetic` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Alphabetic, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "alphabetic_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_alphabetic_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Alphabetic>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Bidi_Control` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn bidi_control_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<BidiControl>().contains32(ch)
        }
        /// Create a set for the `Bidi_Control` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::BidiControl, Struct)]
        #[diplomat::attr(auto, named_constructor = "bidi_control")]
        #[cfg(feature = "compiled_data")]
        pub fn create_bidi_control() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<BidiControl>().static_to_owned(),
            ))
        }

        /// Create a set for the `Bidi_Control` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::BidiControl, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "bidi_control_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_bidi_control_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<BidiControl>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Bidi_Mirrored` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn bidi_mirrored_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<BidiMirrored>().contains32(ch)
        }
        /// Create a set for the `Bidi_Mirrored` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::BidiMirrored, Struct)]
        #[diplomat::attr(auto, named_constructor = "bidi_mirrored")]
        #[cfg(feature = "compiled_data")]
        pub fn create_bidi_mirrored() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<BidiMirrored>().static_to_owned(),
            ))
        }

        /// Create a set for the `Bidi_Mirrored` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::BidiMirrored, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "bidi_mirrored_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_bidi_mirrored_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<BidiMirrored>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Blank` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn blank_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Blank>().contains32(ch)
        }
        /// Create a set for the `Blank` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Blank, Struct)]
        #[diplomat::attr(auto, named_constructor = "blank")]
        #[cfg(feature = "compiled_data")]
        pub fn create_blank() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Blank>().static_to_owned(),
            ))
        }

        /// Create a set for the `Blank` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Blank, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "blank_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_blank_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Blank>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Cased` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn cased_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Cased>().contains32(ch)
        }
        /// Create a set for the `Cased` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Cased, Struct)]
        #[diplomat::attr(auto, named_constructor = "cased")]
        #[cfg(feature = "compiled_data")]
        pub fn create_cased() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Cased>().static_to_owned(),
            ))
        }

        /// Create a set for the `Cased` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Cased, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "cased_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_cased_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Cased>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Case_Ignorable` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn case_ignorable_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<CaseIgnorable>().contains32(ch)
        }
        /// Create a set for the `Case_Ignorable` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::CaseIgnorable, Struct)]
        #[diplomat::attr(auto, named_constructor = "case_ignorable")]
        #[cfg(feature = "compiled_data")]
        pub fn create_case_ignorable() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<CaseIgnorable>().static_to_owned(),
            ))
        }

        /// Create a set for the `Case_Ignorable` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::CaseIgnorable, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "case_ignorable_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_case_ignorable_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<CaseIgnorable>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Full_Composition_Exclusion` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn full_composition_exclusion_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<FullCompositionExclusion>().contains32(ch)
        }
        /// Create a set for the `Full_Composition_Exclusion` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::FullCompositionExclusion, Struct)]
        #[diplomat::attr(auto, named_constructor = "full_composition_exclusion")]
        #[cfg(feature = "compiled_data")]
        pub fn create_full_composition_exclusion() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<FullCompositionExclusion>()
                    .static_to_owned(),
            ))
        }

        /// Create a set for the `Full_Composition_Exclusion` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::FullCompositionExclusion, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "full_composition_exclusion_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_full_composition_exclusion_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<FullCompositionExclusion>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Changes_When_Casefolded` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn changes_when_casefolded_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<ChangesWhenCasefolded>().contains32(ch)
        }
        /// Create a set for the `Changes_When_Casefolded` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenCasefolded, Struct)]
        #[diplomat::attr(auto, named_constructor = "changes_when_casefolded")]
        #[cfg(feature = "compiled_data")]
        pub fn create_changes_when_casefolded() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<ChangesWhenCasefolded>().static_to_owned(),
            ))
        }

        /// Create a set for the `Changes_When_Casefolded` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenCasefolded, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "changes_when_casefolded_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_changes_when_casefolded_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenCasefolded>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Changes_When_Casemapped` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn changes_when_casemapped_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<ChangesWhenCasemapped>().contains32(ch)
        }
        /// Create a set for the `Changes_When_Casemapped` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenCasemapped, Struct)]
        #[diplomat::attr(auto, named_constructor = "changes_when_casemapped")]
        #[cfg(feature = "compiled_data")]
        pub fn create_changes_when_casemapped() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<ChangesWhenCasemapped>().static_to_owned(),
            ))
        }

        /// Create a set for the `Changes_When_Casemapped` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenCasemapped, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "changes_when_casemapped_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_changes_when_casemapped_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenCasemapped>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Changes_When_Nfkc_Casefolded` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn changes_when_nfkc_casefolded_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<ChangesWhenNfkcCasefolded>().contains32(ch)
        }
        /// Create a set for the `Changes_When_Nfkc_Casefolded` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenNfkcCasefolded, Struct)]
        #[diplomat::attr(auto, named_constructor = "changes_when_nfkc_casefolded")]
        #[cfg(feature = "compiled_data")]
        pub fn create_changes_when_nfkc_casefolded() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<ChangesWhenNfkcCasefolded>()
                    .static_to_owned(),
            ))
        }

        /// Create a set for the `Changes_When_Nfkc_Casefolded` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenNfkcCasefolded, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "changes_when_nfkc_casefolded_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_changes_when_nfkc_casefolded_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenNfkcCasefolded>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Changes_When_Lowercased` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn changes_when_lowercased_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<ChangesWhenLowercased>().contains32(ch)
        }
        /// Create a set for the `Changes_When_Lowercased` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenLowercased, Struct)]
        #[diplomat::attr(auto, named_constructor = "changes_when_lowercased")]
        #[cfg(feature = "compiled_data")]
        pub fn create_changes_when_lowercased() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<ChangesWhenLowercased>().static_to_owned(),
            ))
        }

        /// Create a set for the `Changes_When_Lowercased` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenLowercased, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "changes_when_lowercased_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_changes_when_lowercased_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenLowercased>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Changes_When_Titlecased` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn changes_when_titlecased_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<ChangesWhenTitlecased>().contains32(ch)
        }
        /// Create a set for the `Changes_When_Titlecased` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenTitlecased, Struct)]
        #[diplomat::attr(auto, named_constructor = "changes_when_titlecased")]
        #[cfg(feature = "compiled_data")]
        pub fn create_changes_when_titlecased() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<ChangesWhenTitlecased>().static_to_owned(),
            ))
        }

        /// Create a set for the `Changes_When_Titlecased` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenTitlecased, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "changes_when_titlecased_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_changes_when_titlecased_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenTitlecased>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Changes_When_Uppercased` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn changes_when_uppercased_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<ChangesWhenUppercased>().contains32(ch)
        }
        /// Create a set for the `Changes_When_Uppercased` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenUppercased, Struct)]
        #[diplomat::attr(auto, named_constructor = "changes_when_uppercased")]
        #[cfg(feature = "compiled_data")]
        pub fn create_changes_when_uppercased() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<ChangesWhenUppercased>().static_to_owned(),
            ))
        }

        /// Create a set for the `Changes_When_Uppercased` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::ChangesWhenUppercased, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "changes_when_uppercased_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_changes_when_uppercased_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenUppercased>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Dash` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn dash_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Dash>().contains32(ch)
        }
        /// Create a set for the `Dash` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Dash, Struct)]
        #[diplomat::attr(auto, named_constructor = "dash")]
        #[cfg(feature = "compiled_data")]
        pub fn create_dash() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Dash>().static_to_owned(),
            ))
        }

        /// Create a set for the `Dash` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Dash, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "dash_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_dash_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Dash>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Deprecated` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn deprecated_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Deprecated>().contains32(ch)
        }
        /// Create a set for the `Deprecated` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Deprecated, Struct)]
        #[diplomat::attr(auto, named_constructor = "deprecated")]
        #[cfg(feature = "compiled_data")]
        pub fn create_deprecated() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Deprecated>().static_to_owned(),
            ))
        }

        /// Create a set for the `Deprecated` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Deprecated, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "deprecated_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_deprecated_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Deprecated>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Default_Ignorable_Code_Point` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn default_ignorable_code_point_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<DefaultIgnorableCodePoint>().contains32(ch)
        }
        /// Create a set for the `Default_Ignorable_Code_Point` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::DefaultIgnorableCodePoint, Struct)]
        #[diplomat::attr(auto, named_constructor = "default_ignorable_code_point")]
        #[cfg(feature = "compiled_data")]
        pub fn create_default_ignorable_code_point() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<DefaultIgnorableCodePoint>()
                    .static_to_owned(),
            ))
        }

        /// Create a set for the `Default_Ignorable_Code_Point` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::DefaultIgnorableCodePoint, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "default_ignorable_code_point_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_default_ignorable_code_point_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<DefaultIgnorableCodePoint>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Diacritic` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn diacritic_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Diacritic>().contains32(ch)
        }
        /// Create a set for the `Diacritic` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Diacritic, Struct)]
        #[diplomat::attr(auto, named_constructor = "diacritic")]
        #[cfg(feature = "compiled_data")]
        pub fn create_diacritic() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Diacritic>().static_to_owned(),
            ))
        }

        /// Create a set for the `Diacritic` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Diacritic, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "diacritic_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_diacritic_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Diacritic>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Emoji_Modifier_Base` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn emoji_modifier_base_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<EmojiModifierBase>().contains32(ch)
        }
        /// Create a set for the `Emoji_Modifier_Base` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::EmojiModifierBase, Struct)]
        #[diplomat::attr(auto, named_constructor = "emoji_modifier_base")]
        #[cfg(feature = "compiled_data")]
        pub fn create_emoji_modifier_base() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<EmojiModifierBase>().static_to_owned(),
            ))
        }

        /// Create a set for the `Emoji_Modifier_Base` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::EmojiModifierBase, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "emoji_modifier_base_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_emoji_modifier_base_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<EmojiModifierBase>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Emoji_Component` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn emoji_component_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<EmojiComponent>().contains32(ch)
        }
        /// Create a set for the `Emoji_Component` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::EmojiComponent, Struct)]
        #[diplomat::attr(auto, named_constructor = "emoji_component")]
        #[cfg(feature = "compiled_data")]
        pub fn create_emoji_component() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<EmojiComponent>().static_to_owned(),
            ))
        }

        /// Create a set for the `Emoji_Component` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::EmojiComponent, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "emoji_component_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_emoji_component_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<EmojiComponent>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Emoji_Modifier` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn emoji_modifier_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<EmojiModifier>().contains32(ch)
        }
        /// Create a set for the `Emoji_Modifier` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::EmojiModifier, Struct)]
        #[diplomat::attr(auto, named_constructor = "emoji_modifier")]
        #[cfg(feature = "compiled_data")]
        pub fn create_emoji_modifier() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<EmojiModifier>().static_to_owned(),
            ))
        }

        /// Create a set for the `Emoji_Modifier` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::EmojiModifier, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "emoji_modifier_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_emoji_modifier_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<EmojiModifier>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Emoji` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn emoji_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Emoji>().contains32(ch)
        }
        /// Create a set for the `Emoji` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Emoji, Struct)]
        #[diplomat::attr(auto, named_constructor = "emoji")]
        #[cfg(feature = "compiled_data")]
        pub fn create_emoji() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Emoji>().static_to_owned(),
            ))
        }

        /// Create a set for the `Emoji` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Emoji, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "emoji_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_emoji_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Emoji>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Emoji_Presentation` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn emoji_presentation_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<EmojiPresentation>().contains32(ch)
        }
        /// Create a set for the `Emoji_Presentation` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::EmojiPresentation, Struct)]
        #[diplomat::attr(auto, named_constructor = "emoji_presentation")]
        #[cfg(feature = "compiled_data")]
        pub fn create_emoji_presentation() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<EmojiPresentation>().static_to_owned(),
            ))
        }

        /// Create a set for the `Emoji_Presentation` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::EmojiPresentation, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "emoji_presentation_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_emoji_presentation_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<EmojiPresentation>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Extender` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn extender_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Extender>().contains32(ch)
        }
        /// Create a set for the `Extender` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Extender, Struct)]
        #[diplomat::attr(auto, named_constructor = "extender")]
        #[cfg(feature = "compiled_data")]
        pub fn create_extender() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Extender>().static_to_owned(),
            ))
        }

        /// Create a set for the `Extender` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Extender, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "extender_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_extender_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Extender>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Extended_Pictographic` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn extended_pictographic_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<ExtendedPictographic>().contains32(ch)
        }
        /// Create a set for the `Extended_Pictographic` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::ExtendedPictographic, Struct)]
        #[diplomat::attr(auto, named_constructor = "extended_pictographic")]
        #[cfg(feature = "compiled_data")]
        pub fn create_extended_pictographic() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<ExtendedPictographic>().static_to_owned(),
            ))
        }

        /// Create a set for the `Extended_Pictographic` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::ExtendedPictographic, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "extended_pictographic_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_extended_pictographic_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<ExtendedPictographic>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Graph` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn graph_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Graph>().contains32(ch)
        }
        /// Create a set for the `Graph` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Graph, Struct)]
        #[diplomat::attr(auto, named_constructor = "graph")]
        #[cfg(feature = "compiled_data")]
        pub fn create_graph() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Graph>().static_to_owned(),
            ))
        }

        /// Create a set for the `Graph` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Graph, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "graph_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_graph_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Graph>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Grapheme_Base` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn grapheme_base_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<GraphemeBase>().contains32(ch)
        }
        /// Create a set for the `Grapheme_Base` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::GraphemeBase, Struct)]
        #[diplomat::attr(auto, named_constructor = "grapheme_base")]
        #[cfg(feature = "compiled_data")]
        pub fn create_grapheme_base() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<GraphemeBase>().static_to_owned(),
            ))
        }

        /// Create a set for the `Grapheme_Base` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::GraphemeBase, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "grapheme_base_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_grapheme_base_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<GraphemeBase>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Grapheme_Extend` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn grapheme_extend_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<GraphemeExtend>().contains32(ch)
        }
        /// Create a set for the `Grapheme_Extend` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::GraphemeExtend, Struct)]
        #[diplomat::attr(auto, named_constructor = "grapheme_extend")]
        #[cfg(feature = "compiled_data")]
        pub fn create_grapheme_extend() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<GraphemeExtend>().static_to_owned(),
            ))
        }

        /// Create a set for the `Grapheme_Extend` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::GraphemeExtend, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "grapheme_extend_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_grapheme_extend_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<GraphemeExtend>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Grapheme_Link` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn grapheme_link_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<GraphemeLink>().contains32(ch)
        }
        /// Create a set for the `Grapheme_Link` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::GraphemeLink, Struct)]
        #[diplomat::attr(auto, named_constructor = "grapheme_link")]
        #[cfg(feature = "compiled_data")]
        pub fn create_grapheme_link() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<GraphemeLink>().static_to_owned(),
            ))
        }

        /// Create a set for the `Grapheme_Link` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::GraphemeLink, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "grapheme_link_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_grapheme_link_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<GraphemeLink>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Hex_Digit` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn hex_digit_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<HexDigit>().contains32(ch)
        }
        /// Create a set for the `Hex_Digit` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::HexDigit, Struct)]
        #[diplomat::attr(auto, named_constructor = "hex_digit")]
        #[cfg(feature = "compiled_data")]
        pub fn create_hex_digit() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<HexDigit>().static_to_owned(),
            ))
        }

        /// Create a set for the `Hex_Digit` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::HexDigit, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "hex_digit_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_hex_digit_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<HexDigit>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Hyphen` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn hyphen_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Hyphen>().contains32(ch)
        }
        /// Create a set for the `Hyphen` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Hyphen, Struct)]
        #[diplomat::attr(auto, named_constructor = "hyphen")]
        #[cfg(feature = "compiled_data")]
        pub fn create_hyphen() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Hyphen>().static_to_owned(),
            ))
        }

        /// Create a set for the `Hyphen` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Hyphen, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "hyphen_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_hyphen_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Hyphen>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `ID_Compat_Math_Continue` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn id_compat_math_continue_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<IdCompatMathContinue>().contains32(ch)
        }
        /// Create a set for the `ID_Compat_Math_Continue` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::IdCompatMathContinue, Struct)]
        #[diplomat::attr(auto, named_constructor = "id_compat_math_continue")]
        #[cfg(feature = "compiled_data")]
        pub fn create_id_compat_math_continue() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<IdCompatMathContinue>().static_to_owned(),
            ))
        }
        /// Create a set for the `ID_Compat_Math_Continue` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::IdCompatMathContinue, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "id_compat_math_continue_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_id_compat_math_continue_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<IdCompatMathContinue>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `ID_Compat_Math_Start` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn id_compat_math_start_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<IdCompatMathStart>().contains32(ch)
        }
        /// Create a set for the `ID_Compat_Math_Start` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::IdCompatMathStart, Struct)]
        #[diplomat::attr(auto, named_constructor = "id_compat_math_start")]
        #[cfg(feature = "compiled_data")]
        pub fn create_id_compat_math_start() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<IdCompatMathStart>().static_to_owned(),
            ))
        }
        /// Create a set for the `ID_Compat_Math_Start` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::IdCompatMathStart, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "id_compat_math_start_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_id_compat_math_start_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<IdCompatMathStart>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Id_Continue` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn id_continue_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<IdContinue>().contains32(ch)
        }
        /// Create a set for the `Id_Continue` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::IdContinue, Struct)]
        #[diplomat::attr(auto, named_constructor = "id_continue")]
        #[cfg(feature = "compiled_data")]
        pub fn create_id_continue() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<IdContinue>().static_to_owned(),
            ))
        }

        /// Create a set for the `Id_Continue` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::IdContinue, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "id_continue_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_id_continue_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<IdContinue>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Ideographic` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn ideographic_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Ideographic>().contains32(ch)
        }
        /// Create a set for the `Ideographic` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Ideographic, Struct)]
        #[diplomat::attr(auto, named_constructor = "ideographic")]
        #[cfg(feature = "compiled_data")]
        pub fn create_ideographic() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Ideographic>().static_to_owned(),
            ))
        }

        /// Create a set for the `Ideographic` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Ideographic, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "ideographic_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_ideographic_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Ideographic>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Id_Start` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn id_start_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<IdStart>().contains32(ch)
        }
        /// Create a set for the `Id_Start` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::IdStart, Struct)]
        #[diplomat::attr(auto, named_constructor = "id_start")]
        #[cfg(feature = "compiled_data")]
        pub fn create_id_start() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<IdStart>().static_to_owned(),
            ))
        }

        /// Create a set for the `Id_Start` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::IdStart, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "id_start_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_id_start_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<IdStart>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Ids_Binary_Operator` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn ids_binary_operator_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<IdsBinaryOperator>().contains32(ch)
        }
        /// Create a set for the `Ids_Binary_Operator` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::IdsBinaryOperator, Struct)]
        #[diplomat::attr(auto, named_constructor = "ids_binary_operator")]
        #[cfg(feature = "compiled_data")]
        pub fn create_ids_binary_operator() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<IdsBinaryOperator>().static_to_owned(),
            ))
        }

        /// Create a set for the `Ids_Binary_Operator` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::IdsBinaryOperator, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "ids_binary_operator_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_ids_binary_operator_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<IdsBinaryOperator>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Ids_Trinary_Operator` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn ids_trinary_operator_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<IdsTrinaryOperator>().contains32(ch)
        }
        /// Create a set for the `Ids_Trinary_Operator` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::IdsTrinaryOperator, Struct)]
        #[diplomat::attr(auto, named_constructor = "ids_trinary_operator")]
        #[cfg(feature = "compiled_data")]
        pub fn create_ids_trinary_operator() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<IdsTrinaryOperator>().static_to_owned(),
            ))
        }

        /// Create a set for the `Ids_Trinary_Operator` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::IdsTrinaryOperator, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "ids_trinary_operator_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_ids_trinary_operator_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<IdsTrinaryOperator>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Ids_Unary_Operator` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn ids_unary_operator_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<IdsUnaryOperator>().contains32(ch)
        }
        /// Create a set for the `Ids_Unary_Operator` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::IdsUnaryOperator, Struct)]
        #[diplomat::attr(auto, named_constructor = "ids_unary_operator")]
        #[cfg(feature = "compiled_data")]
        pub fn create_ids_unary_operator() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<IdsUnaryOperator>().static_to_owned(),
            ))
        }

        /// Create a set for the `Ids_Unary_Operator` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::IdsUnaryOperator, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "ids_unary_operator_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_ids_unary_operator_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<IdsUnaryOperator>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Join_Control` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn join_control_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<JoinControl>().contains32(ch)
        }
        /// Create a set for the `Join_Control` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::JoinControl, Struct)]
        #[diplomat::attr(auto, named_constructor = "join_control")]
        #[cfg(feature = "compiled_data")]
        pub fn create_join_control() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<JoinControl>().static_to_owned(),
            ))
        }

        /// Create a set for the `Join_Control` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::JoinControl, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "join_control_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_join_control_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<JoinControl>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Logical_Order_Exception` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn logical_order_exception_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<LogicalOrderException>().contains32(ch)
        }
        /// Create a set for the `Logical_Order_Exception` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::LogicalOrderException, Struct)]
        #[diplomat::attr(auto, named_constructor = "logical_order_exception")]
        #[cfg(feature = "compiled_data")]
        pub fn create_logical_order_exception() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<LogicalOrderException>().static_to_owned(),
            ))
        }

        /// Create a set for the `Logical_Order_Exception` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::LogicalOrderException, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "logical_order_exception_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_logical_order_exception_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<LogicalOrderException>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Lowercase` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn lowercase_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Lowercase>().contains32(ch)
        }
        /// Create a set for the `Lowercase` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Lowercase, Struct)]
        #[diplomat::attr(auto, named_constructor = "lowercase")]
        #[cfg(feature = "compiled_data")]
        pub fn create_lowercase() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Lowercase>().static_to_owned(),
            ))
        }

        /// Create a set for the `Lowercase` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Lowercase, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "lowercase_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_lowercase_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Lowercase>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Math` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn math_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Math>().contains32(ch)
        }
        /// Create a set for the `Math` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Math, Struct)]
        #[diplomat::attr(auto, named_constructor = "math")]
        #[cfg(feature = "compiled_data")]
        pub fn create_math() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Math>().static_to_owned(),
            ))
        }

        /// Create a set for the `Math` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Math, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "math_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_math_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Math>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Modifier_Combining_mark` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn modifier_combining_mark_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<ModifierCombiningMark>().contains32(ch)
        }
        /// Create a set for the `Modifier_Combining_mark` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::ModifierCombiningMark, Struct)]
        #[diplomat::attr(auto, named_constructor = "modifier_combining_mark")]
        #[cfg(feature = "compiled_data")]
        pub fn create_modifier_combining_mark() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<ModifierCombiningMark>().static_to_owned(),
            ))
        }

        /// Create a set for the `Modifier_Combining_mark` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::ModifierCombiningMark, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "modifier_combining_mark_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_modifier_combining_mark_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<ModifierCombiningMark>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Noncharacter_Code_Point` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn noncharacter_code_point_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<NoncharacterCodePoint>().contains32(ch)
        }
        /// Create a set for the `Noncharacter_Code_Point` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::NoncharacterCodePoint, Struct)]
        #[diplomat::attr(auto, named_constructor = "noncharacter_code_point")]
        #[cfg(feature = "compiled_data")]
        pub fn create_noncharacter_code_point() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<NoncharacterCodePoint>().static_to_owned(),
            ))
        }

        /// Create a set for the `Noncharacter_Code_Point` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::NoncharacterCodePoint, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "noncharacter_code_point_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_noncharacter_code_point_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<NoncharacterCodePoint>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Nfc_Inert` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn nfc_inert_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<NfcInert>().contains32(ch)
        }
        /// Create a set for the `Nfc_Inert` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::NfcInert, Struct)]
        #[diplomat::attr(auto, named_constructor = "nfc_inert")]
        #[cfg(feature = "compiled_data")]
        pub fn create_nfc_inert() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<NfcInert>().static_to_owned(),
            ))
        }

        /// Create a set for the `Nfc_Inert` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::NfcInert, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "nfc_inert_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_nfc_inert_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<NfcInert>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Nfd_Inert` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn nfd_inert_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<NfdInert>().contains32(ch)
        }
        /// Create a set for the `Nfd_Inert` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::NfdInert, Struct)]
        #[diplomat::attr(auto, named_constructor = "nfd_inert")]
        #[cfg(feature = "compiled_data")]
        pub fn create_nfd_inert() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<NfdInert>().static_to_owned(),
            ))
        }

        /// Create a set for the `Nfd_Inert` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::NfdInert, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "nfd_inert_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_nfd_inert_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<NfdInert>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Nfkc_Inert` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn nfkc_inert_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<NfkcInert>().contains32(ch)
        }
        /// Create a set for the `Nfkc_Inert` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::NfkcInert, Struct)]
        #[diplomat::attr(auto, named_constructor = "nfkc_inert")]
        #[cfg(feature = "compiled_data")]
        pub fn create_nfkc_inert() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<NfkcInert>().static_to_owned(),
            ))
        }

        /// Create a set for the `Nfkc_Inert` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::NfkcInert, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "nfkc_inert_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_nfkc_inert_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<NfkcInert>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Nfkd_Inert` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn nfkd_inert_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<NfkdInert>().contains32(ch)
        }
        /// Create a set for the `Nfkd_Inert` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::NfkdInert, Struct)]
        #[diplomat::attr(auto, named_constructor = "nfkd_inert")]
        #[cfg(feature = "compiled_data")]
        pub fn create_nfkd_inert() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<NfkdInert>().static_to_owned(),
            ))
        }

        /// Create a set for the `Nfkd_Inert` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::NfkdInert, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "nfkd_inert_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_nfkd_inert_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<NfkdInert>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Pattern_Syntax` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn pattern_syntax_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<PatternSyntax>().contains32(ch)
        }
        /// Create a set for the `Pattern_Syntax` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::PatternSyntax, Struct)]
        #[diplomat::attr(auto, named_constructor = "pattern_syntax")]
        #[cfg(feature = "compiled_data")]
        pub fn create_pattern_syntax() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<PatternSyntax>().static_to_owned(),
            ))
        }

        /// Create a set for the `Pattern_Syntax` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::PatternSyntax, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "pattern_syntax_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_pattern_syntax_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<PatternSyntax>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Pattern_White_Space` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn pattern_white_space_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<PatternWhiteSpace>().contains32(ch)
        }
        /// Create a set for the `Pattern_White_Space` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::PatternWhiteSpace, Struct)]
        #[diplomat::attr(auto, named_constructor = "pattern_white_space")]
        #[cfg(feature = "compiled_data")]
        pub fn create_pattern_white_space() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<PatternWhiteSpace>().static_to_owned(),
            ))
        }

        /// Create a set for the `Pattern_White_Space` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::PatternWhiteSpace, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "pattern_white_space_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_pattern_white_space_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<PatternWhiteSpace>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Prepended_Concatenation_Mark` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn prepended_concatenation_mark_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<PrependedConcatenationMark>().contains32(ch)
        }
        /// Create a set for the `Prepended_Concatenation_Mark` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::PrependedConcatenationMark, Struct)]
        #[diplomat::attr(auto, named_constructor = "prepended_concatenation_mark")]
        #[cfg(feature = "compiled_data")]
        pub fn create_prepended_concatenation_mark() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<PrependedConcatenationMark>()
                    .static_to_owned(),
            ))
        }

        /// Create a set for the `Prepended_Concatenation_Mark` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::PrependedConcatenationMark, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "prepended_concatenation_mark_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_prepended_concatenation_mark_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<PrependedConcatenationMark>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Print` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn print_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Print>().contains32(ch)
        }
        /// Create a set for the `Print` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Print, Struct)]
        #[diplomat::attr(auto, named_constructor = "print")]
        #[cfg(feature = "compiled_data")]
        pub fn create_print() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Print>().static_to_owned(),
            ))
        }

        /// Create a set for the `Print` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Print, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "print_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_print_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Print>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Quotation_Mark` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn quotation_mark_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<QuotationMark>().contains32(ch)
        }
        /// Create a set for the `Quotation_Mark` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::QuotationMark, Struct)]
        #[diplomat::attr(auto, named_constructor = "quotation_mark")]
        #[cfg(feature = "compiled_data")]
        pub fn create_quotation_mark() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<QuotationMark>().static_to_owned(),
            ))
        }

        /// Create a set for the `Quotation_Mark` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::QuotationMark, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "quotation_mark_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_quotation_mark_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<QuotationMark>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Radical` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn radical_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Radical>().contains32(ch)
        }
        /// Create a set for the `Radical` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Radical, Struct)]
        #[diplomat::attr(auto, named_constructor = "radical")]
        #[cfg(feature = "compiled_data")]
        pub fn create_radical() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Radical>().static_to_owned(),
            ))
        }

        /// Create a set for the `Radical` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Radical, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "radical_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_radical_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Radical>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Regional_Indicator` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn regional_indicator_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<RegionalIndicator>().contains32(ch)
        }
        /// Create a set for the `Regional_Indicator` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::RegionalIndicator, Struct)]
        #[diplomat::attr(auto, named_constructor = "regional_indicator")]
        #[cfg(feature = "compiled_data")]
        pub fn create_regional_indicator() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<RegionalIndicator>().static_to_owned(),
            ))
        }

        /// Create a set for the `Regional_Indicator` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::RegionalIndicator, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "regional_indicator_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_regional_indicator_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<RegionalIndicator>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Soft_Dotted` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn soft_dotted_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<SoftDotted>().contains32(ch)
        }
        /// Create a set for the `Soft_Dotted` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::SoftDotted, Struct)]
        #[diplomat::attr(auto, named_constructor = "soft_dotted")]
        #[cfg(feature = "compiled_data")]
        pub fn create_soft_dotted() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<SoftDotted>().static_to_owned(),
            ))
        }

        /// Create a set for the `Soft_Dotted` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::SoftDotted, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "soft_dotted_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_soft_dotted_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<SoftDotted>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Segment_Starter` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn segment_starter_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<SegmentStarter>().contains32(ch)
        }
        /// Create a set for the `Segment_Starter` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::SegmentStarter, Struct)]
        #[diplomat::attr(auto, named_constructor = "segment_starter")]
        #[cfg(feature = "compiled_data")]
        pub fn create_segment_starter() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<SegmentStarter>().static_to_owned(),
            ))
        }

        /// Create a set for the `Segment_Starter` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::SegmentStarter, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "segment_starter_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_segment_starter_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<SegmentStarter>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Case_Sensitive` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn case_sensitive_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<CaseSensitive>().contains32(ch)
        }
        /// Create a set for the `Case_Sensitive` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::CaseSensitive, Struct)]
        #[diplomat::attr(auto, named_constructor = "case_sensitive")]
        #[cfg(feature = "compiled_data")]
        pub fn create_case_sensitive() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<CaseSensitive>().static_to_owned(),
            ))
        }

        /// Create a set for the `Case_Sensitive` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::CaseSensitive, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "case_sensitive_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_case_sensitive_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<CaseSensitive>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Sentence_Terminal` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn sentence_terminal_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<SentenceTerminal>().contains32(ch)
        }
        /// Create a set for the `Sentence_Terminal` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::SentenceTerminal, Struct)]
        #[diplomat::attr(auto, named_constructor = "sentence_terminal")]
        #[cfg(feature = "compiled_data")]
        pub fn create_sentence_terminal() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<SentenceTerminal>().static_to_owned(),
            ))
        }

        /// Create a set for the `Sentence_Terminal` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::SentenceTerminal, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "sentence_terminal_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_sentence_terminal_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<SentenceTerminal>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Terminal_Punctuation` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn terminal_punctuation_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<TerminalPunctuation>().contains32(ch)
        }
        /// Create a set for the `Terminal_Punctuation` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::TerminalPunctuation, Struct)]
        #[diplomat::attr(auto, named_constructor = "terminal_punctuation")]
        #[cfg(feature = "compiled_data")]
        pub fn create_terminal_punctuation() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<TerminalPunctuation>().static_to_owned(),
            ))
        }

        /// Create a set for the `Terminal_Punctuation` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::TerminalPunctuation, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "terminal_punctuation_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_terminal_punctuation_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<TerminalPunctuation>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Unified_Ideograph` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn unified_ideograph_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<UnifiedIdeograph>().contains32(ch)
        }
        /// Create a set for the `Unified_Ideograph` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::UnifiedIdeograph, Struct)]
        #[diplomat::attr(auto, named_constructor = "unified_ideograph")]
        #[cfg(feature = "compiled_data")]
        pub fn create_unified_ideograph() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<UnifiedIdeograph>().static_to_owned(),
            ))
        }

        /// Create a set for the `Unified_Ideograph` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::UnifiedIdeograph, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "unified_ideograph_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_unified_ideograph_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<UnifiedIdeograph>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Uppercase` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn uppercase_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Uppercase>().contains32(ch)
        }
        /// Create a set for the `Uppercase` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Uppercase, Struct)]
        #[diplomat::attr(auto, named_constructor = "uppercase")]
        #[cfg(feature = "compiled_data")]
        pub fn create_uppercase() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Uppercase>().static_to_owned(),
            ))
        }

        /// Create a set for the `Uppercase` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Uppercase, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "uppercase_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_uppercase_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Uppercase>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Variation_Selector` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn variation_selector_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<VariationSelector>().contains32(ch)
        }
        /// Create a set for the `Variation_Selector` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::VariationSelector, Struct)]
        #[diplomat::attr(auto, named_constructor = "variation_selector")]
        #[cfg(feature = "compiled_data")]
        pub fn create_variation_selector() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<VariationSelector>().static_to_owned(),
            ))
        }

        /// Create a set for the `Variation_Selector` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::VariationSelector, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "variation_selector_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_variation_selector_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<VariationSelector>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `White_Space` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn white_space_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<WhiteSpace>().contains32(ch)
        }
        /// Create a set for the `White_Space` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::WhiteSpace, Struct)]
        #[diplomat::attr(auto, named_constructor = "white_space")]
        #[cfg(feature = "compiled_data")]
        pub fn create_white_space() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<WhiteSpace>().static_to_owned(),
            ))
        }

        /// Create a set for the `White_Space` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::WhiteSpace, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "white_space_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_white_space_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<WhiteSpace>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Xdigit` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn xdigit_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<Xdigit>().contains32(ch)
        }
        /// Create a set for the `Xdigit` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::Xdigit, Struct)]
        #[diplomat::attr(auto, named_constructor = "xdigit")]
        #[cfg(feature = "compiled_data")]
        pub fn create_xdigit() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<Xdigit>().static_to_owned(),
            ))
        }

        /// Create a set for the `Xdigit` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::Xdigit, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "xdigit_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_xdigit_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<Xdigit>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Xid_Continue` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn xid_continue_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<XidContinue>().contains32(ch)
        }
        /// Create a set for the `Xid_Continue` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::XidContinue, Struct)]
        #[diplomat::attr(auto, named_constructor = "xid_continue")]
        #[cfg(feature = "compiled_data")]
        pub fn create_xid_continue() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<XidContinue>().static_to_owned(),
            ))
        }

        /// Create a set for the `Xid_Continue` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::XidContinue, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "xid_continue_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_xid_continue_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<XidContinue>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// Get the `Xid_Start` value for a given character, using compiled data
        #[diplomat::rust_link(icu::properties::props::BinaryProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn xid_start_for_char(ch: DiplomatChar) -> bool {
            icu_properties::CodePointSetData::new::<XidStart>().contains32(ch)
        }
        /// Create a set for the `Xid_Start` property, using compiled data.
        #[diplomat::rust_link(icu::properties::props::XidStart, Struct)]
        #[diplomat::attr(auto, named_constructor = "xid_start")]
        #[cfg(feature = "compiled_data")]
        pub fn create_xid_start() -> Box<CodePointSetData> {
            Box::new(CodePointSetData(
                icu_properties::CodePointSetData::new::<XidStart>().static_to_owned(),
            ))
        }

        /// Create a set for the `Xid_Start` property, using a particular data source.
        #[diplomat::rust_link(icu::properties::props::XidStart, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "xid_start_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_xid_start_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_unstable::<XidStart>(
                    &provider.get_unstable()?,
                )?,
            )))
        }

        /// [ecma]: https://tc39.es/ecma262/#table-binary-unicode-properties
        #[diplomat::rust_link(icu::properties::CodePointSetData::new_for_ecma262, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "for_ecma262")]
        #[cfg(feature = "compiled_data")]
        pub fn create_for_ecma262(
            property_name: &DiplomatStr,
        ) -> Result<Box<CodePointSetData>, DataError> {
            let data = icu_properties::CodePointSetData::new_for_ecma262(property_name)
                .ok_or(DataError::Custom)?
                .static_to_owned();
            Ok(Box::new(CodePointSetData(data)))
        }

        /// [ecma]: https://tc39.es/ecma262/#table-binary-unicode-properties
        #[diplomat::rust_link(icu::properties::CodePointSetData::new_for_ecma262, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "for_ecma262_with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_for_ecma262_with_provider(
            provider: &DataProvider,
            property_name: &DiplomatStr,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                icu_properties::CodePointSetData::try_new_for_ecma262_unstable(
                    &provider.get_unstable()?,
                    property_name,
                )
                .ok_or(DataError::Custom)??,
            )))
        }
    }
}
