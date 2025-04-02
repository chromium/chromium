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
    BidiClassNameToValueV2, BidiClassValueToLongNameV1, BidiClassValueToShortNameV1,
    CanonicalCombiningClassNameToValueV2, CanonicalCombiningClassValueToLongNameV1,
    CanonicalCombiningClassValueToShortNameV1, EastAsianWidthNameToValueV2,
    EastAsianWidthValueToLongNameV1, EastAsianWidthValueToShortNameV1,
    GeneralCategoryMaskNameToValueV2, GeneralCategoryNameToValueV2,
    GeneralCategoryValueToLongNameV1, GeneralCategoryValueToShortNameV1,
    GraphemeClusterBreakNameToValueV2, GraphemeClusterBreakValueToLongNameV1,
    GraphemeClusterBreakValueToShortNameV1, HangulSyllableTypeNameToValueV2,
    HangulSyllableTypeValueToLongNameV1, HangulSyllableTypeValueToShortNameV1,
    IndicSyllabicCategoryNameToValueV2, IndicSyllabicCategoryValueToLongNameV1,
    IndicSyllabicCategoryValueToShortNameV1, JoiningTypeNameToValueV2,
    JoiningTypeValueToLongNameV1, JoiningTypeValueToShortNameV1, LineBreakNameToValueV2,
    LineBreakValueToLongNameV1, LineBreakValueToShortNameV1, ScriptNameToValueV2,
    ScriptValueToLongNameV1, ScriptValueToShortNameV1, SentenceBreakNameToValueV2,
    SentenceBreakValueToLongNameV1, SentenceBreakValueToShortNameV1, WordBreakNameToValueV2,
    WordBreakValueToLongNameV1, WordBreakValueToShortNameV1,
};

pub use crate::props::gc::GeneralCategoryULE;
use crate::props::*;
use crate::script::ScriptWithExt;
use core::ops::RangeInclusive;
use icu_collections::codepointinvlist::CodePointInversionList;
use icu_collections::codepointinvliststringlist::CodePointInversionListAndStringList;
use icu_collections::codepointtrie::{CodePointMapRange, CodePointTrie, TrieValue};
use icu_provider::prelude::*;
use zerofrom::ZeroFrom;
use zerovec::{VarZeroVec, ZeroSlice};

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
    impl_alnum_v1!(Baked);
    impl_alphabetic_v1!(Baked);
    impl_ascii_hex_digit_v1!(Baked);
    impl_basic_emoji_v1!(Baked);
    impl_bidi_class_name_to_value_v2!(Baked);
    impl_bidi_class_v1!(Baked);
    impl_bidi_class_value_to_long_name_v1!(Baked);
    impl_bidi_class_value_to_short_name_v1!(Baked);
    impl_bidi_control_v1!(Baked);
    impl_bidi_mirrored_v1!(Baked);
    impl_bidi_mirroring_glyph_v1!(Baked);
    impl_blank_v1!(Baked);
    impl_canonical_combining_class_name_to_value_v2!(Baked);
    impl_canonical_combining_class_v1!(Baked);
    impl_canonical_combining_class_value_to_long_name_v1!(Baked);
    impl_canonical_combining_class_value_to_short_name_v1!(Baked);
    impl_case_ignorable_v1!(Baked);
    impl_case_sensitive_v1!(Baked);
    impl_cased_v1!(Baked);
    impl_changes_when_casefolded_v1!(Baked);
    impl_changes_when_casemapped_v1!(Baked);
    impl_changes_when_lowercased_v1!(Baked);
    impl_changes_when_nfkc_casefolded_v1!(Baked);
    impl_changes_when_titlecased_v1!(Baked);
    impl_changes_when_uppercased_v1!(Baked);
    impl_dash_v1!(Baked);
    impl_default_ignorable_code_point_v1!(Baked);
    impl_deprecated_v1!(Baked);
    impl_diacritic_v1!(Baked);
    impl_east_asian_width_name_to_value_v2!(Baked);
    impl_east_asian_width_v1!(Baked);
    impl_east_asian_width_value_to_long_name_v1!(Baked);
    impl_east_asian_width_value_to_short_name_v1!(Baked);
    impl_emoji_component_v1!(Baked);
    impl_emoji_modifier_base_v1!(Baked);
    impl_emoji_modifier_v1!(Baked);
    impl_emoji_presentation_v1!(Baked);
    impl_emoji_v1!(Baked);
    impl_extended_pictographic_v1!(Baked);
    impl_extender_v1!(Baked);
    impl_full_composition_exclusion_v1!(Baked);
    impl_general_category_mask_name_to_value_v2!(Baked);
    impl_general_category_name_to_value_v2!(Baked);
    impl_general_category_v1!(Baked);
    impl_general_category_value_to_long_name_v1!(Baked);
    impl_general_category_value_to_short_name_v1!(Baked);
    impl_graph_v1!(Baked);
    impl_grapheme_base_v1!(Baked);
    impl_grapheme_cluster_break_name_to_value_v2!(Baked);
    impl_grapheme_cluster_break_v1!(Baked);
    impl_grapheme_cluster_break_value_to_long_name_v1!(Baked);
    impl_grapheme_cluster_break_value_to_short_name_v1!(Baked);
    impl_grapheme_extend_v1!(Baked);
    impl_grapheme_link_v1!(Baked);
    impl_hangul_syllable_type_name_to_value_v2!(Baked);
    impl_hangul_syllable_type_v1!(Baked);
    impl_hangul_syllable_type_value_to_long_name_v1!(Baked);
    impl_hangul_syllable_type_value_to_short_name_v1!(Baked);
    impl_hex_digit_v1!(Baked);
    impl_hyphen_v1!(Baked);
    impl_id_continue_v1!(Baked);
    impl_id_start_v1!(Baked);
    impl_ideographic_v1!(Baked);
    impl_ids_binary_operator_v1!(Baked);
    impl_ids_trinary_operator_v1!(Baked);
    impl_indic_syllabic_category_name_to_value_v2!(Baked);
    impl_indic_syllabic_category_v1!(Baked);
    impl_indic_syllabic_category_value_to_long_name_v1!(Baked);
    impl_indic_syllabic_category_value_to_short_name_v1!(Baked);
    impl_join_control_v1!(Baked);
    impl_joining_type_name_to_value_v2!(Baked);
    impl_joining_type_v1!(Baked);
    impl_joining_type_value_to_long_name_v1!(Baked);
    impl_joining_type_value_to_short_name_v1!(Baked);
    impl_line_break_name_to_value_v2!(Baked);
    impl_line_break_v1!(Baked);
    impl_line_break_value_to_long_name_v1!(Baked);
    impl_line_break_value_to_short_name_v1!(Baked);
    impl_logical_order_exception_v1!(Baked);
    impl_lowercase_v1!(Baked);
    impl_math_v1!(Baked);
    impl_nfc_inert_v1!(Baked);
    impl_nfd_inert_v1!(Baked);
    impl_nfkc_inert_v1!(Baked);
    impl_nfkd_inert_v1!(Baked);
    impl_noncharacter_code_point_v1!(Baked);
    impl_pattern_syntax_v1!(Baked);
    impl_pattern_white_space_v1!(Baked);
    impl_prepended_concatenation_mark_v1!(Baked);
    impl_print_v1!(Baked);
    impl_quotation_mark_v1!(Baked);
    impl_radical_v1!(Baked);
    impl_regional_indicator_v1!(Baked);
    impl_script_name_to_value_v2!(Baked);
    impl_script_v1!(Baked);
    impl_script_value_to_long_name_v1!(Baked);
    impl_script_value_to_short_name_v1!(Baked);
    impl_script_with_extensions_property_v1!(Baked);
    impl_segment_starter_v1!(Baked);
    impl_sentence_break_name_to_value_v2!(Baked);
    impl_sentence_break_v1!(Baked);
    impl_sentence_break_value_to_long_name_v1!(Baked);
    impl_sentence_break_value_to_short_name_v1!(Baked);
    impl_sentence_terminal_v1!(Baked);
    impl_soft_dotted_v1!(Baked);
    impl_terminal_punctuation_v1!(Baked);
    impl_unified_ideograph_v1!(Baked);
    impl_uppercase_v1!(Baked);
    impl_variation_selector_v1!(Baked);
    impl_white_space_v1!(Baked);
    impl_word_break_name_to_value_v2!(Baked);
    impl_word_break_v1!(Baked);
    impl_word_break_value_to_long_name_v1!(Baked);
    impl_word_break_value_to_short_name_v1!(Baked);
    impl_xdigit_v1!(Baked);
    impl_xid_continue_v1!(Baked);
    impl_xid_start_v1!(Baked);
};

icu_provider::data_marker!(
    /// `AlnumV1`
    AlnumV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `AlphabeticV1`
    AlphabeticV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `AsciiHexDigitV1`
    AsciiHexDigitV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `BidiControlV1`
    BidiControlV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `BidiMirroredV1`
    BidiMirroredV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `BlankV1`
    BlankV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `CasedV1`
    CasedV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `CaseIgnorableV1`
    CaseIgnorableV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `CaseSensitiveV1`
    CaseSensitiveV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ChangesWhenCasefoldedV1`
    ChangesWhenCasefoldedV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ChangesWhenCasemappedV1`
    ChangesWhenCasemappedV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ChangesWhenLowercasedV1`
    ChangesWhenLowercasedV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ChangesWhenNfkcCasefoldedV1`
    ChangesWhenNfkcCasefoldedV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ChangesWhenTitlecasedV1`
    ChangesWhenTitlecasedV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ChangesWhenUppercasedV1`
    ChangesWhenUppercasedV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `DashV1`
    DashV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `DefaultIgnorableCodePointV1`
    DefaultIgnorableCodePointV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `DeprecatedV1`
    DeprecatedV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `DiacriticV1`
    DiacriticV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `EmojiComponentV1`
    EmojiComponentV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `EmojiModifierBaseV1`
    EmojiModifierBaseV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `EmojiModifierV1`
    EmojiModifierV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `EmojiPresentationV1`
    EmojiPresentationV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `EmojiV1`
    EmojiV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ExtendedPictographicV1`
    ExtendedPictographicV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ExtenderV1`
    ExtenderV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `FullCompositionExclusionV1`
    FullCompositionExclusionV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GraphemeBaseV1`
    GraphemeBaseV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GraphemeExtendV1`
    GraphemeExtendV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GraphemeLinkV1`
    GraphemeLinkV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `GraphV1`
    GraphV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `HexDigitV1`
    HexDigitV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `HyphenV1`
    HyphenV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `IdContinueV1`
    IdContinueV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `IdeographicV1`
    IdeographicV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `IdsBinaryOperatorV1`
    IdsBinaryOperatorV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `IdStartV1`
    IdStartV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `IdsTrinaryOperatorV1`
    IdsTrinaryOperatorV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `JoinControlV1`
    JoinControlV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `LogicalOrderExceptionV1`
    LogicalOrderExceptionV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `LowercaseV1`
    LowercaseV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `MathV1`
    MathV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `NfcInertV1`
    NfcInertV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `NfdInertV1`
    NfdInertV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `NfkcInertV1`
    NfkcInertV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `NfkdInertV1`
    NfkdInertV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `NoncharacterCodePointV1`
    NoncharacterCodePointV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `PatternSyntaxV1`
    PatternSyntaxV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `PatternWhiteSpaceV1`
    PatternWhiteSpaceV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `PrependedConcatenationMarkV1`
    PrependedConcatenationMarkV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `PrintV1`
    PrintV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `QuotationMarkV1`
    QuotationMarkV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `RadicalV1`
    RadicalV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `RegionalIndicatorV1`
    RegionalIndicatorV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `SegmentStarterV1`
    SegmentStarterV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `SentenceTerminalV1`
    SentenceTerminalV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `SoftDottedV1`
    SoftDottedV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `TerminalPunctuationV1`
    TerminalPunctuationV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `UnifiedIdeographV1`
    UnifiedIdeographV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `UppercaseV1`
    UppercaseV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `VariationSelectorV1`
    VariationSelectorV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `WhiteSpaceV1`
    WhiteSpaceV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `XdigitV1`
    XdigitV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `XidContinueV1`
    XidContinueV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `XidStartV1`
    XidStartV1,
    PropertyCodePointSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// Data marker for the 'BidiClass' Unicode property
    BidiClassV1,
    PropertyCodePointMap<'static, crate::props::BidiClass>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'CanonicalCombiningClass' Unicode property
    CanonicalCombiningClassV1,
    PropertyCodePointMap<'static, crate::props::CanonicalCombiningClass>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'EastAsianWidth' Unicode property
    EastAsianWidthV1,
    PropertyCodePointMap<'static, crate::props::EastAsianWidth>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'GeneralCategory' Unicode property
    GeneralCategoryV1,
    PropertyCodePointMap<'static, crate::props::GeneralCategory>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'GraphemeClusterBreak' Unicode property
    GraphemeClusterBreakV1,
    PropertyCodePointMap<'static, crate::props::GraphemeClusterBreak>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'HangulSyllableType' Unicode property
    HangulSyllableTypeV1,
    PropertyCodePointMap<'static, crate::props::HangulSyllableType>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'IndicSyllabicCategory' Unicode property
    IndicSyllabicCategoryV1,
    PropertyCodePointMap<'static, crate::props::IndicSyllabicCategory>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'JoiningType' Unicode property
    JoiningTypeV1,
    PropertyCodePointMap<'static, crate::props::JoiningType>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'LineBreak' Unicode property
    LineBreakV1,
    PropertyCodePointMap<'static, crate::props::LineBreak>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'Script' Unicode property
    ScriptV1,
    PropertyCodePointMap<'static, crate::props::Script>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'SentenceBreak' Unicode property
    SentenceBreakV1,
    PropertyCodePointMap<'static, crate::props::SentenceBreak>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'WordBreak' Unicode property
    WordBreakV1,
    PropertyCodePointMap<'static, crate::props::WordBreak>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// Data marker for the 'BidiMirroringGlyph' Unicode property
    BidiMirroringGlyphV1,
    PropertyCodePointMap<'static, crate::bidi::BidiMirroringGlyph>,
    is_singleton = true,
);
icu_provider::data_marker!(
    /// `BasicEmojiV1`
    BasicEmojiV1,
    PropertyUnicodeSet<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// `ScriptWithExtensionsPropertyV1`
    ScriptWithExtensionsPropertyV1,
    ScriptWithExtensionsProperty<'static>,
    is_singleton = true
);

/// All data keys in this module.
pub const MARKERS: &[DataMarkerInfo] = &[
    AlnumV1::INFO,
    AlphabeticV1::INFO,
    AsciiHexDigitV1::INFO,
    BasicEmojiV1::INFO,
    BidiControlV1::INFO,
    BidiMirroredV1::INFO,
    BidiMirroringGlyphV1::INFO,
    BlankV1::INFO,
    CasedV1::INFO,
    CaseIgnorableV1::INFO,
    CaseSensitiveV1::INFO,
    ChangesWhenCasefoldedV1::INFO,
    ChangesWhenCasemappedV1::INFO,
    ChangesWhenLowercasedV1::INFO,
    ChangesWhenNfkcCasefoldedV1::INFO,
    ChangesWhenTitlecasedV1::INFO,
    ChangesWhenUppercasedV1::INFO,
    DashV1::INFO,
    DefaultIgnorableCodePointV1::INFO,
    DeprecatedV1::INFO,
    DiacriticV1::INFO,
    EmojiComponentV1::INFO,
    EmojiModifierBaseV1::INFO,
    EmojiModifierV1::INFO,
    EmojiPresentationV1::INFO,
    EmojiV1::INFO,
    ExtendedPictographicV1::INFO,
    ExtenderV1::INFO,
    FullCompositionExclusionV1::INFO,
    GraphemeBaseV1::INFO,
    GraphemeExtendV1::INFO,
    GraphemeLinkV1::INFO,
    GraphV1::INFO,
    HexDigitV1::INFO,
    HyphenV1::INFO,
    IdContinueV1::INFO,
    IdeographicV1::INFO,
    IdsBinaryOperatorV1::INFO,
    IdStartV1::INFO,
    IdsTrinaryOperatorV1::INFO,
    JoinControlV1::INFO,
    LogicalOrderExceptionV1::INFO,
    LowercaseV1::INFO,
    MathV1::INFO,
    NfcInertV1::INFO,
    NfdInertV1::INFO,
    NfkcInertV1::INFO,
    NfkdInertV1::INFO,
    NoncharacterCodePointV1::INFO,
    PatternSyntaxV1::INFO,
    PatternWhiteSpaceV1::INFO,
    PrependedConcatenationMarkV1::INFO,
    PrintV1::INFO,
    QuotationMarkV1::INFO,
    RadicalV1::INFO,
    RegionalIndicatorV1::INFO,
    ScriptWithExtensionsPropertyV1::INFO,
    ScriptWithExtensionsPropertyV1::INFO,
    SegmentStarterV1::INFO,
    SentenceTerminalV1::INFO,
    SoftDottedV1::INFO,
    TerminalPunctuationV1::INFO,
    UnifiedIdeographV1::INFO,
    UppercaseV1::INFO,
    VariationSelectorV1::INFO,
    WhiteSpaceV1::INFO,
    XdigitV1::INFO,
    XidContinueV1::INFO,
    XidStartV1::INFO,
    BidiClassNameToValueV2::INFO,
    BidiClassV1::INFO,
    BidiClassValueToLongNameV1::INFO,
    BidiClassValueToShortNameV1::INFO,
    CanonicalCombiningClassNameToValueV2::INFO,
    CanonicalCombiningClassV1::INFO,
    CanonicalCombiningClassValueToLongNameV1::INFO,
    CanonicalCombiningClassValueToShortNameV1::INFO,
    EastAsianWidthNameToValueV2::INFO,
    EastAsianWidthV1::INFO,
    EastAsianWidthValueToLongNameV1::INFO,
    EastAsianWidthValueToShortNameV1::INFO,
    GeneralCategoryMaskNameToValueV2::INFO,
    GeneralCategoryNameToValueV2::INFO,
    GeneralCategoryV1::INFO,
    GeneralCategoryValueToLongNameV1::INFO,
    GeneralCategoryValueToShortNameV1::INFO,
    GraphemeClusterBreakNameToValueV2::INFO,
    GraphemeClusterBreakV1::INFO,
    GraphemeClusterBreakValueToLongNameV1::INFO,
    GraphemeClusterBreakValueToShortNameV1::INFO,
    HangulSyllableTypeNameToValueV2::INFO,
    HangulSyllableTypeV1::INFO,
    HangulSyllableTypeValueToLongNameV1::INFO,
    HangulSyllableTypeValueToShortNameV1::INFO,
    IndicSyllabicCategoryNameToValueV2::INFO,
    IndicSyllabicCategoryV1::INFO,
    IndicSyllabicCategoryValueToLongNameV1::INFO,
    IndicSyllabicCategoryValueToShortNameV1::INFO,
    JoiningTypeNameToValueV2::INFO,
    JoiningTypeV1::INFO,
    JoiningTypeValueToLongNameV1::INFO,
    JoiningTypeValueToShortNameV1::INFO,
    LineBreakNameToValueV2::INFO,
    LineBreakV1::INFO,
    LineBreakValueToLongNameV1::INFO,
    LineBreakValueToShortNameV1::INFO,
    ScriptNameToValueV2::INFO,
    ScriptV1::INFO,
    ScriptValueToLongNameV1::INFO,
    ScriptValueToShortNameV1::INFO,
    SentenceBreakNameToValueV2::INFO,
    SentenceBreakV1::INFO,
    SentenceBreakValueToLongNameV1::INFO,
    SentenceBreakValueToShortNameV1::INFO,
    WordBreakNameToValueV2::INFO,
    WordBreakV1::INFO,
    WordBreakValueToLongNameV1::INFO,
    WordBreakValueToShortNameV1::INFO,
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
#[derive(Debug, Eq, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[non_exhaustive]
pub enum PropertyCodePointSet<'data> {
    /// The set of characters, represented as an inversion list
    InversionList(#[cfg_attr(feature = "serde", serde(borrow))] CodePointInversionList<'data>),
    // new variants should go BELOW existing ones
    // Serde serializes based on variant name and index in the enum
    // https://docs.rs/serde/latest/serde/trait.Serializer.html#tymethod.serialize_unit_variant
}

icu_provider::data_struct!(
    PropertyCodePointSet<'_>,
    #[cfg(feature = "datagen")]
);

// See CodePointSetData for documentation of these functions
impl<'data> PropertyCodePointSet<'data> {
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
pub enum PropertyCodePointMap<'data, T: TrieValue> {
    /// A codepoint trie storing the data
    CodePointTrie(#[cfg_attr(feature = "serde", serde(borrow))] CodePointTrie<'data, T>),
    // new variants should go BELOW existing ones
    // Serde serializes based on variant name and index in the enum
    // https://docs.rs/serde/latest/serde/trait.Serializer.html#tymethod.serialize_unit_variant
}

icu_provider::data_struct!(
    <T: TrieValue> PropertyCodePointMap<'_, T>,
    #[cfg(feature = "datagen")]
);

// See CodePointMapData for documentation of these functions
impl<'data, T: TrieValue> PropertyCodePointMap<'data, T> {
    #[inline]
    pub(crate) fn get32(&self, ch: u32) -> T {
        match *self {
            Self::CodePointTrie(ref t) => t.get32(ch),
        }
    }

    #[inline]
    #[cfg(feature = "alloc")]
    pub(crate) fn try_into_converted<P>(
        self,
    ) -> Result<PropertyCodePointMap<'data, P>, zerovec::ule::UleError>
    where
        P: TrieValue,
    {
        match self {
            Self::CodePointTrie(t) => t
                .try_into_converted()
                .map(PropertyCodePointMap::CodePointTrie),
        }
    }

    #[inline]
    #[cfg(feature = "alloc")]
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
#[derive(Debug, Eq, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[non_exhaustive]
pub enum PropertyUnicodeSet<'data> {
    /// A set representing characters in an inversion list, and the strings in a list.
    CPInversionListStrList(
        #[cfg_attr(feature = "serde", serde(borrow))] CodePointInversionListAndStringList<'data>,
    ),
    // new variants should go BELOW existing ones
    // Serde serializes based on variant name and index in the enum
    // https://docs.rs/serde/latest/serde/trait.Serializer.html#tymethod.serialize_unit_variant
}

icu_provider::data_struct!(
    PropertyUnicodeSet<'_>,
    #[cfg(feature = "datagen")]
);

impl<'data> PropertyUnicodeSet<'data> {
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
#[derive(Debug, Eq, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_properties::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct ScriptWithExtensionsProperty<'data> {
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

icu_provider::data_struct!(
    ScriptWithExtensionsProperty<'_>,
    #[cfg(feature = "datagen")]
);
