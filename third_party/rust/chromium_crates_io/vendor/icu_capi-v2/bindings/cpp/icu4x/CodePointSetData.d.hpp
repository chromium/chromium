#ifndef icu4x_CodePointSetData_D_HPP
#define icu4x_CodePointSetData_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CodePointRangeIterator; }
class CodePointRangeIterator;
namespace capi { struct CodePointSetData; }
class CodePointSetData;
namespace capi { struct DataProvider; }
class DataProvider;
struct GeneralCategoryGroup;
class DataError;
}


namespace icu4x {
namespace capi {
    struct CodePointSetData;
} // namespace capi
} // namespace

namespace icu4x {
class CodePointSetData {
public:

  inline bool contains(char32_t cp) const;

  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges() const;

  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_complemented() const;

  inline static std::unique_ptr<icu4x::CodePointSetData> create_general_category_group(icu4x::GeneralCategoryGroup group);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_general_category_group_with_provider(const icu4x::DataProvider& provider, uint32_t group);

  inline static bool ascii_hex_digit_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_ascii_hex_digit();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_ascii_hex_digit_with_provider(const icu4x::DataProvider& provider);

  inline static bool alnum_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_alnum();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_alnum_with_provider(const icu4x::DataProvider& provider);

  inline static bool alphabetic_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_alphabetic();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_alphabetic_with_provider(const icu4x::DataProvider& provider);

  inline static bool bidi_control_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_bidi_control();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_bidi_control_with_provider(const icu4x::DataProvider& provider);

  inline static bool bidi_mirrored_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_bidi_mirrored();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_bidi_mirrored_with_provider(const icu4x::DataProvider& provider);

  inline static bool blank_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_blank();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_blank_with_provider(const icu4x::DataProvider& provider);

  inline static bool cased_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_cased();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_cased_with_provider(const icu4x::DataProvider& provider);

  inline static bool case_ignorable_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_case_ignorable();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_case_ignorable_with_provider(const icu4x::DataProvider& provider);

  inline static bool full_composition_exclusion_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_full_composition_exclusion();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_full_composition_exclusion_with_provider(const icu4x::DataProvider& provider);

  inline static bool changes_when_casefolded_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_casefolded();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_casefolded_with_provider(const icu4x::DataProvider& provider);

  inline static bool changes_when_casemapped_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_casemapped();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_casemapped_with_provider(const icu4x::DataProvider& provider);

  inline static bool changes_when_nfkc_casefolded_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_nfkc_casefolded();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_nfkc_casefolded_with_provider(const icu4x::DataProvider& provider);

  inline static bool changes_when_lowercased_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_lowercased();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_lowercased_with_provider(const icu4x::DataProvider& provider);

  inline static bool changes_when_titlecased_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_titlecased();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_titlecased_with_provider(const icu4x::DataProvider& provider);

  inline static bool changes_when_uppercased_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_changes_when_uppercased();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_changes_when_uppercased_with_provider(const icu4x::DataProvider& provider);

  inline static bool dash_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_dash();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_dash_with_provider(const icu4x::DataProvider& provider);

  inline static bool deprecated_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_deprecated();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_deprecated_with_provider(const icu4x::DataProvider& provider);

  inline static bool default_ignorable_code_point_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_default_ignorable_code_point();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_default_ignorable_code_point_with_provider(const icu4x::DataProvider& provider);

  inline static bool diacritic_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_diacritic();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_diacritic_with_provider(const icu4x::DataProvider& provider);

  inline static bool emoji_modifier_base_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_emoji_modifier_base();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_emoji_modifier_base_with_provider(const icu4x::DataProvider& provider);

  inline static bool emoji_component_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_emoji_component();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_emoji_component_with_provider(const icu4x::DataProvider& provider);

  inline static bool emoji_modifier_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_emoji_modifier();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_emoji_modifier_with_provider(const icu4x::DataProvider& provider);

  inline static bool emoji_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_emoji();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_emoji_with_provider(const icu4x::DataProvider& provider);

  inline static bool emoji_presentation_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_emoji_presentation();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_emoji_presentation_with_provider(const icu4x::DataProvider& provider);

  inline static bool extender_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_extender();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_extender_with_provider(const icu4x::DataProvider& provider);

  inline static bool extended_pictographic_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_extended_pictographic();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_extended_pictographic_with_provider(const icu4x::DataProvider& provider);

  inline static bool graph_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_graph();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_graph_with_provider(const icu4x::DataProvider& provider);

  inline static bool grapheme_base_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_grapheme_base();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_grapheme_base_with_provider(const icu4x::DataProvider& provider);

  inline static bool grapheme_extend_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_grapheme_extend();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_grapheme_extend_with_provider(const icu4x::DataProvider& provider);

  inline static bool grapheme_link_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_grapheme_link();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_grapheme_link_with_provider(const icu4x::DataProvider& provider);

  inline static bool hex_digit_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_hex_digit();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_hex_digit_with_provider(const icu4x::DataProvider& provider);

  inline static bool hyphen_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_hyphen();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_hyphen_with_provider(const icu4x::DataProvider& provider);

  inline static bool id_continue_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_id_continue();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_id_continue_with_provider(const icu4x::DataProvider& provider);

  inline static bool ideographic_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_ideographic();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_ideographic_with_provider(const icu4x::DataProvider& provider);

  inline static bool id_start_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_id_start();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_id_start_with_provider(const icu4x::DataProvider& provider);

  inline static bool ids_binary_operator_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_ids_binary_operator();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_ids_binary_operator_with_provider(const icu4x::DataProvider& provider);

  inline static bool ids_trinary_operator_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_ids_trinary_operator();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_ids_trinary_operator_with_provider(const icu4x::DataProvider& provider);

  inline static bool join_control_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_join_control();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_join_control_with_provider(const icu4x::DataProvider& provider);

  inline static bool logical_order_exception_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_logical_order_exception();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_logical_order_exception_with_provider(const icu4x::DataProvider& provider);

  inline static bool lowercase_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_lowercase();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_lowercase_with_provider(const icu4x::DataProvider& provider);

  inline static bool math_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_math();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_math_with_provider(const icu4x::DataProvider& provider);

  inline static bool noncharacter_code_point_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_noncharacter_code_point();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_noncharacter_code_point_with_provider(const icu4x::DataProvider& provider);

  inline static bool nfc_inert_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_nfc_inert();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_nfc_inert_with_provider(const icu4x::DataProvider& provider);

  inline static bool nfd_inert_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_nfd_inert();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_nfd_inert_with_provider(const icu4x::DataProvider& provider);

  inline static bool nfkc_inert_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_nfkc_inert();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_nfkc_inert_with_provider(const icu4x::DataProvider& provider);

  inline static bool nfkd_inert_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_nfkd_inert();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_nfkd_inert_with_provider(const icu4x::DataProvider& provider);

  inline static bool pattern_syntax_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_pattern_syntax();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_pattern_syntax_with_provider(const icu4x::DataProvider& provider);

  inline static bool pattern_white_space_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_pattern_white_space();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_pattern_white_space_with_provider(const icu4x::DataProvider& provider);

  inline static bool prepended_concatenation_mark_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_prepended_concatenation_mark();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_prepended_concatenation_mark_with_provider(const icu4x::DataProvider& provider);

  inline static bool print_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_print();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_print_with_provider(const icu4x::DataProvider& provider);

  inline static bool quotation_mark_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_quotation_mark();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_quotation_mark_with_provider(const icu4x::DataProvider& provider);

  inline static bool radical_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_radical();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_radical_with_provider(const icu4x::DataProvider& provider);

  inline static bool regional_indicator_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_regional_indicator();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_regional_indicator_with_provider(const icu4x::DataProvider& provider);

  inline static bool soft_dotted_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_soft_dotted();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_soft_dotted_with_provider(const icu4x::DataProvider& provider);

  inline static bool segment_starter_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_segment_starter();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_segment_starter_with_provider(const icu4x::DataProvider& provider);

  inline static bool case_sensitive_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_case_sensitive();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_case_sensitive_with_provider(const icu4x::DataProvider& provider);

  inline static bool sentence_terminal_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_sentence_terminal();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_sentence_terminal_with_provider(const icu4x::DataProvider& provider);

  inline static bool terminal_punctuation_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_terminal_punctuation();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_terminal_punctuation_with_provider(const icu4x::DataProvider& provider);

  inline static bool unified_ideograph_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_unified_ideograph();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_unified_ideograph_with_provider(const icu4x::DataProvider& provider);

  inline static bool uppercase_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_uppercase();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_uppercase_with_provider(const icu4x::DataProvider& provider);

  inline static bool variation_selector_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_variation_selector();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_variation_selector_with_provider(const icu4x::DataProvider& provider);

  inline static bool white_space_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_white_space();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_white_space_with_provider(const icu4x::DataProvider& provider);

  inline static bool xdigit_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_xdigit();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_xdigit_with_provider(const icu4x::DataProvider& provider);

  inline static bool xid_continue_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_xid_continue();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_xid_continue_with_provider(const icu4x::DataProvider& provider);

  inline static bool xid_start_for_char(char32_t ch);

  inline static std::unique_ptr<icu4x::CodePointSetData> create_xid_start();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_xid_start_with_provider(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_for_ecma262(std::string_view property_name);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> create_for_ecma262_with_provider(const icu4x::DataProvider& provider, std::string_view property_name);

  inline const icu4x::capi::CodePointSetData* AsFFI() const;
  inline icu4x::capi::CodePointSetData* AsFFI();
  inline static const icu4x::CodePointSetData* FromFFI(const icu4x::capi::CodePointSetData* ptr);
  inline static icu4x::CodePointSetData* FromFFI(icu4x::capi::CodePointSetData* ptr);
  inline static void operator delete(void* ptr);
private:
  CodePointSetData() = delete;
  CodePointSetData(const icu4x::CodePointSetData&) = delete;
  CodePointSetData(icu4x::CodePointSetData&&) noexcept = delete;
  CodePointSetData operator=(const icu4x::CodePointSetData&) = delete;
  CodePointSetData operator=(icu4x::CodePointSetData&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_CodePointSetData_D_HPP
