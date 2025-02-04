#ifndef icu4x_CodePointMapData8_D_HPP
#define icu4x_CodePointMapData8_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
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

  inline static uint32_t general_category_to_mask(uint8_t gc);

  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_value(uint8_t value) const;

  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_value_complemented(uint8_t value) const;

  inline std::unique_ptr<icu4x::CodePointRangeIterator> iter_ranges_for_mask(uint32_t mask) const;

  inline std::unique_ptr<icu4x::CodePointSetData> get_set_for_value(uint8_t value) const;

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> load_general_category(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> load_bidi_class(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> load_east_asian_width(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> load_hangul_syllable_type(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> load_indic_syllabic_category(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> load_line_break(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> try_grapheme_cluster_break(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> load_word_break(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> load_sentence_break(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> load_joining_type(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> load_canonical_combining_class(const icu4x::DataProvider& provider);

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
