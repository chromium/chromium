#ifndef ICU4X_CodePointSetData_HPP
#define ICU4X_CodePointSetData_HPP

#include "CodePointSetData.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CodePointRangeIterator.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "GeneralCategoryGroup.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    bool icu4x_CodePointSetData_contains_mv1(const icu4x::capi::CodePointSetData* self, char32_t cp);

    icu4x::capi::CodePointRangeIterator* icu4x_CodePointSetData_iter_ranges_mv1(const icu4x::capi::CodePointSetData* self);

    icu4x::capi::CodePointRangeIterator* icu4x_CodePointSetData_iter_ranges_complemented_mv1(const icu4x::capi::CodePointSetData* self);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_general_category_group_mv1(icu4x::capi::GeneralCategoryGroup group);

    typedef struct icu4x_CodePointSetData_create_general_category_group_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_general_category_group_with_provider_mv1_result;
    icu4x_CodePointSetData_create_general_category_group_with_provider_mv1_result icu4x_CodePointSetData_create_general_category_group_with_provider_mv1(const icu4x::capi::DataProvider* provider, uint32_t group);

    bool icu4x_CodePointSetData_ascii_hex_digit_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_ascii_hex_digit_mv1(void);

    typedef struct icu4x_CodePointSetData_create_ascii_hex_digit_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_ascii_hex_digit_with_provider_mv1_result;
    icu4x_CodePointSetData_create_ascii_hex_digit_with_provider_mv1_result icu4x_CodePointSetData_create_ascii_hex_digit_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_alnum_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_alnum_mv1(void);

    typedef struct icu4x_CodePointSetData_create_alnum_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_alnum_with_provider_mv1_result;
    icu4x_CodePointSetData_create_alnum_with_provider_mv1_result icu4x_CodePointSetData_create_alnum_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_alphabetic_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_alphabetic_mv1(void);

    typedef struct icu4x_CodePointSetData_create_alphabetic_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_alphabetic_with_provider_mv1_result;
    icu4x_CodePointSetData_create_alphabetic_with_provider_mv1_result icu4x_CodePointSetData_create_alphabetic_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_bidi_control_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_bidi_control_mv1(void);

    typedef struct icu4x_CodePointSetData_create_bidi_control_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_bidi_control_with_provider_mv1_result;
    icu4x_CodePointSetData_create_bidi_control_with_provider_mv1_result icu4x_CodePointSetData_create_bidi_control_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_bidi_mirrored_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_bidi_mirrored_mv1(void);

    typedef struct icu4x_CodePointSetData_create_bidi_mirrored_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_bidi_mirrored_with_provider_mv1_result;
    icu4x_CodePointSetData_create_bidi_mirrored_with_provider_mv1_result icu4x_CodePointSetData_create_bidi_mirrored_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_blank_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_blank_mv1(void);

    typedef struct icu4x_CodePointSetData_create_blank_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_blank_with_provider_mv1_result;
    icu4x_CodePointSetData_create_blank_with_provider_mv1_result icu4x_CodePointSetData_create_blank_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_cased_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_cased_mv1(void);

    typedef struct icu4x_CodePointSetData_create_cased_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_cased_with_provider_mv1_result;
    icu4x_CodePointSetData_create_cased_with_provider_mv1_result icu4x_CodePointSetData_create_cased_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_case_ignorable_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_case_ignorable_mv1(void);

    typedef struct icu4x_CodePointSetData_create_case_ignorable_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_case_ignorable_with_provider_mv1_result;
    icu4x_CodePointSetData_create_case_ignorable_with_provider_mv1_result icu4x_CodePointSetData_create_case_ignorable_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_full_composition_exclusion_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_full_composition_exclusion_mv1(void);

    typedef struct icu4x_CodePointSetData_create_full_composition_exclusion_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_full_composition_exclusion_with_provider_mv1_result;
    icu4x_CodePointSetData_create_full_composition_exclusion_with_provider_mv1_result icu4x_CodePointSetData_create_full_composition_exclusion_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_changes_when_casefolded_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_changes_when_casefolded_mv1(void);

    typedef struct icu4x_CodePointSetData_create_changes_when_casefolded_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_changes_when_casefolded_with_provider_mv1_result;
    icu4x_CodePointSetData_create_changes_when_casefolded_with_provider_mv1_result icu4x_CodePointSetData_create_changes_when_casefolded_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_changes_when_casemapped_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_changes_when_casemapped_mv1(void);

    typedef struct icu4x_CodePointSetData_create_changes_when_casemapped_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_changes_when_casemapped_with_provider_mv1_result;
    icu4x_CodePointSetData_create_changes_when_casemapped_with_provider_mv1_result icu4x_CodePointSetData_create_changes_when_casemapped_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_changes_when_nfkc_casefolded_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_changes_when_nfkc_casefolded_mv1(void);

    typedef struct icu4x_CodePointSetData_create_changes_when_nfkc_casefolded_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_changes_when_nfkc_casefolded_with_provider_mv1_result;
    icu4x_CodePointSetData_create_changes_when_nfkc_casefolded_with_provider_mv1_result icu4x_CodePointSetData_create_changes_when_nfkc_casefolded_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_changes_when_lowercased_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_changes_when_lowercased_mv1(void);

    typedef struct icu4x_CodePointSetData_create_changes_when_lowercased_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_changes_when_lowercased_with_provider_mv1_result;
    icu4x_CodePointSetData_create_changes_when_lowercased_with_provider_mv1_result icu4x_CodePointSetData_create_changes_when_lowercased_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_changes_when_titlecased_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_changes_when_titlecased_mv1(void);

    typedef struct icu4x_CodePointSetData_create_changes_when_titlecased_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_changes_when_titlecased_with_provider_mv1_result;
    icu4x_CodePointSetData_create_changes_when_titlecased_with_provider_mv1_result icu4x_CodePointSetData_create_changes_when_titlecased_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_changes_when_uppercased_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_changes_when_uppercased_mv1(void);

    typedef struct icu4x_CodePointSetData_create_changes_when_uppercased_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_changes_when_uppercased_with_provider_mv1_result;
    icu4x_CodePointSetData_create_changes_when_uppercased_with_provider_mv1_result icu4x_CodePointSetData_create_changes_when_uppercased_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_dash_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_dash_mv1(void);

    typedef struct icu4x_CodePointSetData_create_dash_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_dash_with_provider_mv1_result;
    icu4x_CodePointSetData_create_dash_with_provider_mv1_result icu4x_CodePointSetData_create_dash_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_deprecated_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_deprecated_mv1(void);

    typedef struct icu4x_CodePointSetData_create_deprecated_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_deprecated_with_provider_mv1_result;
    icu4x_CodePointSetData_create_deprecated_with_provider_mv1_result icu4x_CodePointSetData_create_deprecated_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_default_ignorable_code_point_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_default_ignorable_code_point_mv1(void);

    typedef struct icu4x_CodePointSetData_create_default_ignorable_code_point_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_default_ignorable_code_point_with_provider_mv1_result;
    icu4x_CodePointSetData_create_default_ignorable_code_point_with_provider_mv1_result icu4x_CodePointSetData_create_default_ignorable_code_point_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_diacritic_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_diacritic_mv1(void);

    typedef struct icu4x_CodePointSetData_create_diacritic_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_diacritic_with_provider_mv1_result;
    icu4x_CodePointSetData_create_diacritic_with_provider_mv1_result icu4x_CodePointSetData_create_diacritic_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_emoji_modifier_base_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_emoji_modifier_base_mv1(void);

    typedef struct icu4x_CodePointSetData_create_emoji_modifier_base_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_emoji_modifier_base_with_provider_mv1_result;
    icu4x_CodePointSetData_create_emoji_modifier_base_with_provider_mv1_result icu4x_CodePointSetData_create_emoji_modifier_base_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_emoji_component_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_emoji_component_mv1(void);

    typedef struct icu4x_CodePointSetData_create_emoji_component_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_emoji_component_with_provider_mv1_result;
    icu4x_CodePointSetData_create_emoji_component_with_provider_mv1_result icu4x_CodePointSetData_create_emoji_component_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_emoji_modifier_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_emoji_modifier_mv1(void);

    typedef struct icu4x_CodePointSetData_create_emoji_modifier_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_emoji_modifier_with_provider_mv1_result;
    icu4x_CodePointSetData_create_emoji_modifier_with_provider_mv1_result icu4x_CodePointSetData_create_emoji_modifier_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_emoji_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_emoji_mv1(void);

    typedef struct icu4x_CodePointSetData_create_emoji_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_emoji_with_provider_mv1_result;
    icu4x_CodePointSetData_create_emoji_with_provider_mv1_result icu4x_CodePointSetData_create_emoji_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_emoji_presentation_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_emoji_presentation_mv1(void);

    typedef struct icu4x_CodePointSetData_create_emoji_presentation_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_emoji_presentation_with_provider_mv1_result;
    icu4x_CodePointSetData_create_emoji_presentation_with_provider_mv1_result icu4x_CodePointSetData_create_emoji_presentation_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_extender_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_extender_mv1(void);

    typedef struct icu4x_CodePointSetData_create_extender_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_extender_with_provider_mv1_result;
    icu4x_CodePointSetData_create_extender_with_provider_mv1_result icu4x_CodePointSetData_create_extender_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_extended_pictographic_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_extended_pictographic_mv1(void);

    typedef struct icu4x_CodePointSetData_create_extended_pictographic_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_extended_pictographic_with_provider_mv1_result;
    icu4x_CodePointSetData_create_extended_pictographic_with_provider_mv1_result icu4x_CodePointSetData_create_extended_pictographic_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_graph_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_graph_mv1(void);

    typedef struct icu4x_CodePointSetData_create_graph_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_graph_with_provider_mv1_result;
    icu4x_CodePointSetData_create_graph_with_provider_mv1_result icu4x_CodePointSetData_create_graph_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_grapheme_base_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_grapheme_base_mv1(void);

    typedef struct icu4x_CodePointSetData_create_grapheme_base_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_grapheme_base_with_provider_mv1_result;
    icu4x_CodePointSetData_create_grapheme_base_with_provider_mv1_result icu4x_CodePointSetData_create_grapheme_base_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_grapheme_extend_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_grapheme_extend_mv1(void);

    typedef struct icu4x_CodePointSetData_create_grapheme_extend_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_grapheme_extend_with_provider_mv1_result;
    icu4x_CodePointSetData_create_grapheme_extend_with_provider_mv1_result icu4x_CodePointSetData_create_grapheme_extend_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_grapheme_link_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_grapheme_link_mv1(void);

    typedef struct icu4x_CodePointSetData_create_grapheme_link_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_grapheme_link_with_provider_mv1_result;
    icu4x_CodePointSetData_create_grapheme_link_with_provider_mv1_result icu4x_CodePointSetData_create_grapheme_link_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_hex_digit_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_hex_digit_mv1(void);

    typedef struct icu4x_CodePointSetData_create_hex_digit_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_hex_digit_with_provider_mv1_result;
    icu4x_CodePointSetData_create_hex_digit_with_provider_mv1_result icu4x_CodePointSetData_create_hex_digit_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_hyphen_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_hyphen_mv1(void);

    typedef struct icu4x_CodePointSetData_create_hyphen_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_hyphen_with_provider_mv1_result;
    icu4x_CodePointSetData_create_hyphen_with_provider_mv1_result icu4x_CodePointSetData_create_hyphen_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_id_compat_math_continue_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_id_compat_math_continue_mv1(void);

    typedef struct icu4x_CodePointSetData_create_id_compat_math_continue_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_id_compat_math_continue_with_provider_mv1_result;
    icu4x_CodePointSetData_create_id_compat_math_continue_with_provider_mv1_result icu4x_CodePointSetData_create_id_compat_math_continue_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_id_compat_math_start_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_id_compat_math_start_mv1(void);

    typedef struct icu4x_CodePointSetData_create_id_compat_math_start_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_id_compat_math_start_with_provider_mv1_result;
    icu4x_CodePointSetData_create_id_compat_math_start_with_provider_mv1_result icu4x_CodePointSetData_create_id_compat_math_start_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_id_continue_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_id_continue_mv1(void);

    typedef struct icu4x_CodePointSetData_create_id_continue_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_id_continue_with_provider_mv1_result;
    icu4x_CodePointSetData_create_id_continue_with_provider_mv1_result icu4x_CodePointSetData_create_id_continue_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_ideographic_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_ideographic_mv1(void);

    typedef struct icu4x_CodePointSetData_create_ideographic_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_ideographic_with_provider_mv1_result;
    icu4x_CodePointSetData_create_ideographic_with_provider_mv1_result icu4x_CodePointSetData_create_ideographic_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_id_start_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_id_start_mv1(void);

    typedef struct icu4x_CodePointSetData_create_id_start_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_id_start_with_provider_mv1_result;
    icu4x_CodePointSetData_create_id_start_with_provider_mv1_result icu4x_CodePointSetData_create_id_start_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_ids_binary_operator_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_ids_binary_operator_mv1(void);

    typedef struct icu4x_CodePointSetData_create_ids_binary_operator_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_ids_binary_operator_with_provider_mv1_result;
    icu4x_CodePointSetData_create_ids_binary_operator_with_provider_mv1_result icu4x_CodePointSetData_create_ids_binary_operator_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_ids_trinary_operator_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_ids_trinary_operator_mv1(void);

    typedef struct icu4x_CodePointSetData_create_ids_trinary_operator_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_ids_trinary_operator_with_provider_mv1_result;
    icu4x_CodePointSetData_create_ids_trinary_operator_with_provider_mv1_result icu4x_CodePointSetData_create_ids_trinary_operator_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_ids_unary_operator_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_ids_unary_operator_mv1(void);

    typedef struct icu4x_CodePointSetData_create_ids_unary_operator_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_ids_unary_operator_with_provider_mv1_result;
    icu4x_CodePointSetData_create_ids_unary_operator_with_provider_mv1_result icu4x_CodePointSetData_create_ids_unary_operator_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_join_control_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_join_control_mv1(void);

    typedef struct icu4x_CodePointSetData_create_join_control_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_join_control_with_provider_mv1_result;
    icu4x_CodePointSetData_create_join_control_with_provider_mv1_result icu4x_CodePointSetData_create_join_control_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_logical_order_exception_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_logical_order_exception_mv1(void);

    typedef struct icu4x_CodePointSetData_create_logical_order_exception_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_logical_order_exception_with_provider_mv1_result;
    icu4x_CodePointSetData_create_logical_order_exception_with_provider_mv1_result icu4x_CodePointSetData_create_logical_order_exception_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_lowercase_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_lowercase_mv1(void);

    typedef struct icu4x_CodePointSetData_create_lowercase_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_lowercase_with_provider_mv1_result;
    icu4x_CodePointSetData_create_lowercase_with_provider_mv1_result icu4x_CodePointSetData_create_lowercase_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_math_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_math_mv1(void);

    typedef struct icu4x_CodePointSetData_create_math_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_math_with_provider_mv1_result;
    icu4x_CodePointSetData_create_math_with_provider_mv1_result icu4x_CodePointSetData_create_math_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_modifier_combining_mark_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_modifier_combining_mark_mv1(void);

    typedef struct icu4x_CodePointSetData_create_modifier_combining_mark_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_modifier_combining_mark_with_provider_mv1_result;
    icu4x_CodePointSetData_create_modifier_combining_mark_with_provider_mv1_result icu4x_CodePointSetData_create_modifier_combining_mark_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_noncharacter_code_point_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_noncharacter_code_point_mv1(void);

    typedef struct icu4x_CodePointSetData_create_noncharacter_code_point_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_noncharacter_code_point_with_provider_mv1_result;
    icu4x_CodePointSetData_create_noncharacter_code_point_with_provider_mv1_result icu4x_CodePointSetData_create_noncharacter_code_point_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_nfc_inert_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_nfc_inert_mv1(void);

    typedef struct icu4x_CodePointSetData_create_nfc_inert_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_nfc_inert_with_provider_mv1_result;
    icu4x_CodePointSetData_create_nfc_inert_with_provider_mv1_result icu4x_CodePointSetData_create_nfc_inert_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_nfd_inert_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_nfd_inert_mv1(void);

    typedef struct icu4x_CodePointSetData_create_nfd_inert_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_nfd_inert_with_provider_mv1_result;
    icu4x_CodePointSetData_create_nfd_inert_with_provider_mv1_result icu4x_CodePointSetData_create_nfd_inert_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_nfkc_inert_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_nfkc_inert_mv1(void);

    typedef struct icu4x_CodePointSetData_create_nfkc_inert_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_nfkc_inert_with_provider_mv1_result;
    icu4x_CodePointSetData_create_nfkc_inert_with_provider_mv1_result icu4x_CodePointSetData_create_nfkc_inert_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_nfkd_inert_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_nfkd_inert_mv1(void);

    typedef struct icu4x_CodePointSetData_create_nfkd_inert_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_nfkd_inert_with_provider_mv1_result;
    icu4x_CodePointSetData_create_nfkd_inert_with_provider_mv1_result icu4x_CodePointSetData_create_nfkd_inert_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_pattern_syntax_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_pattern_syntax_mv1(void);

    typedef struct icu4x_CodePointSetData_create_pattern_syntax_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_pattern_syntax_with_provider_mv1_result;
    icu4x_CodePointSetData_create_pattern_syntax_with_provider_mv1_result icu4x_CodePointSetData_create_pattern_syntax_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_pattern_white_space_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_pattern_white_space_mv1(void);

    typedef struct icu4x_CodePointSetData_create_pattern_white_space_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_pattern_white_space_with_provider_mv1_result;
    icu4x_CodePointSetData_create_pattern_white_space_with_provider_mv1_result icu4x_CodePointSetData_create_pattern_white_space_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_prepended_concatenation_mark_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_prepended_concatenation_mark_mv1(void);

    typedef struct icu4x_CodePointSetData_create_prepended_concatenation_mark_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_prepended_concatenation_mark_with_provider_mv1_result;
    icu4x_CodePointSetData_create_prepended_concatenation_mark_with_provider_mv1_result icu4x_CodePointSetData_create_prepended_concatenation_mark_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_print_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_print_mv1(void);

    typedef struct icu4x_CodePointSetData_create_print_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_print_with_provider_mv1_result;
    icu4x_CodePointSetData_create_print_with_provider_mv1_result icu4x_CodePointSetData_create_print_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_quotation_mark_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_quotation_mark_mv1(void);

    typedef struct icu4x_CodePointSetData_create_quotation_mark_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_quotation_mark_with_provider_mv1_result;
    icu4x_CodePointSetData_create_quotation_mark_with_provider_mv1_result icu4x_CodePointSetData_create_quotation_mark_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_radical_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_radical_mv1(void);

    typedef struct icu4x_CodePointSetData_create_radical_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_radical_with_provider_mv1_result;
    icu4x_CodePointSetData_create_radical_with_provider_mv1_result icu4x_CodePointSetData_create_radical_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_regional_indicator_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_regional_indicator_mv1(void);

    typedef struct icu4x_CodePointSetData_create_regional_indicator_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_regional_indicator_with_provider_mv1_result;
    icu4x_CodePointSetData_create_regional_indicator_with_provider_mv1_result icu4x_CodePointSetData_create_regional_indicator_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_soft_dotted_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_soft_dotted_mv1(void);

    typedef struct icu4x_CodePointSetData_create_soft_dotted_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_soft_dotted_with_provider_mv1_result;
    icu4x_CodePointSetData_create_soft_dotted_with_provider_mv1_result icu4x_CodePointSetData_create_soft_dotted_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_segment_starter_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_segment_starter_mv1(void);

    typedef struct icu4x_CodePointSetData_create_segment_starter_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_segment_starter_with_provider_mv1_result;
    icu4x_CodePointSetData_create_segment_starter_with_provider_mv1_result icu4x_CodePointSetData_create_segment_starter_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_case_sensitive_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_case_sensitive_mv1(void);

    typedef struct icu4x_CodePointSetData_create_case_sensitive_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_case_sensitive_with_provider_mv1_result;
    icu4x_CodePointSetData_create_case_sensitive_with_provider_mv1_result icu4x_CodePointSetData_create_case_sensitive_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_sentence_terminal_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_sentence_terminal_mv1(void);

    typedef struct icu4x_CodePointSetData_create_sentence_terminal_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_sentence_terminal_with_provider_mv1_result;
    icu4x_CodePointSetData_create_sentence_terminal_with_provider_mv1_result icu4x_CodePointSetData_create_sentence_terminal_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_terminal_punctuation_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_terminal_punctuation_mv1(void);

    typedef struct icu4x_CodePointSetData_create_terminal_punctuation_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_terminal_punctuation_with_provider_mv1_result;
    icu4x_CodePointSetData_create_terminal_punctuation_with_provider_mv1_result icu4x_CodePointSetData_create_terminal_punctuation_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_unified_ideograph_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_unified_ideograph_mv1(void);

    typedef struct icu4x_CodePointSetData_create_unified_ideograph_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_unified_ideograph_with_provider_mv1_result;
    icu4x_CodePointSetData_create_unified_ideograph_with_provider_mv1_result icu4x_CodePointSetData_create_unified_ideograph_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_uppercase_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_uppercase_mv1(void);

    typedef struct icu4x_CodePointSetData_create_uppercase_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_uppercase_with_provider_mv1_result;
    icu4x_CodePointSetData_create_uppercase_with_provider_mv1_result icu4x_CodePointSetData_create_uppercase_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_variation_selector_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_variation_selector_mv1(void);

    typedef struct icu4x_CodePointSetData_create_variation_selector_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_variation_selector_with_provider_mv1_result;
    icu4x_CodePointSetData_create_variation_selector_with_provider_mv1_result icu4x_CodePointSetData_create_variation_selector_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_white_space_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_white_space_mv1(void);

    typedef struct icu4x_CodePointSetData_create_white_space_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_white_space_with_provider_mv1_result;
    icu4x_CodePointSetData_create_white_space_with_provider_mv1_result icu4x_CodePointSetData_create_white_space_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_xdigit_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_xdigit_mv1(void);

    typedef struct icu4x_CodePointSetData_create_xdigit_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_xdigit_with_provider_mv1_result;
    icu4x_CodePointSetData_create_xdigit_with_provider_mv1_result icu4x_CodePointSetData_create_xdigit_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_xid_continue_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_xid_continue_mv1(void);

    typedef struct icu4x_CodePointSetData_create_xid_continue_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_xid_continue_with_provider_mv1_result;
    icu4x_CodePointSetData_create_xid_continue_with_provider_mv1_result icu4x_CodePointSetData_create_xid_continue_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_CodePointSetData_xid_start_for_char_mv1(char32_t ch);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetData_create_xid_start_mv1(void);

    typedef struct icu4x_CodePointSetData_create_xid_start_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_xid_start_with_provider_mv1_result;
    icu4x_CodePointSetData_create_xid_start_with_provider_mv1_result icu4x_CodePointSetData_create_xid_start_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    typedef struct icu4x_CodePointSetData_create_for_ecma262_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_for_ecma262_mv1_result;
    icu4x_CodePointSetData_create_for_ecma262_mv1_result icu4x_CodePointSetData_create_for_ecma262_mv1(icu4x::diplomat::capi::DiplomatStringView property_name);

    typedef struct icu4x_CodePointSetData_create_for_ecma262_with_provider_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_create_for_ecma262_with_provider_mv1_result;
    icu4x_CodePointSetData_create_for_ecma262_with_provider_mv1_result icu4x_CodePointSetData_create_for_ecma262_with_provider_mv1(const icu4x::capi::DataProvider* provider, icu4x::diplomat::capi::DiplomatStringView property_name);

    void icu4x_CodePointSetData_destroy_mv1(CodePointSetData* self);

    } // extern "C"
} // namespace capi
} // namespace

inline bool icu4x::CodePointSetData::contains(char32_t cp) const {
    auto result = icu4x::capi::icu4x_CodePointSetData_contains_mv1(this->AsFFI(),
        cp);
    return result;
}

inline std::unique_ptr<icu4x::CodePointRangeIterator> icu4x::CodePointSetData::iter_ranges() const {
    auto result = icu4x::capi::icu4x_CodePointSetData_iter_ranges_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::CodePointRangeIterator>(icu4x::CodePointRangeIterator::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointRangeIterator> icu4x::CodePointSetData::iter_ranges_complemented() const {
    auto result = icu4x::capi::icu4x_CodePointSetData_iter_ranges_complemented_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::CodePointRangeIterator>(icu4x::CodePointRangeIterator::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_general_category_group(icu4x::GeneralCategoryGroup group) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_general_category_group_mv1(group.AsFFI());
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_general_category_group_with_provider(const icu4x::DataProvider& provider, uint32_t group) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_general_category_group_with_provider_mv1(provider.AsFFI(),
        group);
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::ascii_hex_digit_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_ascii_hex_digit_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_ascii_hex_digit() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_ascii_hex_digit_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_ascii_hex_digit_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_ascii_hex_digit_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::alnum_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_alnum_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_alnum() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_alnum_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_alnum_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_alnum_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::alphabetic_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_alphabetic_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_alphabetic() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_alphabetic_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_alphabetic_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_alphabetic_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::bidi_control_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_bidi_control_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_bidi_control() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_bidi_control_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_bidi_control_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_bidi_control_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::bidi_mirrored_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_bidi_mirrored_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_bidi_mirrored() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_bidi_mirrored_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_bidi_mirrored_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_bidi_mirrored_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::blank_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_blank_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_blank() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_blank_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_blank_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_blank_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::cased_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_cased_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_cased() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_cased_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_cased_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_cased_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::case_ignorable_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_case_ignorable_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_case_ignorable() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_case_ignorable_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_case_ignorable_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_case_ignorable_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::full_composition_exclusion_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_full_composition_exclusion_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_full_composition_exclusion() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_full_composition_exclusion_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_full_composition_exclusion_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_full_composition_exclusion_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::changes_when_casefolded_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_changes_when_casefolded_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_changes_when_casefolded() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_casefolded_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_changes_when_casefolded_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_casefolded_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::changes_when_casemapped_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_changes_when_casemapped_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_changes_when_casemapped() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_casemapped_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_changes_when_casemapped_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_casemapped_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::changes_when_nfkc_casefolded_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_changes_when_nfkc_casefolded_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_changes_when_nfkc_casefolded() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_nfkc_casefolded_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_changes_when_nfkc_casefolded_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_nfkc_casefolded_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::changes_when_lowercased_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_changes_when_lowercased_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_changes_when_lowercased() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_lowercased_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_changes_when_lowercased_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_lowercased_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::changes_when_titlecased_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_changes_when_titlecased_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_changes_when_titlecased() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_titlecased_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_changes_when_titlecased_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_titlecased_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::changes_when_uppercased_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_changes_when_uppercased_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_changes_when_uppercased() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_uppercased_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_changes_when_uppercased_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_changes_when_uppercased_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::dash_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_dash_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_dash() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_dash_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_dash_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_dash_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::deprecated_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_deprecated_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_deprecated() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_deprecated_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_deprecated_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_deprecated_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::default_ignorable_code_point_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_default_ignorable_code_point_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_default_ignorable_code_point() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_default_ignorable_code_point_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_default_ignorable_code_point_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_default_ignorable_code_point_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::diacritic_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_diacritic_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_diacritic() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_diacritic_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_diacritic_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_diacritic_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::emoji_modifier_base_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_emoji_modifier_base_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_emoji_modifier_base() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_emoji_modifier_base_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_emoji_modifier_base_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_emoji_modifier_base_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::emoji_component_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_emoji_component_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_emoji_component() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_emoji_component_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_emoji_component_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_emoji_component_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::emoji_modifier_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_emoji_modifier_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_emoji_modifier() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_emoji_modifier_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_emoji_modifier_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_emoji_modifier_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::emoji_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_emoji_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_emoji() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_emoji_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_emoji_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_emoji_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::emoji_presentation_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_emoji_presentation_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_emoji_presentation() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_emoji_presentation_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_emoji_presentation_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_emoji_presentation_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::extender_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_extender_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_extender() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_extender_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_extender_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_extender_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::extended_pictographic_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_extended_pictographic_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_extended_pictographic() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_extended_pictographic_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_extended_pictographic_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_extended_pictographic_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::graph_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_graph_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_graph() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_graph_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_graph_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_graph_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::grapheme_base_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_grapheme_base_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_grapheme_base() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_grapheme_base_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_grapheme_base_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_grapheme_base_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::grapheme_extend_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_grapheme_extend_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_grapheme_extend() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_grapheme_extend_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_grapheme_extend_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_grapheme_extend_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::grapheme_link_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_grapheme_link_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_grapheme_link() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_grapheme_link_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_grapheme_link_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_grapheme_link_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::hex_digit_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_hex_digit_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_hex_digit() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_hex_digit_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_hex_digit_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_hex_digit_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::hyphen_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_hyphen_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_hyphen() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_hyphen_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_hyphen_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_hyphen_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::id_compat_math_continue_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_id_compat_math_continue_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_id_compat_math_continue() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_id_compat_math_continue_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_id_compat_math_continue_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_id_compat_math_continue_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::id_compat_math_start_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_id_compat_math_start_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_id_compat_math_start() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_id_compat_math_start_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_id_compat_math_start_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_id_compat_math_start_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::id_continue_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_id_continue_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_id_continue() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_id_continue_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_id_continue_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_id_continue_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::ideographic_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_ideographic_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_ideographic() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_ideographic_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_ideographic_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_ideographic_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::id_start_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_id_start_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_id_start() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_id_start_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_id_start_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_id_start_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::ids_binary_operator_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_ids_binary_operator_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_ids_binary_operator() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_ids_binary_operator_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_ids_binary_operator_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_ids_binary_operator_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::ids_trinary_operator_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_ids_trinary_operator_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_ids_trinary_operator() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_ids_trinary_operator_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_ids_trinary_operator_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_ids_trinary_operator_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::ids_unary_operator_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_ids_unary_operator_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_ids_unary_operator() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_ids_unary_operator_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_ids_unary_operator_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_ids_unary_operator_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::join_control_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_join_control_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_join_control() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_join_control_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_join_control_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_join_control_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::logical_order_exception_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_logical_order_exception_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_logical_order_exception() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_logical_order_exception_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_logical_order_exception_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_logical_order_exception_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::lowercase_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_lowercase_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_lowercase() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_lowercase_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_lowercase_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_lowercase_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::math_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_math_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_math() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_math_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_math_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_math_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::modifier_combining_mark_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_modifier_combining_mark_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_modifier_combining_mark() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_modifier_combining_mark_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_modifier_combining_mark_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_modifier_combining_mark_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::noncharacter_code_point_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_noncharacter_code_point_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_noncharacter_code_point() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_noncharacter_code_point_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_noncharacter_code_point_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_noncharacter_code_point_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::nfc_inert_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_nfc_inert_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_nfc_inert() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_nfc_inert_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_nfc_inert_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_nfc_inert_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::nfd_inert_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_nfd_inert_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_nfd_inert() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_nfd_inert_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_nfd_inert_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_nfd_inert_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::nfkc_inert_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_nfkc_inert_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_nfkc_inert() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_nfkc_inert_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_nfkc_inert_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_nfkc_inert_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::nfkd_inert_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_nfkd_inert_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_nfkd_inert() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_nfkd_inert_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_nfkd_inert_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_nfkd_inert_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::pattern_syntax_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_pattern_syntax_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_pattern_syntax() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_pattern_syntax_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_pattern_syntax_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_pattern_syntax_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::pattern_white_space_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_pattern_white_space_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_pattern_white_space() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_pattern_white_space_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_pattern_white_space_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_pattern_white_space_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::prepended_concatenation_mark_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_prepended_concatenation_mark_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_prepended_concatenation_mark() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_prepended_concatenation_mark_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_prepended_concatenation_mark_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_prepended_concatenation_mark_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::print_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_print_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_print() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_print_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_print_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_print_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::quotation_mark_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_quotation_mark_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_quotation_mark() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_quotation_mark_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_quotation_mark_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_quotation_mark_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::radical_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_radical_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_radical() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_radical_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_radical_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_radical_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::regional_indicator_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_regional_indicator_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_regional_indicator() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_regional_indicator_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_regional_indicator_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_regional_indicator_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::soft_dotted_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_soft_dotted_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_soft_dotted() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_soft_dotted_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_soft_dotted_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_soft_dotted_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::segment_starter_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_segment_starter_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_segment_starter() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_segment_starter_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_segment_starter_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_segment_starter_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::case_sensitive_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_case_sensitive_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_case_sensitive() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_case_sensitive_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_case_sensitive_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_case_sensitive_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::sentence_terminal_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_sentence_terminal_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_sentence_terminal() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_sentence_terminal_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_sentence_terminal_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_sentence_terminal_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::terminal_punctuation_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_terminal_punctuation_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_terminal_punctuation() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_terminal_punctuation_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_terminal_punctuation_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_terminal_punctuation_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::unified_ideograph_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_unified_ideograph_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_unified_ideograph() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_unified_ideograph_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_unified_ideograph_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_unified_ideograph_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::uppercase_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_uppercase_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_uppercase() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_uppercase_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_uppercase_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_uppercase_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::variation_selector_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_variation_selector_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_variation_selector() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_variation_selector_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_variation_selector_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_variation_selector_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::white_space_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_white_space_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_white_space() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_white_space_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_white_space_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_white_space_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::xdigit_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_xdigit_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_xdigit() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_xdigit_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_xdigit_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_xdigit_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::xid_continue_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_xid_continue_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_xid_continue() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_xid_continue_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_xid_continue_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_xid_continue_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::CodePointSetData::xid_start_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CodePointSetData_xid_start_for_char_mv1(ch);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetData::create_xid_start() {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_xid_start_mv1();
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_xid_start_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_xid_start_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_for_ecma262(std::string_view property_name) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_for_ecma262_mv1({property_name.data(), property_name.size()});
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::create_for_ecma262_with_provider(const icu4x::DataProvider& provider, std::string_view property_name) {
    auto result = icu4x::capi::icu4x_CodePointSetData_create_for_ecma262_with_provider_mv1(provider.AsFFI(),
        {property_name.data(), property_name.size()});
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline const icu4x::capi::CodePointSetData* icu4x::CodePointSetData::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::CodePointSetData*>(this);
}

inline icu4x::capi::CodePointSetData* icu4x::CodePointSetData::AsFFI() {
    return reinterpret_cast<icu4x::capi::CodePointSetData*>(this);
}

inline const icu4x::CodePointSetData* icu4x::CodePointSetData::FromFFI(const icu4x::capi::CodePointSetData* ptr) {
    return reinterpret_cast<const icu4x::CodePointSetData*>(ptr);
}

inline icu4x::CodePointSetData* icu4x::CodePointSetData::FromFFI(icu4x::capi::CodePointSetData* ptr) {
    return reinterpret_cast<icu4x::CodePointSetData*>(ptr);
}

inline void icu4x::CodePointSetData::operator delete(void* ptr) {
    icu4x::capi::icu4x_CodePointSetData_destroy_mv1(reinterpret_cast<icu4x::capi::CodePointSetData*>(ptr));
}


#endif // ICU4X_CodePointSetData_HPP
