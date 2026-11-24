//! AAT feat-table feature and selector metadata.
use crate::face::hr_face_t;
use crate::object;
use crate::ot_name::hr_ot_name_id_t;
use core::ffi::c_uint;
use read_fonts::{
    tables::feat::{Feat, SettingName},
    types::FixedSize,
    TableProvider,
};

/// An AAT feature type.
pub type hr_aat_layout_feature_type_t = c_uint;
/// An AAT feature selector.
pub type hr_aat_layout_feature_selector_t = c_uint;
/// No exclusive default selector is available.
pub const HR_AAT_LAYOUT_NO_SELECTOR_INDEX: c_uint = 0xFFFFu32;

pub const HR_AAT_LAYOUT_FEATURE_TYPE_INVALID: hr_aat_layout_feature_type_t = 0xFFFFu32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_ALL_TYPOGRAPHIC: hr_aat_layout_feature_type_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_LIGATURES: hr_aat_layout_feature_type_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_CURSIVE_CONNECTION: hr_aat_layout_feature_type_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_LETTER_CASE: hr_aat_layout_feature_type_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_VERTICAL_SUBSTITUTION: hr_aat_layout_feature_type_t = 4u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_LINGUISTIC_REARRANGEMENT: hr_aat_layout_feature_type_t = 5u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_NUMBER_SPACING: hr_aat_layout_feature_type_t = 6u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_SMART_SWASH_TYPE: hr_aat_layout_feature_type_t = 8u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_DIACRITICS_TYPE: hr_aat_layout_feature_type_t = 9u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_VERTICAL_POSITION: hr_aat_layout_feature_type_t = 10u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_FRACTIONS: hr_aat_layout_feature_type_t = 11u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_OVERLAPPING_CHARACTERS_TYPE: hr_aat_layout_feature_type_t =
    13u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_TYPOGRAPHIC_EXTRAS: hr_aat_layout_feature_type_t = 14u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_MATHEMATICAL_EXTRAS: hr_aat_layout_feature_type_t = 15u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_ORNAMENT_SETS_TYPE: hr_aat_layout_feature_type_t = 16u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_CHARACTER_ALTERNATIVES: hr_aat_layout_feature_type_t = 17u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_DESIGN_COMPLEXITY_TYPE: hr_aat_layout_feature_type_t = 18u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_STYLE_OPTIONS: hr_aat_layout_feature_type_t = 19u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_CHARACTER_SHAPE: hr_aat_layout_feature_type_t = 20u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_NUMBER_CASE: hr_aat_layout_feature_type_t = 21u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_TEXT_SPACING: hr_aat_layout_feature_type_t = 22u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_TRANSLITERATION: hr_aat_layout_feature_type_t = 23u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_ANNOTATION_TYPE: hr_aat_layout_feature_type_t = 24u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_KANA_SPACING_TYPE: hr_aat_layout_feature_type_t = 25u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_IDEOGRAPHIC_SPACING_TYPE: hr_aat_layout_feature_type_t = 26u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_UNICODE_DECOMPOSITION_TYPE: hr_aat_layout_feature_type_t =
    27u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_RUBY_KANA: hr_aat_layout_feature_type_t = 28u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_CJK_SYMBOL_ALTERNATIVES_TYPE: hr_aat_layout_feature_type_t =
    29u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_IDEOGRAPHIC_ALTERNATIVES_TYPE: hr_aat_layout_feature_type_t =
    30u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_CJK_VERTICAL_ROMAN_PLACEMENT_TYPE:
    hr_aat_layout_feature_type_t = 31u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_ITALIC_CJK_ROMAN: hr_aat_layout_feature_type_t = 32u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_CASE_SENSITIVE_LAYOUT: hr_aat_layout_feature_type_t = 33u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_ALTERNATE_KANA: hr_aat_layout_feature_type_t = 34u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_STYLISTIC_ALTERNATIVES: hr_aat_layout_feature_type_t = 35u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_CONTEXTUAL_ALTERNATIVES: hr_aat_layout_feature_type_t = 36u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_LOWER_CASE: hr_aat_layout_feature_type_t = 37u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_UPPER_CASE: hr_aat_layout_feature_type_t = 38u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_LANGUAGE_TAG_TYPE: hr_aat_layout_feature_type_t = 39u32;
pub const HR_AAT_LAYOUT_FEATURE_TYPE_CJK_ROMAN_SPACING_TYPE: hr_aat_layout_feature_type_t = 103u32;

pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_INVALID: hr_aat_layout_feature_selector_t = 0xFFFFu32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ALL_TYPE_FEATURES_ON: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ALL_TYPE_FEATURES_OFF: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_REQUIRED_LIGATURES_ON: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_REQUIRED_LIGATURES_OFF: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_COMMON_LIGATURES_ON: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_COMMON_LIGATURES_OFF: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_RARE_LIGATURES_ON: hr_aat_layout_feature_selector_t = 4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_RARE_LIGATURES_OFF: hr_aat_layout_feature_selector_t =
    5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LOGOS_ON: hr_aat_layout_feature_selector_t = 6u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LOGOS_OFF: hr_aat_layout_feature_selector_t = 7u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_REBUS_PICTURES_ON: hr_aat_layout_feature_selector_t = 8u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_REBUS_PICTURES_OFF: hr_aat_layout_feature_selector_t =
    9u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DIPHTHONG_LIGATURES_ON: hr_aat_layout_feature_selector_t =
    10u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DIPHTHONG_LIGATURES_OFF: hr_aat_layout_feature_selector_t =
    11u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SQUARED_LIGATURES_ON: hr_aat_layout_feature_selector_t =
    12u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SQUARED_LIGATURES_OFF: hr_aat_layout_feature_selector_t =
    13u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ABBREV_SQUARED_LIGATURES_ON:
    hr_aat_layout_feature_selector_t = 14u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ABBREV_SQUARED_LIGATURES_OFF:
    hr_aat_layout_feature_selector_t = 15u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SYMBOL_LIGATURES_ON: hr_aat_layout_feature_selector_t =
    16u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SYMBOL_LIGATURES_OFF: hr_aat_layout_feature_selector_t =
    17u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CONTEXTUAL_LIGATURES_ON: hr_aat_layout_feature_selector_t =
    18u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CONTEXTUAL_LIGATURES_OFF:
    hr_aat_layout_feature_selector_t = 19u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HISTORICAL_LIGATURES_ON: hr_aat_layout_feature_selector_t =
    20u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HISTORICAL_LIGATURES_OFF:
    hr_aat_layout_feature_selector_t = 21u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_UNCONNECTED: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PARTIALLY_CONNECTED: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CURSIVE: hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_UPPER_AND_LOWER_CASE: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ALL_CAPS: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ALL_LOWER_CASE: hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SMALL_CAPS: hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_INITIAL_CAPS: hr_aat_layout_feature_selector_t = 4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_INITIAL_CAPS_AND_SMALL_CAPS:
    hr_aat_layout_feature_selector_t = 5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SUBSTITUTE_VERTICAL_FORMS_ON:
    hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SUBSTITUTE_VERTICAL_FORMS_OFF:
    hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LINGUISTIC_REARRANGEMENT_ON:
    hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LINGUISTIC_REARRANGEMENT_OFF:
    hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_MONOSPACED_NUMBERS: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PROPORTIONAL_NUMBERS: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_THIRD_WIDTH_NUMBERS: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_QUARTER_WIDTH_NUMBERS: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_WORD_INITIAL_SWASHES_ON: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_WORD_INITIAL_SWASHES_OFF:
    hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_WORD_FINAL_SWASHES_ON: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_WORD_FINAL_SWASHES_OFF: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LINE_INITIAL_SWASHES_ON: hr_aat_layout_feature_selector_t =
    4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LINE_INITIAL_SWASHES_OFF:
    hr_aat_layout_feature_selector_t = 5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LINE_FINAL_SWASHES_ON: hr_aat_layout_feature_selector_t =
    6u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LINE_FINAL_SWASHES_OFF: hr_aat_layout_feature_selector_t =
    7u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NON_FINAL_SWASHES_ON: hr_aat_layout_feature_selector_t =
    8u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NON_FINAL_SWASHES_OFF: hr_aat_layout_feature_selector_t =
    9u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SHOW_DIACRITICS: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HIDE_DIACRITICS: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DECOMPOSE_DIACRITICS: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NORMAL_POSITION: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SUPERIORS: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_INFERIORS: hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ORDINALS: hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SCIENTIFIC_INFERIORS: hr_aat_layout_feature_selector_t =
    4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_FRACTIONS: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_VERTICAL_FRACTIONS: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DIAGONAL_FRACTIONS: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PREVENT_OVERLAP_ON: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PREVENT_OVERLAP_OFF: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HYPHENS_TO_EM_DASH_ON: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HYPHENS_TO_EM_DASH_OFF: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HYPHEN_TO_EN_DASH_ON: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HYPHEN_TO_EN_DASH_OFF: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SLASHED_ZERO_ON: hr_aat_layout_feature_selector_t = 4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SLASHED_ZERO_OFF: hr_aat_layout_feature_selector_t = 5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_FORM_INTERROBANG_ON: hr_aat_layout_feature_selector_t =
    6u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_FORM_INTERROBANG_OFF: hr_aat_layout_feature_selector_t =
    7u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SMART_QUOTES_ON: hr_aat_layout_feature_selector_t = 8u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SMART_QUOTES_OFF: hr_aat_layout_feature_selector_t = 9u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PERIODS_TO_ELLIPSIS_ON: hr_aat_layout_feature_selector_t =
    10u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PERIODS_TO_ELLIPSIS_OFF: hr_aat_layout_feature_selector_t =
    11u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HYPHEN_TO_MINUS_ON: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HYPHEN_TO_MINUS_OFF: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ASTERISK_TO_MULTIPLY_ON: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ASTERISK_TO_MULTIPLY_OFF:
    hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SLASH_TO_DIVIDE_ON: hr_aat_layout_feature_selector_t =
    4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SLASH_TO_DIVIDE_OFF: hr_aat_layout_feature_selector_t =
    5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_INEQUALITY_LIGATURES_ON: hr_aat_layout_feature_selector_t =
    6u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_INEQUALITY_LIGATURES_OFF:
    hr_aat_layout_feature_selector_t = 7u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_EXPONENTS_ON: hr_aat_layout_feature_selector_t = 8u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_EXPONENTS_OFF: hr_aat_layout_feature_selector_t = 9u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_MATHEMATICAL_GREEK_ON: hr_aat_layout_feature_selector_t =
    10u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_MATHEMATICAL_GREEK_OFF: hr_aat_layout_feature_selector_t =
    11u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_ORNAMENTS: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DINGBATS: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PI_CHARACTERS: hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_FLEURONS: hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DECORATIVE_BORDERS: hr_aat_layout_feature_selector_t =
    4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_INTERNATIONAL_SYMBOLS: hr_aat_layout_feature_selector_t =
    5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_MATH_SYMBOLS: hr_aat_layout_feature_selector_t = 6u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_ALTERNATES: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DESIGN_LEVEL1: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DESIGN_LEVEL2: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DESIGN_LEVEL3: hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DESIGN_LEVEL4: hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DESIGN_LEVEL5: hr_aat_layout_feature_selector_t = 4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_STYLE_OPTIONS: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DISPLAY_TEXT: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ENGRAVED_TEXT: hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ILLUMINATED_CAPS: hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TITLING_CAPS: hr_aat_layout_feature_selector_t = 4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TALL_CAPS: hr_aat_layout_feature_selector_t = 5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TRADITIONAL_CHARACTERS: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SIMPLIFIED_CHARACTERS: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_JIS1978_CHARACTERS: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_JIS1983_CHARACTERS: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_JIS1990_CHARACTERS: hr_aat_layout_feature_selector_t =
    4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TRADITIONAL_ALT_ONE: hr_aat_layout_feature_selector_t =
    5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TRADITIONAL_ALT_TWO: hr_aat_layout_feature_selector_t =
    6u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TRADITIONAL_ALT_THREE: hr_aat_layout_feature_selector_t =
    7u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TRADITIONAL_ALT_FOUR: hr_aat_layout_feature_selector_t =
    8u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TRADITIONAL_ALT_FIVE: hr_aat_layout_feature_selector_t =
    9u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_EXPERT_CHARACTERS: hr_aat_layout_feature_selector_t =
    10u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_JIS2004_CHARACTERS: hr_aat_layout_feature_selector_t =
    11u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HOJO_CHARACTERS: hr_aat_layout_feature_selector_t = 12u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NLCCHARACTERS: hr_aat_layout_feature_selector_t = 13u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TRADITIONAL_NAMES_CHARACTERS:
    hr_aat_layout_feature_selector_t = 14u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LOWER_CASE_NUMBERS: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_UPPER_CASE_NUMBERS: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PROPORTIONAL_TEXT: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_MONOSPACED_TEXT: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HALF_WIDTH_TEXT: hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_THIRD_WIDTH_TEXT: hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_QUARTER_WIDTH_TEXT: hr_aat_layout_feature_selector_t =
    4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ALT_PROPORTIONAL_TEXT: hr_aat_layout_feature_selector_t =
    5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ALT_HALF_WIDTH_TEXT: hr_aat_layout_feature_selector_t =
    6u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_TRANSLITERATION: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HANJA_TO_HANGUL: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HIRAGANA_TO_KATAKANA: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_KATAKANA_TO_HIRAGANA: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_KANA_TO_ROMANIZATION: hr_aat_layout_feature_selector_t =
    4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ROMANIZATION_TO_HIRAGANA:
    hr_aat_layout_feature_selector_t = 5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ROMANIZATION_TO_KATAKANA:
    hr_aat_layout_feature_selector_t = 6u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HANJA_TO_HANGUL_ALT_ONE: hr_aat_layout_feature_selector_t =
    7u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HANJA_TO_HANGUL_ALT_TWO: hr_aat_layout_feature_selector_t =
    8u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HANJA_TO_HANGUL_ALT_THREE:
    hr_aat_layout_feature_selector_t = 9u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_ANNOTATION: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_BOX_ANNOTATION: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ROUNDED_BOX_ANNOTATION: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CIRCLE_ANNOTATION: hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_INVERTED_CIRCLE_ANNOTATION:
    hr_aat_layout_feature_selector_t = 4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PARENTHESIS_ANNOTATION: hr_aat_layout_feature_selector_t =
    5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PERIOD_ANNOTATION: hr_aat_layout_feature_selector_t = 6u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ROMAN_NUMERAL_ANNOTATION:
    hr_aat_layout_feature_selector_t = 7u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DIAMOND_ANNOTATION: hr_aat_layout_feature_selector_t =
    8u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_INVERTED_BOX_ANNOTATION: hr_aat_layout_feature_selector_t =
    9u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_INVERTED_ROUNDED_BOX_ANNOTATION:
    hr_aat_layout_feature_selector_t = 10u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_FULL_WIDTH_KANA: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PROPORTIONAL_KANA: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_FULL_WIDTH_IDEOGRAPHS: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PROPORTIONAL_IDEOGRAPHS: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HALF_WIDTH_IDEOGRAPHS: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CANONICAL_COMPOSITION_ON:
    hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CANONICAL_COMPOSITION_OFF:
    hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_COMPATIBILITY_COMPOSITION_ON:
    hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_COMPATIBILITY_COMPOSITION_OFF:
    hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TRANSCODING_COMPOSITION_ON:
    hr_aat_layout_feature_selector_t = 4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_TRANSCODING_COMPOSITION_OFF:
    hr_aat_layout_feature_selector_t = 5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_RUBY_KANA: hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_RUBY_KANA: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_RUBY_KANA_ON: hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_RUBY_KANA_OFF: hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_CJK_SYMBOL_ALTERNATIVES:
    hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CJK_SYMBOL_ALT_ONE: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CJK_SYMBOL_ALT_TWO: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CJK_SYMBOL_ALT_THREE: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CJK_SYMBOL_ALT_FOUR: hr_aat_layout_feature_selector_t =
    4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CJK_SYMBOL_ALT_FIVE: hr_aat_layout_feature_selector_t =
    5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_IDEOGRAPHIC_ALTERNATIVES:
    hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_IDEOGRAPHIC_ALT_ONE: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_IDEOGRAPHIC_ALT_TWO: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_IDEOGRAPHIC_ALT_THREE: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_IDEOGRAPHIC_ALT_FOUR: hr_aat_layout_feature_selector_t =
    4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_IDEOGRAPHIC_ALT_FIVE: hr_aat_layout_feature_selector_t =
    5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CJK_VERTICAL_ROMAN_CENTERED:
    hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CJK_VERTICAL_ROMAN_HBASELINE:
    hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_CJK_ITALIC_ROMAN: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CJK_ITALIC_ROMAN: hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CJK_ITALIC_ROMAN_ON: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CJK_ITALIC_ROMAN_OFF: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CASE_SENSITIVE_LAYOUT_ON:
    hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CASE_SENSITIVE_LAYOUT_OFF:
    hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CASE_SENSITIVE_SPACING_ON:
    hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CASE_SENSITIVE_SPACING_OFF:
    hr_aat_layout_feature_selector_t = 3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ALTERNATE_HORIZ_KANA_ON: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ALTERNATE_HORIZ_KANA_OFF:
    hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ALTERNATE_VERT_KANA_ON: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_ALTERNATE_VERT_KANA_OFF: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_NO_STYLISTIC_ALTERNATES: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_ONE_ON: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_ONE_OFF: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_TWO_ON: hr_aat_layout_feature_selector_t =
    4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_TWO_OFF: hr_aat_layout_feature_selector_t =
    5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_THREE_ON: hr_aat_layout_feature_selector_t =
    6u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_THREE_OFF: hr_aat_layout_feature_selector_t =
    7u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_FOUR_ON: hr_aat_layout_feature_selector_t =
    8u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_FOUR_OFF: hr_aat_layout_feature_selector_t =
    9u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_FIVE_ON: hr_aat_layout_feature_selector_t =
    10u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_FIVE_OFF: hr_aat_layout_feature_selector_t =
    11u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_SIX_ON: hr_aat_layout_feature_selector_t =
    12u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_SIX_OFF: hr_aat_layout_feature_selector_t =
    13u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_SEVEN_ON: hr_aat_layout_feature_selector_t =
    14u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_SEVEN_OFF: hr_aat_layout_feature_selector_t =
    15u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_EIGHT_ON: hr_aat_layout_feature_selector_t =
    16u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_EIGHT_OFF: hr_aat_layout_feature_selector_t =
    17u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_NINE_ON: hr_aat_layout_feature_selector_t =
    18u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_NINE_OFF: hr_aat_layout_feature_selector_t =
    19u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_TEN_ON: hr_aat_layout_feature_selector_t =
    20u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_TEN_OFF: hr_aat_layout_feature_selector_t =
    21u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_ELEVEN_ON: hr_aat_layout_feature_selector_t =
    22u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_ELEVEN_OFF:
    hr_aat_layout_feature_selector_t = 23u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_TWELVE_ON: hr_aat_layout_feature_selector_t =
    24u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_TWELVE_OFF:
    hr_aat_layout_feature_selector_t = 25u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_THIRTEEN_ON:
    hr_aat_layout_feature_selector_t = 26u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_THIRTEEN_OFF:
    hr_aat_layout_feature_selector_t = 27u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_FOURTEEN_ON:
    hr_aat_layout_feature_selector_t = 28u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_FOURTEEN_OFF:
    hr_aat_layout_feature_selector_t = 29u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_FIFTEEN_ON:
    hr_aat_layout_feature_selector_t = 30u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_FIFTEEN_OFF:
    hr_aat_layout_feature_selector_t = 31u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_SIXTEEN_ON:
    hr_aat_layout_feature_selector_t = 32u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_SIXTEEN_OFF:
    hr_aat_layout_feature_selector_t = 33u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_SEVENTEEN_ON:
    hr_aat_layout_feature_selector_t = 34u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_SEVENTEEN_OFF:
    hr_aat_layout_feature_selector_t = 35u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_EIGHTEEN_ON:
    hr_aat_layout_feature_selector_t = 36u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_EIGHTEEN_OFF:
    hr_aat_layout_feature_selector_t = 37u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_NINETEEN_ON:
    hr_aat_layout_feature_selector_t = 38u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_NINETEEN_OFF:
    hr_aat_layout_feature_selector_t = 39u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_TWENTY_ON: hr_aat_layout_feature_selector_t =
    40u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_STYLISTIC_ALT_TWENTY_OFF:
    hr_aat_layout_feature_selector_t = 41u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CONTEXTUAL_ALTERNATES_ON:
    hr_aat_layout_feature_selector_t = 0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CONTEXTUAL_ALTERNATES_OFF:
    hr_aat_layout_feature_selector_t = 1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SWASH_ALTERNATES_ON: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_SWASH_ALTERNATES_OFF: hr_aat_layout_feature_selector_t =
    3u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CONTEXTUAL_SWASH_ALTERNATES_ON:
    hr_aat_layout_feature_selector_t = 4u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_CONTEXTUAL_SWASH_ALTERNATES_OFF:
    hr_aat_layout_feature_selector_t = 5u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DEFAULT_LOWER_CASE: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LOWER_CASE_SMALL_CAPS: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_LOWER_CASE_PETITE_CAPS: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DEFAULT_UPPER_CASE: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_UPPER_CASE_SMALL_CAPS: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_UPPER_CASE_PETITE_CAPS: hr_aat_layout_feature_selector_t =
    2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_HALF_WIDTH_CJK_ROMAN: hr_aat_layout_feature_selector_t =
    0u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_PROPORTIONAL_CJK_ROMAN: hr_aat_layout_feature_selector_t =
    1u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_DEFAULT_CJK_ROMAN: hr_aat_layout_feature_selector_t = 2u32;
pub const HR_AAT_LAYOUT_FEATURE_SELECTOR_FULL_WIDTH_CJK_ROMAN: hr_aat_layout_feature_selector_t =
    3u32;

/// A named selector and the values that enable and disable it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct hr_aat_layout_feature_selector_info_t {
    pub name_id: hr_ot_name_id_t,
    pub enable: hr_aat_layout_feature_selector_t,
    pub disable: hr_aat_layout_feature_selector_t,
    /// Reserved; always zero.
    pub reserved: c_uint,
}

fn feat(face: &hr_face_t) -> Option<Feat<'_>> {
    let table = face.font()?.tables().feat().ok()?;
    (table.version().major == 1 && *face.feat_sane.get_or_init(|| settings_bounded(&table)))
        .then_some(table)
}

/// Whether reading the selectors of every listed feature type reads the table
/// at most 64 times over, the limit HarfBuzz's sanitizer puts on it.
///
/// Setting arrays may overlap each other and the feature records, and a
/// repeated feature type finds the same array each time it is listed.
fn settings_bounded(table: &Feat) -> bool {
    let limit = table.offset_data().len().saturating_mul(64).max(16384);
    let read = table
        .names()
        .iter()
        .filter_map(|record| table.find(record.feature()))
        .map(|feature| usize::from(feature.n_settings()) * SettingName::RAW_BYTE_LEN)
        .fold(0, usize::saturating_add);
    read <= limit
}

/// Returns the total number of feature types in the face's AAT feat table.
///
/// When both output pointers are non-null, `feature_count` gives capacity on
/// entry and the number written on return. A null array leaves the count
/// unchanged. Results preserve the order in the table.
/// # Safety
/// `face` must be null or live; `feature_count` must be null or writable;
/// `features` must hold the input capacity when non-null.
#[no_mangle]
pub unsafe extern "C" fn hr_aat_layout_get_feature_types(
    face: *mut hr_face_t,
    start_offset: c_uint,
    feature_count: *mut c_uint,
    features: *mut hr_aat_layout_feature_type_t,
) -> c_uint {
    let face = unsafe { object::or_empty(face.cast_const()) };
    let table = feat(face);
    let records = table.as_ref().map_or(&[][..], |table| table.names());
    if !features.is_null() {
        if let Some(count) = unsafe { feature_count.as_mut() } {
            let mut written = 0;
            for record in records
                .iter()
                .skip(start_offset as usize)
                .take(*count as usize)
            {
                unsafe { features.add(written).write(record.feature() as c_uint) };
                written += 1;
            }
            *count = written as c_uint;
        }
    }
    records.len() as c_uint
}

/// Returns the total number of selectors for an AAT feature and copies a page.
///
/// For exclusive features, `default_index` is relative to the entire selector
/// array and each disable value is the default selector's value. Otherwise
/// the index is `HR_AAT_LAYOUT_NO_SELECTOR_INDEX` and each disable value is
/// its enable value plus one. A null selector array leaves the count unchanged.
/// # Safety
/// `face` must be null or live; `selector_count` and `default_index` must be
/// null or writable; `selectors` must hold the input capacity when non-null.
#[no_mangle]
pub unsafe extern "C" fn hr_aat_layout_feature_type_get_selector_infos(
    face: *mut hr_face_t,
    feature_type: hr_aat_layout_feature_type_t,
    start_offset: c_uint,
    selector_count: *mut c_uint,
    selectors: *mut hr_aat_layout_feature_selector_info_t,
    default_index: *mut c_uint,
) -> c_uint {
    let face = unsafe { object::or_empty(face.cast_const()) };
    let feature = feat(face).and_then(|table| {
        let feature = table.find(u16::try_from(feature_type).ok()?)?;
        let settings = feature.setting_table(table.offset_data()).ok()?;
        Some((feature, settings))
    });
    let (index, disable) = feature
        .as_ref()
        .filter(|(feature, _)| feature.is_exclusive())
        .map_or(
            (HR_AAT_LAYOUT_NO_SELECTOR_INDEX, None),
            |(feature, settings)| {
                let index = feature.default_setting_index() as c_uint;
                let disable = settings
                    .settings()
                    .get(index as usize)
                    .map_or(0, |setting| setting.setting() as c_uint);
                (index, Some(disable))
            },
        );
    if let Some(output) = unsafe { default_index.as_mut() } {
        *output = index;
    }
    let records = feature
        .as_ref()
        .map_or(&[][..], |(_, settings)| settings.settings());
    if !selectors.is_null() {
        if let Some(count) = unsafe { selector_count.as_mut() } {
            let mut written = 0;
            for setting in records
                .iter()
                .skip(start_offset as usize)
                .take(*count as usize)
            {
                let enable = setting.setting() as c_uint;
                let info = hr_aat_layout_feature_selector_info_t {
                    name_id: setting.name_index().to_u16() as c_uint,
                    enable,
                    disable: disable.unwrap_or(enable + 1),
                    reserved: 0,
                };
                unsafe { selectors.add(written).write(info) };
                written += 1;
            }
            *count = written as c_uint;
        }
    }
    records.len() as c_uint
}
