#ifndef icu4x_CaseMapper_D_HPP
#define icu4x_CaseMapper_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CaseMapper; }
class CaseMapper;
namespace capi { struct CodePointSetBuilder; }
class CodePointSetBuilder;
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct Locale; }
class Locale;
struct TitlecaseOptionsV1;
class DataError;
}


namespace icu4x {
namespace capi {
    struct CaseMapper;
} // namespace capi
} // namespace

namespace icu4x {
class CaseMapper {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::CaseMapper>, icu4x::DataError> create(const icu4x::DataProvider& provider);

  inline diplomat::result<std::string, diplomat::Utf8Error> lowercase(std::string_view s, const icu4x::Locale& locale) const;

  inline diplomat::result<std::string, diplomat::Utf8Error> uppercase(std::string_view s, const icu4x::Locale& locale) const;

  inline diplomat::result<std::string, diplomat::Utf8Error> titlecase_segment_with_only_case_data_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options) const;

  inline diplomat::result<std::string, diplomat::Utf8Error> fold(std::string_view s) const;

  inline diplomat::result<std::string, diplomat::Utf8Error> fold_turkic(std::string_view s) const;

  inline void add_case_closure_to(char32_t c, icu4x::CodePointSetBuilder& builder) const;

  inline char32_t simple_lowercase(char32_t ch) const;

  inline char32_t simple_uppercase(char32_t ch) const;

  inline char32_t simple_titlecase(char32_t ch) const;

  inline char32_t simple_fold(char32_t ch) const;

  inline char32_t simple_fold_turkic(char32_t ch) const;

  inline const icu4x::capi::CaseMapper* AsFFI() const;
  inline icu4x::capi::CaseMapper* AsFFI();
  inline static const icu4x::CaseMapper* FromFFI(const icu4x::capi::CaseMapper* ptr);
  inline static icu4x::CaseMapper* FromFFI(icu4x::capi::CaseMapper* ptr);
  inline static void operator delete(void* ptr);
private:
  CaseMapper() = delete;
  CaseMapper(const icu4x::CaseMapper&) = delete;
  CaseMapper(icu4x::CaseMapper&&) noexcept = delete;
  CaseMapper operator=(const icu4x::CaseMapper&) = delete;
  CaseMapper operator=(icu4x::CaseMapper&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_CaseMapper_D_HPP
