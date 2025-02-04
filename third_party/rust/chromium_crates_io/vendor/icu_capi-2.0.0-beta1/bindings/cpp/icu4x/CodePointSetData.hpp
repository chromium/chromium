#ifndef icu4x_CodePointSetData_HPP
#define icu4x_CodePointSetData_HPP

#include "CodePointSetData.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "CodePointRangeIterator.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    bool icu4x_CodePointSetData_contains_mv1(const icu4x::capi::CodePointSetData* self, char32_t cp);
    
    icu4x::capi::CodePointRangeIterator* icu4x_CodePointSetData_iter_ranges_mv1(const icu4x::capi::CodePointSetData* self);
    
    icu4x::capi::CodePointRangeIterator* icu4x_CodePointSetData_iter_ranges_complemented_mv1(const icu4x::capi::CodePointSetData* self);
    
    typedef struct icu4x_CodePointSetData_load_for_general_category_group_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_for_general_category_group_mv1_result;
    icu4x_CodePointSetData_load_for_general_category_group_mv1_result icu4x_CodePointSetData_load_for_general_category_group_mv1(const icu4x::capi::DataProvider* provider, uint32_t group);
    
    typedef struct icu4x_CodePointSetData_load_ascii_hex_digit_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_ascii_hex_digit_mv1_result;
    icu4x_CodePointSetData_load_ascii_hex_digit_mv1_result icu4x_CodePointSetData_load_ascii_hex_digit_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_alnum_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_alnum_mv1_result;
    icu4x_CodePointSetData_load_alnum_mv1_result icu4x_CodePointSetData_load_alnum_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_alphabetic_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_alphabetic_mv1_result;
    icu4x_CodePointSetData_load_alphabetic_mv1_result icu4x_CodePointSetData_load_alphabetic_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_bidi_control_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_bidi_control_mv1_result;
    icu4x_CodePointSetData_load_bidi_control_mv1_result icu4x_CodePointSetData_load_bidi_control_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_bidi_mirrored_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_bidi_mirrored_mv1_result;
    icu4x_CodePointSetData_load_bidi_mirrored_mv1_result icu4x_CodePointSetData_load_bidi_mirrored_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_blank_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_blank_mv1_result;
    icu4x_CodePointSetData_load_blank_mv1_result icu4x_CodePointSetData_load_blank_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_cased_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_cased_mv1_result;
    icu4x_CodePointSetData_load_cased_mv1_result icu4x_CodePointSetData_load_cased_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_case_ignorable_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_case_ignorable_mv1_result;
    icu4x_CodePointSetData_load_case_ignorable_mv1_result icu4x_CodePointSetData_load_case_ignorable_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_full_composition_exclusion_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_full_composition_exclusion_mv1_result;
    icu4x_CodePointSetData_load_full_composition_exclusion_mv1_result icu4x_CodePointSetData_load_full_composition_exclusion_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_changes_when_casefolded_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_changes_when_casefolded_mv1_result;
    icu4x_CodePointSetData_load_changes_when_casefolded_mv1_result icu4x_CodePointSetData_load_changes_when_casefolded_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_changes_when_casemapped_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_changes_when_casemapped_mv1_result;
    icu4x_CodePointSetData_load_changes_when_casemapped_mv1_result icu4x_CodePointSetData_load_changes_when_casemapped_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_changes_when_nfkc_casefolded_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_changes_when_nfkc_casefolded_mv1_result;
    icu4x_CodePointSetData_load_changes_when_nfkc_casefolded_mv1_result icu4x_CodePointSetData_load_changes_when_nfkc_casefolded_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_changes_when_lowercased_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_changes_when_lowercased_mv1_result;
    icu4x_CodePointSetData_load_changes_when_lowercased_mv1_result icu4x_CodePointSetData_load_changes_when_lowercased_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_changes_when_titlecased_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_changes_when_titlecased_mv1_result;
    icu4x_CodePointSetData_load_changes_when_titlecased_mv1_result icu4x_CodePointSetData_load_changes_when_titlecased_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_changes_when_uppercased_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_changes_when_uppercased_mv1_result;
    icu4x_CodePointSetData_load_changes_when_uppercased_mv1_result icu4x_CodePointSetData_load_changes_when_uppercased_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_dash_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_dash_mv1_result;
    icu4x_CodePointSetData_load_dash_mv1_result icu4x_CodePointSetData_load_dash_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_deprecated_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_deprecated_mv1_result;
    icu4x_CodePointSetData_load_deprecated_mv1_result icu4x_CodePointSetData_load_deprecated_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_default_ignorable_code_point_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_default_ignorable_code_point_mv1_result;
    icu4x_CodePointSetData_load_default_ignorable_code_point_mv1_result icu4x_CodePointSetData_load_default_ignorable_code_point_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_diacritic_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_diacritic_mv1_result;
    icu4x_CodePointSetData_load_diacritic_mv1_result icu4x_CodePointSetData_load_diacritic_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_emoji_modifier_base_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_emoji_modifier_base_mv1_result;
    icu4x_CodePointSetData_load_emoji_modifier_base_mv1_result icu4x_CodePointSetData_load_emoji_modifier_base_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_emoji_component_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_emoji_component_mv1_result;
    icu4x_CodePointSetData_load_emoji_component_mv1_result icu4x_CodePointSetData_load_emoji_component_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_emoji_modifier_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_emoji_modifier_mv1_result;
    icu4x_CodePointSetData_load_emoji_modifier_mv1_result icu4x_CodePointSetData_load_emoji_modifier_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_emoji_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_emoji_mv1_result;
    icu4x_CodePointSetData_load_emoji_mv1_result icu4x_CodePointSetData_load_emoji_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_emoji_presentation_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_emoji_presentation_mv1_result;
    icu4x_CodePointSetData_load_emoji_presentation_mv1_result icu4x_CodePointSetData_load_emoji_presentation_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_extender_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_extender_mv1_result;
    icu4x_CodePointSetData_load_extender_mv1_result icu4x_CodePointSetData_load_extender_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_extended_pictographic_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_extended_pictographic_mv1_result;
    icu4x_CodePointSetData_load_extended_pictographic_mv1_result icu4x_CodePointSetData_load_extended_pictographic_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_graph_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_graph_mv1_result;
    icu4x_CodePointSetData_load_graph_mv1_result icu4x_CodePointSetData_load_graph_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_grapheme_base_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_grapheme_base_mv1_result;
    icu4x_CodePointSetData_load_grapheme_base_mv1_result icu4x_CodePointSetData_load_grapheme_base_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_grapheme_extend_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_grapheme_extend_mv1_result;
    icu4x_CodePointSetData_load_grapheme_extend_mv1_result icu4x_CodePointSetData_load_grapheme_extend_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_grapheme_link_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_grapheme_link_mv1_result;
    icu4x_CodePointSetData_load_grapheme_link_mv1_result icu4x_CodePointSetData_load_grapheme_link_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_hex_digit_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_hex_digit_mv1_result;
    icu4x_CodePointSetData_load_hex_digit_mv1_result icu4x_CodePointSetData_load_hex_digit_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_hyphen_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_hyphen_mv1_result;
    icu4x_CodePointSetData_load_hyphen_mv1_result icu4x_CodePointSetData_load_hyphen_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_id_continue_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_id_continue_mv1_result;
    icu4x_CodePointSetData_load_id_continue_mv1_result icu4x_CodePointSetData_load_id_continue_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_ideographic_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_ideographic_mv1_result;
    icu4x_CodePointSetData_load_ideographic_mv1_result icu4x_CodePointSetData_load_ideographic_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_id_start_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_id_start_mv1_result;
    icu4x_CodePointSetData_load_id_start_mv1_result icu4x_CodePointSetData_load_id_start_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_ids_binary_operator_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_ids_binary_operator_mv1_result;
    icu4x_CodePointSetData_load_ids_binary_operator_mv1_result icu4x_CodePointSetData_load_ids_binary_operator_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_ids_trinary_operator_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_ids_trinary_operator_mv1_result;
    icu4x_CodePointSetData_load_ids_trinary_operator_mv1_result icu4x_CodePointSetData_load_ids_trinary_operator_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_join_control_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_join_control_mv1_result;
    icu4x_CodePointSetData_load_join_control_mv1_result icu4x_CodePointSetData_load_join_control_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_logical_order_exception_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_logical_order_exception_mv1_result;
    icu4x_CodePointSetData_load_logical_order_exception_mv1_result icu4x_CodePointSetData_load_logical_order_exception_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_lowercase_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_lowercase_mv1_result;
    icu4x_CodePointSetData_load_lowercase_mv1_result icu4x_CodePointSetData_load_lowercase_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_math_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_math_mv1_result;
    icu4x_CodePointSetData_load_math_mv1_result icu4x_CodePointSetData_load_math_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_noncharacter_code_point_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_noncharacter_code_point_mv1_result;
    icu4x_CodePointSetData_load_noncharacter_code_point_mv1_result icu4x_CodePointSetData_load_noncharacter_code_point_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_nfc_inert_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_nfc_inert_mv1_result;
    icu4x_CodePointSetData_load_nfc_inert_mv1_result icu4x_CodePointSetData_load_nfc_inert_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_nfd_inert_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_nfd_inert_mv1_result;
    icu4x_CodePointSetData_load_nfd_inert_mv1_result icu4x_CodePointSetData_load_nfd_inert_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_nfkc_inert_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_nfkc_inert_mv1_result;
    icu4x_CodePointSetData_load_nfkc_inert_mv1_result icu4x_CodePointSetData_load_nfkc_inert_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_nfkd_inert_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_nfkd_inert_mv1_result;
    icu4x_CodePointSetData_load_nfkd_inert_mv1_result icu4x_CodePointSetData_load_nfkd_inert_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_pattern_syntax_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_pattern_syntax_mv1_result;
    icu4x_CodePointSetData_load_pattern_syntax_mv1_result icu4x_CodePointSetData_load_pattern_syntax_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_pattern_white_space_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_pattern_white_space_mv1_result;
    icu4x_CodePointSetData_load_pattern_white_space_mv1_result icu4x_CodePointSetData_load_pattern_white_space_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_prepended_concatenation_mark_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_prepended_concatenation_mark_mv1_result;
    icu4x_CodePointSetData_load_prepended_concatenation_mark_mv1_result icu4x_CodePointSetData_load_prepended_concatenation_mark_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_print_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_print_mv1_result;
    icu4x_CodePointSetData_load_print_mv1_result icu4x_CodePointSetData_load_print_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_quotation_mark_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_quotation_mark_mv1_result;
    icu4x_CodePointSetData_load_quotation_mark_mv1_result icu4x_CodePointSetData_load_quotation_mark_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_radical_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_radical_mv1_result;
    icu4x_CodePointSetData_load_radical_mv1_result icu4x_CodePointSetData_load_radical_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_regional_indicator_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_regional_indicator_mv1_result;
    icu4x_CodePointSetData_load_regional_indicator_mv1_result icu4x_CodePointSetData_load_regional_indicator_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_soft_dotted_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_soft_dotted_mv1_result;
    icu4x_CodePointSetData_load_soft_dotted_mv1_result icu4x_CodePointSetData_load_soft_dotted_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_segment_starter_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_segment_starter_mv1_result;
    icu4x_CodePointSetData_load_segment_starter_mv1_result icu4x_CodePointSetData_load_segment_starter_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_case_sensitive_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_case_sensitive_mv1_result;
    icu4x_CodePointSetData_load_case_sensitive_mv1_result icu4x_CodePointSetData_load_case_sensitive_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_sentence_terminal_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_sentence_terminal_mv1_result;
    icu4x_CodePointSetData_load_sentence_terminal_mv1_result icu4x_CodePointSetData_load_sentence_terminal_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_terminal_punctuation_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_terminal_punctuation_mv1_result;
    icu4x_CodePointSetData_load_terminal_punctuation_mv1_result icu4x_CodePointSetData_load_terminal_punctuation_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_unified_ideograph_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_unified_ideograph_mv1_result;
    icu4x_CodePointSetData_load_unified_ideograph_mv1_result icu4x_CodePointSetData_load_unified_ideograph_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_uppercase_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_uppercase_mv1_result;
    icu4x_CodePointSetData_load_uppercase_mv1_result icu4x_CodePointSetData_load_uppercase_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_variation_selector_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_variation_selector_mv1_result;
    icu4x_CodePointSetData_load_variation_selector_mv1_result icu4x_CodePointSetData_load_variation_selector_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_white_space_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_white_space_mv1_result;
    icu4x_CodePointSetData_load_white_space_mv1_result icu4x_CodePointSetData_load_white_space_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_xdigit_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_xdigit_mv1_result;
    icu4x_CodePointSetData_load_xdigit_mv1_result icu4x_CodePointSetData_load_xdigit_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_xid_continue_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_xid_continue_mv1_result;
    icu4x_CodePointSetData_load_xid_continue_mv1_result icu4x_CodePointSetData_load_xid_continue_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_xid_start_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_xid_start_mv1_result;
    icu4x_CodePointSetData_load_xid_start_mv1_result icu4x_CodePointSetData_load_xid_start_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointSetData_load_for_ecma262_mv1_result {union {icu4x::capi::CodePointSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointSetData_load_for_ecma262_mv1_result;
    icu4x_CodePointSetData_load_for_ecma262_mv1_result icu4x_CodePointSetData_load_for_ecma262_mv1(const icu4x::capi::DataProvider* provider, diplomat::capi::DiplomatStringView property_name);
    
    
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

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_for_general_category_group(const icu4x::DataProvider& provider, uint32_t group) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_for_general_category_group_mv1(provider.AsFFI(),
    group);
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_ascii_hex_digit(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_ascii_hex_digit_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_alnum(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_alnum_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_alphabetic(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_alphabetic_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_bidi_control(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_bidi_control_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_bidi_mirrored(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_bidi_mirrored_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_blank(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_blank_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_cased(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_cased_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_case_ignorable(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_case_ignorable_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_full_composition_exclusion(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_full_composition_exclusion_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_changes_when_casefolded(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_changes_when_casefolded_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_changes_when_casemapped(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_changes_when_casemapped_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_changes_when_nfkc_casefolded(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_changes_when_nfkc_casefolded_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_changes_when_lowercased(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_changes_when_lowercased_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_changes_when_titlecased(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_changes_when_titlecased_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_changes_when_uppercased(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_changes_when_uppercased_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_dash(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_dash_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_deprecated(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_deprecated_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_default_ignorable_code_point(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_default_ignorable_code_point_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_diacritic(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_diacritic_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_emoji_modifier_base(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_emoji_modifier_base_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_emoji_component(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_emoji_component_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_emoji_modifier(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_emoji_modifier_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_emoji(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_emoji_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_emoji_presentation(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_emoji_presentation_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_extender(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_extender_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_extended_pictographic(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_extended_pictographic_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_graph(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_graph_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_grapheme_base(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_grapheme_base_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_grapheme_extend(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_grapheme_extend_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_grapheme_link(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_grapheme_link_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_hex_digit(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_hex_digit_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_hyphen(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_hyphen_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_id_continue(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_id_continue_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_ideographic(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_ideographic_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_id_start(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_id_start_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_ids_binary_operator(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_ids_binary_operator_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_ids_trinary_operator(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_ids_trinary_operator_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_join_control(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_join_control_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_logical_order_exception(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_logical_order_exception_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_lowercase(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_lowercase_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_math(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_math_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_noncharacter_code_point(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_noncharacter_code_point_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_nfc_inert(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_nfc_inert_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_nfd_inert(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_nfd_inert_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_nfkc_inert(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_nfkc_inert_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_nfkd_inert(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_nfkd_inert_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_pattern_syntax(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_pattern_syntax_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_pattern_white_space(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_pattern_white_space_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_prepended_concatenation_mark(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_prepended_concatenation_mark_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_print(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_print_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_quotation_mark(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_quotation_mark_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_radical(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_radical_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_regional_indicator(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_regional_indicator_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_soft_dotted(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_soft_dotted_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_segment_starter(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_segment_starter_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_case_sensitive(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_case_sensitive_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_sentence_terminal(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_sentence_terminal_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_terminal_punctuation(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_terminal_punctuation_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_unified_ideograph(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_unified_ideograph_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_uppercase(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_uppercase_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_variation_selector(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_variation_selector_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_white_space(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_white_space_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_xdigit(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_xdigit_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_xid_continue(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_xid_continue_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_xid_start(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_xid_start_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> icu4x::CodePointSetData::load_for_ecma262(const icu4x::DataProvider& provider, std::string_view property_name) {
  auto result = icu4x::capi::icu4x_CodePointSetData_load_for_ecma262_mv1(provider.AsFFI(),
    {property_name.data(), property_name.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointSetData>>(std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
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


#endif // icu4x_CodePointSetData_HPP
