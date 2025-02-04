#ifndef icu4x_PropertyValueNameToEnumMapper_D_HPP
#define icu4x_PropertyValueNameToEnumMapper_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct PropertyValueNameToEnumMapper; }
class PropertyValueNameToEnumMapper;
class DataError;
}


namespace icu4x {
namespace capi {
    struct PropertyValueNameToEnumMapper;
} // namespace capi
} // namespace

namespace icu4x {
class PropertyValueNameToEnumMapper {
public:

  inline int16_t get_strict(std::string_view name) const;

  inline int16_t get_loose(std::string_view name) const;

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> load_general_category(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> load_hangul_syllable_type(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> load_east_asian_width(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> load_bidi_class(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> load_indic_syllabic_category(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> load_line_break(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> load_grapheme_cluster_break(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> load_word_break(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> load_sentence_break(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> load_script(const icu4x::DataProvider& provider);

  inline const icu4x::capi::PropertyValueNameToEnumMapper* AsFFI() const;
  inline icu4x::capi::PropertyValueNameToEnumMapper* AsFFI();
  inline static const icu4x::PropertyValueNameToEnumMapper* FromFFI(const icu4x::capi::PropertyValueNameToEnumMapper* ptr);
  inline static icu4x::PropertyValueNameToEnumMapper* FromFFI(icu4x::capi::PropertyValueNameToEnumMapper* ptr);
  inline static void operator delete(void* ptr);
private:
  PropertyValueNameToEnumMapper() = delete;
  PropertyValueNameToEnumMapper(const icu4x::PropertyValueNameToEnumMapper&) = delete;
  PropertyValueNameToEnumMapper(icu4x::PropertyValueNameToEnumMapper&&) noexcept = delete;
  PropertyValueNameToEnumMapper operator=(const icu4x::PropertyValueNameToEnumMapper&) = delete;
  PropertyValueNameToEnumMapper operator=(icu4x::PropertyValueNameToEnumMapper&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_PropertyValueNameToEnumMapper_D_HPP
