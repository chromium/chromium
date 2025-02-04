#ifndef icu4x_CodePointSetData_D_HPP
#define icu4x_CodePointSetData_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CodePointRangeIterator; }
class CodePointRangeIterator;
namespace capi { struct CodePointSetData; }
class CodePointSetData;
namespace capi { struct DataProvider; }
class DataProvider;
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

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_for_general_category_group(const icu4x::DataProvider& provider, uint32_t group);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_ascii_hex_digit(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_alnum(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_alphabetic(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_bidi_control(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_bidi_mirrored(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_blank(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_cased(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_case_ignorable(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_full_composition_exclusion(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_changes_when_casefolded(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_changes_when_casemapped(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_changes_when_nfkc_casefolded(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_changes_when_lowercased(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_changes_when_titlecased(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_changes_when_uppercased(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_dash(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_deprecated(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_default_ignorable_code_point(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_diacritic(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_emoji_modifier_base(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_emoji_component(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_emoji_modifier(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_emoji(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_emoji_presentation(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_extender(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_extended_pictographic(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_graph(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_grapheme_base(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_grapheme_extend(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_grapheme_link(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_hex_digit(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_hyphen(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_id_continue(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_ideographic(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_id_start(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_ids_binary_operator(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_ids_trinary_operator(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_join_control(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_logical_order_exception(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_lowercase(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_math(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_noncharacter_code_point(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_nfc_inert(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_nfd_inert(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_nfkc_inert(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_nfkd_inert(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_pattern_syntax(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_pattern_white_space(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_prepended_concatenation_mark(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_print(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_quotation_mark(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_radical(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_regional_indicator(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_soft_dotted(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_segment_starter(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_case_sensitive(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_sentence_terminal(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_terminal_punctuation(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_unified_ideograph(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_uppercase(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_variation_selector(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_white_space(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_xdigit(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_xid_continue(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_xid_start(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointSetData>, icu4x::DataError> load_for_ecma262(const icu4x::DataProvider& provider, std::string_view property_name);

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
