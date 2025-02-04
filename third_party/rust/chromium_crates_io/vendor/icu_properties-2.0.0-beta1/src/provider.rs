// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! 🚧 \[Unstable\] Data provider struct definitions for this ICU4X component.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

pub mod names;

pub use names::{
    BidiClassNameToValueV2Marker, BidiClassValueToLongNameV1Marker,
    BidiClassValueToShortNameV1Marker, CanonicalCombiningClassNameToValueV2Marker,
    CanonicalCombiningClassValueToLongNameV1Marker,
    CanonicalCombiningClassValueToShortNameV1Marker, EastAsianWidthNameToValueV2Marker,
    EastAsianWidthValueToLongNameV1Marker, EastAsianWidthValueToShortNameV1Marker,
    GeneralCategoryMaskNameToValueV2Marker, GeneralCategoryNameToValueV2Marker,
    GeneralCategoryValueToLongNameV1Marker, GeneralCategoryValueToShortNameV1Marker,
    GraphemeClusterBreakNameToValueV2Marker, GraphemeClusterBreakValueToLongNameV1Marker,
    GraphemeClusterBreakValueToShortNameV1Marker, HangulSyllableTypeNameToValueV2Marker,
    HangulSyllableTypeValueToLongNameV1Marker, HangulSyllableTypeValueToShortNameV1Marker,
    IndicSyllabicCategoryNameToValueV2Marker, IndicSyllabicCategoryValueToLongNameV1Marker,
    IndicSyllabicCategoryValueToShortNameV1Marker, JoiningTypeNameToValueV2Marker,
    JoiningTypeValueToLongNameV1Marker, JoiningTypeValueToShortNameV1Marker,
    LineBreakNameToValueV2Marker, LineBreakValueToLongNameV1Marker,
    LineBreakValueToShortNameV1Marker, ScriptNameToValueV2Marker, ScriptValueToLongNameV1Marker,
    ScriptValueToShortNameV1Marker, SentenceBreakNameToValueV2Marker,
    SentenceBreakValueToLongNameV1Marker, SentenceBreakValueToShortNameV1Marker,
    WordBreakNameToValueV2Marker, WordBreakValueToLongNameV1Marker,
    WordBreakValueToShortNameV1Marker,
};

use crate::bidi::BidiMirroringGlyph;
pub use crate::props::gc::GeneralCategoryULE;
use crate::script::ScriptWithExt;
use core::ops::RangeInclusive;
use icu_collections::codepointinvlist::CodePointInversionList;
use icu_collections::codepointinvliststringlist::CodePointInversionListAndStringList;
use icu_collections::codepointtrie::{CodePointMapRange, CodePointTrie, TrieValue};
use icu_provider::prelude::*;
use zerofrom::ZeroFrom;
use zerovec::{ule::UleError, VarZeroVec, ZeroSlice};

#[cfg(feature = "compiled_data")]
#[derive(Debug)]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub struct Baked;

#[cfg(feature = "compiled_data")]
#[allow(unused_imports)]
const _: () = {
    use icu_properties_data::*;
    pub mod icu {
        pub use crate as properties;
        pub use icu_collections as collections;
    }
    make_provider!(Baked);
    impl_alnum_v1_marker!(Baked);
    impl_alphabetic_v1_marker!(Baked);
    impl_ascii_hex_digit_v1_marker!(Baked);
    impl_basic_emoji_v1_marker!(Baked);
    impl_bidi_class_name_to_value_v2_marker!(Baked);
    impl_bidi_class_v1_marker!(Baked);
    impl_bidi_class_value_to_long_name_v1_marker!(Baked);
    impl_bidi_class_value_to_short_name_v1_marker!(Baked);
    impl_bidi_control_v1_marker!(Baked);
    impl_bidi_mirrored_v1_marker!(Baked);
    impl_bidi_mirroring_glyph_v1_marker!(Baked);
    impl_blank_v1_marker!(Baked);
    impl_canonical_combining_class_name_to_value_v2_marker!(Baked);
    impl_canonical_combining_class_v1_marker!(Baked);
    impl_canonical_combining_class_value_to_long_name_v1_marker!(Baked);
    impl_canonical_combining_class_value_to_short_name_v1_marker!(Baked);
    impl_case_ignorable_v1_marker!(Baked);
    impl_case_sensitive_v1_marker!(Baked);
    impl_cased_v1_marker!(Baked);
    impl_changes_when_casefolded_v1_marker!(Baked);
    impl_changes_when_casemapped_v1_marker!(Baked);
    impl_changes_when_lowercased_v1_marker!(Baked);
    impl_changes_when_nfkc_casefolded_v1_marker!(Baked);
    impl_changes_when_titlecased_v1_marker!(Baked);
    impl_changes_when_uppercased_v1_marker!(Baked);
    impl_dash_v1_marker!(Baked);
    impl_default_ignorable_code_point_v1_marker!(Baked);
    impl_deprecated_v1_marker!(Baked);
    impl_diacritic_v1_marker!(Baked);
    impl_east_asian_width_name_to_value_v2_marker!(Baked);
    impl_east_asian_width_v1_marker!(Baked);
    impl_east_asian_width_value_to_long_name_v1_marker!(Baked);
    impl_east_asian_width_value_to_short_name_v1_marker!(Baked);
    impl_emoji_component_v1_marker!(Baked);
    impl_emoji_modifier_base_v1_marker!(Baked);
    impl_emoji_modifier_v1_marker!(Baked);
    impl_emoji_presentation_v1_marker!(Baked);
    impl_emoji_v1_marker!(Baked);
    impl_extended_pictographic_v1_marker!(Baked);
    impl_extender_v1_marker!(Baked);
    impl_full_composition_exclusion_v1_marker!(Baked);
    impl_general_category_mask_name_to_value_v2_marker!(Baked);
    impl_general_category_name_to_value_v2_marker!(Baked);
    impl_general_category_v1_marker!(Baked);
    impl_general_category_value_to_long_name_v1_marker!(Baked);
    impl_general_category_value_to_short_name_v1_marker!(Baked);
    impl_graph_v1_marker!(Baked);
    impl_grapheme_base_v1_marker!(Baked);
    impl_grapheme_cluster_break_name_to_value_v2_marker!(Baked);
    impl_grapheme_cluster_break_v1_marker!(Baked);
    impl_grapheme_cluster_break_value_to_long_name_v1_marker!(Baked);
    impl_grapheme_cluster_break_value_to_short_name_v1_marker!(Baked);
    impl_grapheme_extend_v1_marker!(Baked);
    impl_grapheme_link_v1_marker!(Baked);
    impl_hangul_syllable_type_name_to_value_v2_marker!(Baked);
    impl_hangul_syllable_type_v1_marker!(Baked);
    impl_hangul_syllable_type_value_to_long_name_v1_marker!(Baked);
    impl_hangul_syllable_type_value_to_short_name_v1_marker!(Baked);
    impl_hex_digit_v1_marker!(Baked);
    impl_hyphen_v1_marker!(Baked);
    impl_id_continue_v1_marker!(Baked);
    impl_id_start_v1_marker!(Baked);
    impl_ideographic_v1_marker!(Baked);
    impl_ids_binary_operator_v1_marker!(Baked);
    impl_ids_trinary_operator_v1_marker!(Baked);
    impl_indic_syllabic_category_name_to_value_v2_marker!(Baked);
    impl_indic_syllabic_category_v1_marker!(Baked);
    impl_indic_syllabic_category_value_to_long_name_v1_marker!(Baked);
    impl_indic_syllabic_category_value_to_short_name_v1_marker!(Baked);
    impl_join_control_v1_marker!(Baked);
    impl_joining_type_name_to_value_v2_marker!(Baked);
    impl_joining_type_v1_marker!(Baked);
    impl_joining_type_value_to_long_name_v1_marker!(Baked);
    impl_joining_type_value_to_short_name_v1_marker!(Baked);
    impl_line_break_name_to_value_v2_marker!(Baked);
    impl_line_break_v1_marker!(Baked);
    impl_line_break_value_to_long_name_v1_marker!(Baked);
    impl_line_break_value_to_short_name_v1_marker!(Baked);
    impl_logical_order_exception_v1_marker!(Baked);
    impl_lowercase_v1_marker!(Baked);
    impl_math_v1_marker!(Baked);
    impl_nfc_inert_v1_marker!(Baked);
    impl_nfd_inert_v1_marker!(Baked);
    impl_nfkc_inert_v1_marker!(Baked);
    impl_nfkd_inert_v1_marker!(Baked);
    impl_noncharacter_code_point_v1_marker!(Baked);
    impl_pattern_syntax_v1_marker!(Baked);
    impl_pattern_white_space_v1_marker!(Baked);
    impl_prepended_concatenation_mark_v1_marker!(Baked);
    impl_print_v1_marker!(Baked);
    impl_quotation_mark_v1_marker!(Baked);
    impl_radical_v1_marker!(Baked);
    impl_regional_indicator_v1_marker!(Baked);
    impl_script_name_to_value_v2_marker!(Baked);
    impl_script_v1_marker!(Baked);
    impl_script_value_to_long_name_v1_marker!(Baked);
    impl_script_value_to_short_name_v1_marker!(Baked);
    impl_script_with_extensions_property_v1_marker!(Baked);
    impl_segment_starter_v1_marker!(Baked);
    impl_sentence_break_name_to_value_v2_marker!(Baked);
    impl_sentence_break_v1_marker!(Baked);
    impl_sentence_break_value_to_long_name_v1_marker!(Baked);
    impl_sentence_break_value_to_short_name_v1_marker!(Baked);
    impl_sentence_terminal_v1_marker!(Baked);
    impl_soft_dotted_v1_marker!(Baked);
    impl_terminal_punctuation_v1_marker!(Baked);
    impl_unified_ideograph_v1_marker!(Baked);
    impl_uppercase_v1_marker!(Baked);
    impl_variation_selector_v1_marker!(Baked);
    impl_white_space_v1_marker!(Baked);
    impl_word_break_name_to_value_v2_marker!(Baked);
    impl_word_break_v1_marker!(Baked);
    impl_word_break_value_to_long_name_v1_marker!(Baked);
    impl_word_break_value_to_short_name_v1_marker!(Baked);
    impl_xdigit_v1_marker!(Baked);
    impl_xid_continue_v1_marker!(Baked);
    impl_xid_start_v1_marker!(Baked);
};

/// All data keys in this module.
pub const MARKERS: &[DataMarkerInfo] = &[
    AlnumV1Marker::INFO,
    AlphabeticV1Marker::INFO,
    AsciiHexDigitV1Marker::INFO,
    BasicEmojiV1Marker::INFO,
    BidiControlV1Marker::INFO,
    BidiMirroredV1Marker::INFO,
    BidiMirroringGlyphV1Marker::INFO,
    BlankV1Marker::INFO,
    CasedV1Marker::INFO,
    CaseIgnorableV1Marker::INFO,
    CaseSensitiveV1Marker::INFO,
    ChangesWhenCasefoldedV1Marker::INFO,
    ChangesWhenCasemappedV1Marker::INFO,
    ChangesWhenLowercasedV1Marker::INFO,
    ChangesWhenNfkcCasefoldedV1Marker::INFO,
    ChangesWhenTitlecasedV1Marker::INFO,
    ChangesWhenUppercasedV1Marker::INFO,
    DashV1Marker::INFO,
    DefaultIgnorableCodePointV1Marker::INFO,
    DeprecatedV1Marker::INFO,
    DiacriticV1Marker::INFO,
    EmojiComponentV1Marker::INFO,
    EmojiModifierBaseV1Marker::INFO,
    EmojiModifierV1Marker::INFO,
    EmojiPresentationV1Marker::INFO,
    EmojiV1Marker::INFO,
    ExtendedPictographicV1Marker::INFO,
    ExtenderV1Marker::INFO,
    FullCompositionExclusionV1Marker::INFO,
    GraphemeBaseV1Marker::INFO,
    GraphemeExtendV1Marker::INFO,
    GraphemeLinkV1Marker::INFO,
    GraphV1Marker::INFO,
    HexDigitV1Marker::INFO,
    HyphenV1Marker::INFO,
    IdContinueV1Marker::INFO,
    IdeographicV1Marker::INFO,
    IdsBinaryOperatorV1Marker::INFO,
    IdStartV1Marker::INFO,
    IdsTrinaryOperatorV1Marker::INFO,
    JoinControlV1Marker::INFO,
    LogicalOrderExceptionV1Marker::INFO,
    LowercaseV1Marker::INFO,
    MathV1Marker::INFO,
    NfcInertV1Marker::INFO,
    NfdInertV1Marker::INFO,
    NfkcInertV1Marker::INFO,
    NfkdInertV1Marker::INFO,
    NoncharacterCodePointV1Marker::INFO,
    PatternSyntaxV1Marker::INFO,
    PatternWhiteSpaceV1Marker::INFO,
    PrependedConcatenationMarkV1Marker::INFO,
    PrintV1Marker::INFO,
    QuotationMarkV1Marker::INFO,
    RadicalV1Marker::INFO,
    RegionalIndicatorV1Marker::INFO,
    ScriptWithExtensionsPropertyV1Marker::INFO,
    ScriptWithExtensionsPropertyV1Marker::INFO,
    SegmentStarterV1Marker::INFO,
    SentenceTerminalV1Marker::INFO,
    SoftDottedV1Marker::INFO,
    TerminalPunctuationV1Marker::INFO,
    UnifiedIdeographV1Marker::INFO,
    UppercaseV1Marker::INFO,
    VariationSelectorV1Marker::INFO,
    WhiteSpaceV1Marker::INFO,
    XdigitV1Marker::INFO,
    XidContinueV1Marker::INFO,
    XidStartV1Marker::INFO,
    BidiClassNameToValueV2Marker::INFO,
    BidiClassV1Marker::INFO,
    BidiClassValueToLongNameV1Marker::INFO,
    BidiClassValueToShortNameV1Marker::INFO,
    CanonicalCombiningClassNameToValueV2Marker::INFO,
    CanonicalCombiningClassV1Marker::INFO,
    CanonicalCombiningClassValueToLongNameV1Marker::INFO,
    CanonicalCombiningClassValueToShortNameV1Marker::INFO,
    EastAsianWidthNameToValueV2Marker::INFO,
    EastAsianWidthV1Marker::INFO,
    EastAsianWidthValueToLongNameV1Marker::INFO,
    EastAsianWidthValueToShortNameV1Marker::INFO,
    GeneralCategoryMaskNameToValueV2Marker::INFO,
    GeneralCategoryNameToValueV2Marker::INFO,
    GeneralCategoryV1Marker::INFO,
    GeneralCategoryValueToLongNameV1Marker::INFO,
    GeneralCategoryValueToShortNameV1Marker::INFO,
    GraphemeClusterBreakNameToValueV2Marker::INFO,
    GraphemeClusterBreakV1Marker::INFO,
    GraphemeClusterBreakValueToLongNameV1Marker::INFO,
    GraphemeClusterBreakValueToShortNameV1Marker::INFO,
    HangulSyllableTypeNameToValueV2Marker::INFO,
    HangulSyllableTypeV1Marker::INFO,
    HangulSyllableTypeValueToLongNameV1Marker::INFO,
    HangulSyllableTypeValueToShortNameV1Marker::INFO,
    IndicSyllabicCategoryNameToValueV2Marker::INFO,
    IndicSyllabicCategoryV1Marker::INFO,
    IndicSyllabicCategoryValueToLongNameV1Marker::INFO,
    IndicSyllabicCategoryValueToShortNameV1Marker::INFO,
    JoiningTypeNameToValueV2Marker::INFO,
    JoiningTypeV1Marker::INFO,
    JoiningTypeValueToLongNameV1Marker::INFO,
    JoiningTypeValueToShortNameV1Marker::INFO,
    LineBreakNameToValueV2Marker::INFO,
    LineBreakV1Marker::INFO,
    LineBreakValueToLongNameV1Marker::INFO,
    LineBreakValueToShortNameV1Marker::INFO,
    ScriptNameToValueV2Marker::INFO,
    ScriptV1Marker::INFO,
    ScriptValueToLongNameV1Marker::INFO,
    ScriptValueToShortNameV1Marker::INFO,
    SentenceBreakNameToValueV2Marker::INFO,
    SentenceBreakV1Marker::INFO,
    SentenceBreakValueToLongNameV1Marker::INFO,
    SentenceBreakValueToShortNameV1Marker::INFO,
    WordBreakNameToValueV2Marker::INFO,
    WordBreakV1Marker::INFO,
    WordBreakValueToLongNameV1Marker::INFO,
    WordBreakValueToShortNameV1Marker::INFO,
];

/// A set of characters which share a particular property value.
///
/// This data enum is extensible, more backends may be added in the future.
/// Old data can be used with newer code but not vice versa.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(AlnumV1Marker, "props/alnum@1", singleton),
    marker(AlphabeticV1Marker, "props/Alpha@1", singleton),
    marker(AsciiHexDigitV1Marker, "props/AHex@1", singleton),
    marker(BidiControlV1Marker, "props/Bidi_C@1", singleton),
    marker(BidiMirroredV1Marker, "props/Bidi_M@1", singleton),
    marker(BlankV1Marker, "props/blank@1", singleton),
    marker(CasedV1Marker, "props/Cased@1", singleton),
    marker(CaseIgnorableV1Marker, "props/CI@1", singleton),
    marker(CaseSensitiveV1Marker, "props/Sensitive@1", singleton),
    marker(ChangesWhenCasefoldedV1Marker, "props/CWCF@1", singleton),
    marker(ChangesWhenCasemappedV1Marker, "props/CWCM@1", singleton),
    marker(ChangesWhenLowercasedV1Marker, "props/CWL@1", singleton),
    marker(ChangesWhenNfkcCasefoldedV1Marker, "props/CWKCF@1", singleton),
    marker(ChangesWhenTitlecasedV1Marker, "props/CWT@1", singleton),
    marker(ChangesWhenUppercasedV1Marker, "props/CWU@1", singleton),
    marker(DashV1Marker, "props/Dash@1", singleton),
    marker(DefaultIgnorableCodePointV1Marker, "props/DI@1", singleton),
    marker(DeprecatedV1Marker, "props/Dep@1", singleton),
    marker(DiacriticV1Marker, "props/Dia@1", singleton),
    marker(EmojiComponentV1Marker, "props/EComp@1", singleton),
    marker(EmojiModifierBaseV1Marker, "props/EBase@1", singleton),
    marker(EmojiModifierV1Marker, "props/EMod@1", singleton),
    marker(EmojiPresentationV1Marker, "props/EPres@1", singleton),
    marker(EmojiV1Marker, "props/Emoji@1", singleton),
    marker(ExtendedPictographicV1Marker, "props/ExtPict@1", singleton),
    marker(ExtenderV1Marker, "props/Ext@1", singleton),
    marker(FullCompositionExclusionV1Marker, "props/Comp_Ex@1", singleton),
    marker(GraphemeBaseV1Marker, "props/Gr_Base@1", singleton),
    marker(GraphemeExtendV1Marker, "props/Gr_Ext@1", singleton),
    marker(GraphemeLinkV1Marker, "props/Gr_Link@1", singleton),
    marker(GraphV1Marker, "props/graph@1", singleton),
    marker(HexDigitV1Marker, "props/Hex@1", singleton),
    marker(HyphenV1Marker, "props/Hyphen@1", singleton),
    marker(IdContinueV1Marker, "props/IDC@1", singleton),
    marker(IdeographicV1Marker, "props/Ideo@1", singleton),
    marker(IdsBinaryOperatorV1Marker, "props/IDSB@1", singleton),
    marker(IdStartV1Marker, "props/IDS@1", singleton),
    marker(IdsTrinaryOperatorV1Marker, "props/IDST@1", singleton),
    marker(JoinControlV1Marker, "props/Join_C@1", singleton),
    marker(LogicalOrderExceptionV1Marker, "props/LOE@1", singleton),
    marker(LowercaseV1Marker, "props/Lower@1", singleton),
    marker(MathV1Marker, "props/Math@1", singleton),
    marker(NfcInertV1Marker, "props/nfcinert@1", singleton),
    marker(NfdInertV1Marker, "props/nfdinert@1", singleton),
    marker(NfkcInertV1Marker, "props/nfkcinert@1", singleton),
    marker(NfkdInertV1Marker, "props/nfkdinert@1", singleton),
    marker(NoncharacterCodePointV1Marker, "props/NChar@1", singleton),
    marker(PatternSyntaxV1Marker, "props/Pat_Syn@1", singleton),
    marker(PatternWhiteSpaceV1Marker, "props/Pat_WS@1", singleton),
    marker(PrependedConcatenationMarkV1Marker, "props/PCM@1", singleton),
    marker(PrintV1Marker, "props/print@1", singleton),
    marker(QuotationMarkV1Marker, "props/QMark@1", singleton),
    marker(RadicalV1Marker, "props/Radical@1", singleton),
    marker(RegionalIndicatorV1Marker, "props/RI@1", singleton),
    marker(SegmentStarterV1Marker, "props/segstart@1", singleton),
    marker(SentenceTerminalV1Marker, "props/STerm@1", singleton),
    marker(SoftDottedV1Marker, "props/SD@1", singleton),
    marker(TerminalPunctuationV1Marker, "props/Term@1", singleton),
    marker(UnifiedIdeographV1Marker, "props/UIdeo@1", singleton),
    marker(UppercaseV1Marker, "props/Upper@1", singleton),
    marker(VariationSelectorV1Marker, "props/VS@1", singleton),
    marker(WhiteSpaceV1Marker, "props/WSpace@1", singleton),
    marker(XdigitV1Marker, "props/xdigit@1", singleton),
    marker(XidContinueV1Marker, "props/XIDC@1", singleton),
    marker(XidStartV1Marker, "props/XIDS@1", singleton)
)]
#[derive(Debug, Eq, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[non_exhaustive]
pub enum PropertyCodePointSetV1<'data> {
    /// The set of characters, represented as an inversion list
    InversionList(#[cfg_attr(feature = "serde", serde(borrow))] CodePointInversionList<'data>),
    // new variants should go BELOW existing ones
    // Serde serializes based on variant name and index in the enum
    // https://docs.rs/serde/latest/serde/trait.Serializer.html#tymethod.serialize_unit_variant
}

// See CodePointSetData for documentation of these functions
impl<'data> PropertyCodePointSetV1<'data> {
    #[inline]
    pub(crate) fn contains(&self, ch: char) -> bool {
        match *self {
            Self::InversionList(ref l) => l.contains(ch),
        }
    }

    #[inline]
    pub(crate) fn contains32(&self, ch: u32) -> bool {
        match *self {
            Self::InversionList(ref l) => l.contains32(ch),
        }
    }

    #[inline]
    pub(crate) fn iter_ranges(&self) -> impl Iterator<Item = RangeInclusive<u32>> + '_ {
        match *self {
            Self::InversionList(ref l) => l.iter_ranges(),
        }
    }

    #[inline]
    pub(crate) fn iter_ranges_complemented(
        &self,
    ) -> impl Iterator<Item = RangeInclusive<u32>> + '_ {
        match *self {
            Self::InversionList(ref l) => l.iter_ranges_complemented(),
        }
    }

    #[inline]
    pub(crate) fn from_code_point_inversion_list(l: CodePointInversionList<'static>) -> Self {
        Self::InversionList(l)
    }

    #[inline]
    pub(crate) fn as_code_point_inversion_list(
        &'_ self,
    ) -> Option<&'_ CodePointInversionList<'data>> {
        match *self {
            Self::InversionList(ref l) => Some(l),
            // any other backing data structure that cannot return a CPInvList in O(1) time should return None
        }
    }

    #[inline]
    pub(crate) fn to_code_point_inversion_list(&self) -> CodePointInversionList<'_> {
        match *self {
            Self::InversionList(ref t) => ZeroFrom::zero_from(t),
        }
    }
}

/// A map efficiently storing data about individual characters.
///
/// This data enum is extensible, more backends may be added in the future.
/// Old data can be used with newer code but not vice versa.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, Debug, Eq, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[non_exhaustive]
pub enum PropertyCodePointMapV1<'data, T: TrieValue> {
    /// A codepoint trie storing the data
    CodePointTrie(#[cfg_attr(feature = "serde", serde(borrow))] CodePointTrie<'data, T>),
    // new variants should go BELOW existing ones
    // Serde serializes based on variant name and index in the enum
    // https://docs.rs/serde/latest/serde/trait.Serializer.html#tymethod.serialize_unit_variant
}

macro_rules! data_struct_generic {
    ($(marker($marker:ident, $ty:ident, $path:literal),)+) => {
        $(
            #[doc = core::concat!("Data marker for the '", stringify!($ty), "' Unicode property")]
            #[derive(Debug, Default)]
            #[cfg_attr(feature = "datagen", derive(databake::Bake))]
            #[cfg_attr(feature = "datagen", databake(path = icu_properties::provider))]
            pub struct $marker;
            impl icu_provider::DynamicDataMarker for $marker {
                type DataStruct = PropertyCodePointMapV1<'static, $ty>;
            }
            impl icu_provider::DataMarker for $marker {
                const INFO: icu_provider::DataMarkerInfo = {
                    let mut info = DataMarkerInfo::from_path(icu_provider::marker::data_marker_path!($path));
                    info.is_singleton = true;
                    info
                };
            }
        )+
    }
}

use crate::props::*;

data_struct_generic!(
    marker(BidiClassV1Marker, BidiClass, "props/bc@1"),
    marker(
        CanonicalCombiningClassV1Marker,
        CanonicalCombiningClass,
        "props/ccc@1"
    ),
    marker(EastAsianWidthV1Marker, EastAsianWidth, "props/ea@1"),
    marker(GeneralCategoryV1Marker, GeneralCategory, "props/gc@1"),
    marker(
        GraphemeClusterBreakV1Marker,
        GraphemeClusterBreak,
        "props/GCB@1"
    ),
    marker(
        HangulSyllableTypeV1Marker,
        HangulSyllableType,
        "props/hst@1"
    ),
    marker(
        IndicSyllabicCategoryV1Marker,
        IndicSyllabicCategory,
        "props/InSC@1"
    ),
    marker(JoiningTypeV1Marker, JoiningType, "props/jt@1"),
    marker(LineBreakV1Marker, LineBreak, "props/lb@1"),
    marker(ScriptV1Marker, Script, "props/sc@1"),
    marker(SentenceBreakV1Marker, SentenceBreak, "props/SB@1"),
    marker(WordBreakV1Marker, WordBreak, "props/WB@1"),
    marker(
        BidiMirroringGlyphV1Marker,
        BidiMirroringGlyph,
        "props/Bidi_G@1"
    ),
);

// See CodePointMapData for documentation of these functions
impl<'data, T: TrieValue> PropertyCodePointMapV1<'data, T> {
    #[inline]
    pub(crate) fn get32(&self, ch: u32) -> T {
        match *self {
            Self::CodePointTrie(ref t) => t.get32(ch),
        }
    }

    #[inline]
    pub(crate) fn try_into_converted<P>(self) -> Result<PropertyCodePointMapV1<'data, P>, UleError>
    where
        P: TrieValue,
    {
        match self {
            Self::CodePointTrie(t) => t
                .try_into_converted()
                .map(PropertyCodePointMapV1::CodePointTrie),
        }
    }

    #[inline]
    pub(crate) fn get_set_for_value(&self, value: T) -> CodePointInversionList<'static> {
        match *self {
            Self::CodePointTrie(ref t) => t.get_set_for_value(value),
        }
    }

    #[inline]
    pub(crate) fn iter_ranges(&self) -> impl Iterator<Item = CodePointMapRange<T>> + '_ {
        match *self {
            Self::CodePointTrie(ref t) => t.iter_ranges(),
        }
    }
    #[inline]
    pub(crate) fn iter_ranges_mapped<'a, U: Eq + 'a>(
        &'a self,
        map: impl FnMut(T) -> U + Copy + 'a,
    ) -> impl Iterator<Item = CodePointMapRange<U>> + 'a {
        match *self {
            Self::CodePointTrie(ref t) => t.iter_ranges_mapped(map),
        }
    }

    #[inline]
    pub(crate) fn from_code_point_trie(trie: CodePointTrie<'static, T>) -> Self {
        Self::CodePointTrie(trie)
    }

    #[inline]
    pub(crate) fn as_code_point_trie(&self) -> Option<&CodePointTrie<'data, T>> {
        match *self {
            Self::CodePointTrie(ref t) => Some(t),
            // any other backing data structure that cannot return a CPT in O(1) time should return None
        }
    }

    #[inline]
    pub(crate) fn to_code_point_trie(&self) -> CodePointTrie<'_, T> {
        match *self {
            Self::CodePointTrie(ref t) => ZeroFrom::zero_from(t),
        }
    }
}

/// A set of characters and strings which share a particular property value.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(BasicEmojiV1Marker, "props/Basic_Emoji@1", singleton))]
#[derive(Debug, Eq, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[non_exhaustive]
pub enum PropertyUnicodeSetV1<'data> {
    /// A set representing characters in an inversion list, and the strings in a list.
    CPInversionListStrList(
        #[cfg_attr(feature = "serde", serde(borrow))] CodePointInversionListAndStringList<'data>,
    ),
    // new variants should go BELOW existing ones
    // Serde serializes based on variant name and index in the enum
    // https://docs.rs/serde/latest/serde/trait.Serializer.html#tymethod.serialize_unit_variant
}

impl<'data> PropertyUnicodeSetV1<'data> {
    #[inline]
    pub(crate) fn contains_str(&self, s: &str) -> bool {
        match *self {
            Self::CPInversionListStrList(ref l) => l.contains_str(s),
        }
    }

    #[inline]
    pub(crate) fn contains32(&self, cp: u32) -> bool {
        match *self {
            Self::CPInversionListStrList(ref l) => l.contains32(cp),
        }
    }

    #[inline]
    pub(crate) fn contains(&self, ch: char) -> bool {
        match *self {
            Self::CPInversionListStrList(ref l) => l.contains(ch),
        }
    }

    #[inline]
    pub(crate) fn from_code_point_inversion_list_string_list(
        l: CodePointInversionListAndStringList<'static>,
    ) -> Self {
        Self::CPInversionListStrList(l)
    }

    #[inline]
    pub(crate) fn as_code_point_inversion_list_string_list(
        &'_ self,
    ) -> Option<&'_ CodePointInversionListAndStringList<'data>> {
        match *self {
            Self::CPInversionListStrList(ref l) => Some(l),
            // any other backing data structure that cannot return a CPInversionListStrList in O(1) time should return None
        }
    }

    #[inline]
    pub(crate) fn to_code_point_inversion_list_string_list(
        &self,
    ) -> CodePointInversionListAndStringList<'_> {
        match *self {
            Self::CPInversionListStrList(ref t) => ZeroFrom::zero_from(t),
        }
    }
}

/// A struct that efficiently stores `Script` and `Script_Extensions` property data.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(
    ScriptWithExtensionsPropertyV1Marker,
    "props/scx@1",
    singleton
))]
#[derive(Debug, Eq, PartialEq, Clone)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct ScriptWithExtensionsPropertyV1<'data> {
    /// Note: The `ScriptWithExt` values in this array will assume a 12-bit layout. The 2
    /// higher order bits 11..10 will indicate how to deduce the Script value and
    /// Script_Extensions value, nearly matching the representation
    /// [in ICU](https://github.com/unicode-org/icu/blob/main/icu4c/source/common/uprops.h):
    ///
    /// | High order 2 bits value | Script                                                 | Script_Extensions                                              |
    /// |-------------------------|--------------------------------------------------------|----------------------------------------------------------------|
    /// | 3                       | First value in sub-array, index given by lower 10 bits | Sub-array excluding first value, index given by lower 10 bits  |
    /// | 2                       | Script=Inherited                                       | Entire sub-array, index given by lower 10 bits                 |
    /// | 1                       | Script=Common                                          | Entire sub-array, index given by lower 10 bits                 |
    /// | 0                       | Value in lower 10 bits                                 | `[ Script value ]` single-element array                        |
    ///
    /// When the lower 10 bits of the value are used as an index, that index is
    /// used for the outer-level vector of the nested `extensions` structure.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub trie: CodePointTrie<'data, ScriptWithExt>,

    /// This companion structure stores Script_Extensions values, which are
    /// themselves arrays / vectors. This structure only stores the values for
    /// cases in which `scx(cp) != [ sc(cp) ]`. Each sub-vector is distinct. The
    /// sub-vector represents the Script_Extensions array value for a code point,
    /// and may also indicate Script value, as described for the `trie` field.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub extensions: VarZeroVec<'data, ZeroSlice<Script>>,
}
