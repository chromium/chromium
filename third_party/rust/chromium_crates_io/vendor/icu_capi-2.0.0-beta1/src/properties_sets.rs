// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_properties::props::GeneralCategory;
    use icu_properties::props::{
        Alnum, Alphabetic, AsciiHexDigit, BidiControl, BidiMirrored, Blank, CaseIgnorable,
        CaseSensitive, Cased, ChangesWhenCasefolded, ChangesWhenCasemapped, ChangesWhenLowercased,
        ChangesWhenNfkcCasefolded, ChangesWhenTitlecased, ChangesWhenUppercased, Dash,
        DefaultIgnorableCodePoint, Deprecated, Diacritic, Emoji, EmojiComponent, EmojiModifier,
        EmojiModifierBase, EmojiPresentation, ExtendedPictographic, Extender,
        FullCompositionExclusion, Graph, GraphemeBase, GraphemeExtend, GraphemeLink, HexDigit,
        Hyphen, IdContinue, IdStart, Ideographic, IdsBinaryOperator, IdsTrinaryOperator,
        JoinControl, LogicalOrderException, Lowercase, Math, NfcInert, NfdInert, NfkcInert,
        NfkdInert, NoncharacterCodePoint, PatternSyntax, PatternWhiteSpace,
        PrependedConcatenationMark, Print, QuotationMark, Radical, RegionalIndicator,
        SegmentStarter, SentenceTerminal, SoftDotted, TerminalPunctuation, UnifiedIdeograph,
        Uppercase, VariationSelector, WhiteSpace, Xdigit, XidContinue, XidStart,
    };

    use crate::errors::ffi::DataError;
    use crate::properties_iter::ffi::CodePointRangeIterator;
    use crate::provider::ffi::DataProvider;

    #[diplomat::opaque]
    /// An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.
    #[diplomat::rust_link(icu::properties, Mod)]
    #[diplomat::rust_link(icu::properties::CodePointSetData, Struct)]
    #[diplomat::rust_link(icu::properties::CodePointSetData::new, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::properties::CodePointSetDataBorrowed, Struct)]
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

        /// which is a mask with the same format as the `U_GC_XX_MASK` mask in ICU4C
        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup, Struct)]
        #[diplomat::rust_link(
            icu::properties::CodePointMapDataBorrowed::get_set_for_value_group,
            FnInStruct
        )]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "for_general_category_group")]
        pub fn load_for_general_category_group(
            provider: &DataProvider,
            group: u32,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(
                call_constructor_unstable!(
                    icu_properties::CodePointMapData::<GeneralCategory>::new [r => Ok(r.static_to_owned())],
                    icu_properties::CodePointMapData::<GeneralCategory>::try_new_unstable,
                    provider,
                )?
                .as_borrowed()
                .get_set_for_value_group(group.into()),
            )))
        }

        #[diplomat::rust_link(icu::properties::props::AsciiHexDigit, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "ascii_hex_digit")]
        pub fn load_ascii_hex_digit(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<AsciiHexDigit> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<AsciiHexDigit>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Alnum, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "alnum")]
        pub fn load_alnum(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Alnum> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Alnum>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Alphabetic, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "alphabetic")]
        pub fn load_alphabetic(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Alphabetic> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Alphabetic>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::BidiControl, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "bidi_control")]
        pub fn load_bidi_control(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<BidiControl> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<BidiControl>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::BidiMirrored, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "bidi_mirrored")]
        pub fn load_bidi_mirrored(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<BidiMirrored> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<BidiMirrored>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Blank, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "blank")]
        pub fn load_blank(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Blank> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Blank>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Cased, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "cased")]
        pub fn load_cased(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Cased> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Cased>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::CaseIgnorable, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "case_ignorable")]
        pub fn load_case_ignorable(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<CaseIgnorable> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<CaseIgnorable>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::FullCompositionExclusion, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "full_composition_exclusion")]
        pub fn load_full_composition_exclusion(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<FullCompositionExclusion> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<FullCompositionExclusion>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::ChangesWhenCasefolded, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "changes_when_casefolded")]
        pub fn load_changes_when_casefolded(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<ChangesWhenCasefolded> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenCasefolded>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::ChangesWhenCasemapped, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "changes_when_casemapped")]
        pub fn load_changes_when_casemapped(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<ChangesWhenCasemapped> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenCasemapped>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::ChangesWhenNfkcCasefolded, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "changes_when_nfkc_casefolded")]
        pub fn load_changes_when_nfkc_casefolded(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<ChangesWhenNfkcCasefolded> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenNfkcCasefolded>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::ChangesWhenLowercased, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "changes_when_lowercased")]
        pub fn load_changes_when_lowercased(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<ChangesWhenLowercased> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenLowercased>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::ChangesWhenTitlecased, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "changes_when_titlecased")]
        pub fn load_changes_when_titlecased(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<ChangesWhenTitlecased> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenTitlecased>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::ChangesWhenUppercased, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "changes_when_uppercased")]
        pub fn load_changes_when_uppercased(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<ChangesWhenUppercased> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<ChangesWhenUppercased>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Dash, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "dash")]
        pub fn load_dash(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Dash> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Dash>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Deprecated, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "deprecated")]
        pub fn load_deprecated(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Deprecated> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Deprecated>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::DefaultIgnorableCodePoint, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "default_ignorable_code_point")]
        pub fn load_default_ignorable_code_point(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<DefaultIgnorableCodePoint> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<DefaultIgnorableCodePoint>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Diacritic, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "diacritic")]
        pub fn load_diacritic(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Diacritic> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Diacritic>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::EmojiModifierBase, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "emoji_modifier_base")]
        pub fn load_emoji_modifier_base(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<EmojiModifierBase> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<EmojiModifierBase>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::EmojiComponent, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "emoji_component")]
        pub fn load_emoji_component(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<EmojiComponent> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<EmojiComponent>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::EmojiModifier, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "emoji_modifier")]
        pub fn load_emoji_modifier(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<EmojiModifier> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<EmojiModifier>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Emoji, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "emoji")]
        pub fn load_emoji(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Emoji> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Emoji>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::EmojiPresentation, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "emoji_presentation")]
        pub fn load_emoji_presentation(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<EmojiPresentation> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<EmojiPresentation>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Extender, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "extender")]
        pub fn load_extender(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Extender> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Extender>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::ExtendedPictographic, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "extended_pictographic")]
        pub fn load_extended_pictographic(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<ExtendedPictographic> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<ExtendedPictographic>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Graph, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "graph")]
        pub fn load_graph(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Graph> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Graph>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::GraphemeBase, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "grapheme_base")]
        pub fn load_grapheme_base(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<GraphemeBase> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<GraphemeBase>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::GraphemeExtend, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "grapheme_extend")]
        pub fn load_grapheme_extend(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<GraphemeExtend> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<GraphemeExtend>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::GraphemeLink, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "grapheme_link")]
        pub fn load_grapheme_link(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<GraphemeLink> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<GraphemeLink>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::HexDigit, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "hex_digit")]
        pub fn load_hex_digit(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<HexDigit> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<HexDigit>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Hyphen, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "hyphen")]
        pub fn load_hyphen(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Hyphen> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Hyphen>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::IdContinue, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "id_continue")]
        pub fn load_id_continue(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<IdContinue> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<IdContinue>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Ideographic, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "ideographic")]
        pub fn load_ideographic(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Ideographic> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Ideographic>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::IdStart, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "id_start")]
        pub fn load_id_start(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<IdStart> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<IdStart>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::IdsBinaryOperator, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "ids_binary_operator")]
        pub fn load_ids_binary_operator(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<IdsBinaryOperator> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<IdsBinaryOperator>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::IdsTrinaryOperator, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "ids_trinary_operator")]
        pub fn load_ids_trinary_operator(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<IdsTrinaryOperator> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<IdsTrinaryOperator>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::JoinControl, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "join_control")]
        pub fn load_join_control(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<JoinControl> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<JoinControl>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::LogicalOrderException, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "logical_order_exception")]
        pub fn load_logical_order_exception(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<LogicalOrderException> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<LogicalOrderException>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Lowercase, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "lowercase")]
        pub fn load_lowercase(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Lowercase> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Lowercase>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Math, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "math")]
        pub fn load_math(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Math> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Math>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::NoncharacterCodePoint, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "noncharacter_code_point")]
        pub fn load_noncharacter_code_point(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<NoncharacterCodePoint> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<NoncharacterCodePoint>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::NfcInert, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "nfc_inert")]
        pub fn load_nfc_inert(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<NfcInert> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<NfcInert>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::NfdInert, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "nfd_inert")]
        pub fn load_nfd_inert(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<NfdInert> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<NfdInert>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::NfkcInert, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "nfkc_inert")]
        pub fn load_nfkc_inert(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<NfkcInert> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<NfkcInert>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::NfkdInert, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "nfkd_inert")]
        pub fn load_nfkd_inert(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<NfkdInert> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<NfkdInert>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::PatternSyntax, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "pattern_syntax")]
        pub fn load_pattern_syntax(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<PatternSyntax> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<PatternSyntax>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::PatternWhiteSpace, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "pattern_white_space")]
        pub fn load_pattern_white_space(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<PatternWhiteSpace> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<PatternWhiteSpace>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::PrependedConcatenationMark, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "prepended_concatenation_mark")]
        pub fn load_prepended_concatenation_mark(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<PrependedConcatenationMark> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<PrependedConcatenationMark>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Print, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "print")]
        pub fn load_print(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Print> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Print>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::QuotationMark, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "quotation_mark")]
        pub fn load_quotation_mark(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<QuotationMark> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<QuotationMark>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Radical, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "radical")]
        pub fn load_radical(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Radical> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Radical>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::RegionalIndicator, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "regional_indicator")]
        pub fn load_regional_indicator(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<RegionalIndicator> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<RegionalIndicator>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::SoftDotted, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "soft_dotted")]
        pub fn load_soft_dotted(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<SoftDotted> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<SoftDotted>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::SegmentStarter, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "segment_starter")]
        pub fn load_segment_starter(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<SegmentStarter> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<SegmentStarter>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::CaseSensitive, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "case_sensitive")]
        pub fn load_case_sensitive(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<CaseSensitive> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<CaseSensitive>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::SentenceTerminal, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "sentence_terminal")]
        pub fn load_sentence_terminal(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<SentenceTerminal> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<SentenceTerminal>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::TerminalPunctuation, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "terminal_punctuation")]
        pub fn load_terminal_punctuation(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<TerminalPunctuation> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<TerminalPunctuation>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::UnifiedIdeograph, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "unified_ideograph")]
        pub fn load_unified_ideograph(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<UnifiedIdeograph> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<UnifiedIdeograph>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Uppercase, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "uppercase")]
        pub fn load_uppercase(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Uppercase> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Uppercase>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::VariationSelector, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "variation_selector")]
        pub fn load_variation_selector(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<VariationSelector> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<VariationSelector>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::WhiteSpace, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "white_space")]
        pub fn load_white_space(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<WhiteSpace> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<WhiteSpace>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::Xdigit, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "xdigit")]
        pub fn load_xdigit(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<Xdigit> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<Xdigit>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::XidContinue, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "xid_continue")]
        pub fn load_xid_continue(
            provider: &DataProvider,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<XidContinue> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<XidContinue>,
                provider
            )?)))
        }

        #[diplomat::rust_link(icu::properties::props::XidStart, Struct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "xid_start")]
        pub fn load_xid_start(provider: &DataProvider) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new::<XidStart> [r => Ok(r.static_to_owned())],
                icu_properties::CodePointSetData::try_new_unstable::<XidStart>,
                provider
            )?)))
        }

        /// Loads data for a property specified as a string as long as it is one of the
        /// [ECMA-262 binary properties][ecma] (not including Any, ASCII, and Assigned pseudoproperties).
        ///
        /// Returns `DataError::Custom` in case the string does not match any property in the list.
        ///
        /// [ecma]: https://tc39.es/ecma262/#table-binary-unicode-properties
        #[diplomat::rust_link(icu::properties::CodePointSetData::new_for_ecma262, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor = "for_ecma262")]
        pub fn load_for_ecma262(
            provider: &DataProvider,
            property_name: &DiplomatStr,
        ) -> Result<Box<CodePointSetData>, DataError> {
            Ok(Box::new(CodePointSetData(call_constructor_unstable!(
                icu_properties::CodePointSetData::new_for_ecma262 [r => r.map(|d| Ok(d.static_to_owned()))],
                icu_properties::CodePointSetData::try_new_for_ecma262_unstable,
                provider,
                property_name
            ).ok_or(DataError::Custom)??)))
        }
    }
}
