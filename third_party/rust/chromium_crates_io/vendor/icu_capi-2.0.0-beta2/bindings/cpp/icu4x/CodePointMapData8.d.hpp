#ifndef icu4x_CodePointMapData8_D_HPP
#define icu4x_CodePointMapData8_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CodePointMapData8; }
class CodePointMapData8;
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
    struct CodePointMapData8;
} // namespace capi
} // namespace

namespace icu4x {
class CodePointMapData8 {
public:

  inline uint8_t get(char32_t cp) const;

  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_value(uint8_t value) const;

  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_value_complemented(uint8_t value) const;

  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_group(icu4x::GeneralCategoryGroup group) const;

  inline std::unique_ptr<icu4x::CodePointSetData> get_set_for_value(uint8_t value) const;

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_general_category();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_general_category_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_bidi_class();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_bidi_class_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_east_asian_width();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_east_asian_width_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_hangul_syllable_type();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_hangul_syllable_type_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_indic_syllabic_category();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_indic_syllabic_category_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_line_break();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_line_break_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_grapheme_cluster_break();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_grapheme_cluster_break_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_word_break();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_word_break_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_sentence_break();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_sentence_break_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_joining_type();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_joining_type_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::CodePointMapData8> create_canonical_combining_class();

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> create_canonical_combining_class_with_provider(const icu4x::DataProvider& provider);

  inline const icu4x::capi::CodePointMapData8* AsFFI() const;
  inline icu4x::capi::CodePointMapData8* AsFFI();
  inline static const icu4x::CodePointMapData8* FromFFI(const icu4x::capi::CodePointMapData8* ptr);
  inline static icu4x::CodePointMapData8* FromFFI(icu4x::capi::CodePointMapData8* ptr);
  inline static void operator delete(void* ptr);
private:
  CodePointMapData8() = delete;
  CodePointMapData8(const icu4x::CodePointMapData8&) = delete;
  CodePointMapData8(icu4x::CodePointMapData8&&) noexcept = delete;
  CodePointMapData8 operator=(const icu4x::CodePointMapData8&) = delete;
  CodePointMapData8 operator=(icu4x::CodePointMapData8&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_CodePointMapData8_D_HPP
