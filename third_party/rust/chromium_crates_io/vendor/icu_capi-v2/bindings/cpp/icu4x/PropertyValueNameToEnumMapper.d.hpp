#ifndef icu4x_PropertyValueNameToEnumMapper_D_HPP
#define icu4x_PropertyValueNameToEnumMapper_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
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

  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_general_category();

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_general_category_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_hangul_syllable_type();

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_hangul_syllable_type_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_east_asian_width();

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_east_asian_width_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_bidi_class();

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_bidi_class_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_indic_syllabic_category();

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_indic_syllabic_category_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_line_break();

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_line_break_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_grapheme_cluster_break();

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_grapheme_cluster_break_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_word_break();

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_word_break_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_sentence_break();

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_sentence_break_with_provider(const icu4x::DataProvider& provider);

  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_script();

  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_script_with_provider(const icu4x::DataProvider& provider);

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
