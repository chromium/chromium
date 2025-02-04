#ifndef icu4x_CaseMapper_HPP
#define icu4x_CaseMapper_HPP

#include "CaseMapper.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "CodePointSetBuilder.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "Locale.hpp"
#include "TitlecaseOptionsV1.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_CaseMapper_create_mv1_result {union {icu4x::capi::CaseMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CaseMapper_create_mv1_result;
    icu4x_CaseMapper_create_mv1_result icu4x_CaseMapper_create_mv1(const icu4x::capi::DataProvider* provider);
    
    void icu4x_CaseMapper_lowercase_mv1(const icu4x::capi::CaseMapper* self, diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, diplomat::capi::DiplomatWrite* write);
    
    void icu4x_CaseMapper_uppercase_mv1(const icu4x::capi::CaseMapper* self, diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, diplomat::capi::DiplomatWrite* write);
    
    void icu4x_CaseMapper_titlecase_segment_with_only_case_data_v1_mv1(const icu4x::capi::CaseMapper* self, diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, icu4x::capi::TitlecaseOptionsV1 options, diplomat::capi::DiplomatWrite* write);
    
    void icu4x_CaseMapper_fold_mv1(const icu4x::capi::CaseMapper* self, diplomat::capi::DiplomatStringView s, diplomat::capi::DiplomatWrite* write);
    
    void icu4x_CaseMapper_fold_turkic_mv1(const icu4x::capi::CaseMapper* self, diplomat::capi::DiplomatStringView s, diplomat::capi::DiplomatWrite* write);
    
    void icu4x_CaseMapper_add_case_closure_to_mv1(const icu4x::capi::CaseMapper* self, char32_t c, icu4x::capi::CodePointSetBuilder* builder);
    
    char32_t icu4x_CaseMapper_simple_lowercase_mv1(const icu4x::capi::CaseMapper* self, char32_t ch);
    
    char32_t icu4x_CaseMapper_simple_uppercase_mv1(const icu4x::capi::CaseMapper* self, char32_t ch);
    
    char32_t icu4x_CaseMapper_simple_titlecase_mv1(const icu4x::capi::CaseMapper* self, char32_t ch);
    
    char32_t icu4x_CaseMapper_simple_fold_mv1(const icu4x::capi::CaseMapper* self, char32_t ch);
    
    char32_t icu4x_CaseMapper_simple_fold_turkic_mv1(const icu4x::capi::CaseMapper* self, char32_t ch);
    
    
    void icu4x_CaseMapper_destroy_mv1(CaseMapper* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::CaseMapper>, icu4x::DataError> icu4x::CaseMapper::create(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CaseMapper_create_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CaseMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CaseMapper>>(std::unique_ptr<icu4x::CaseMapper>(icu4x::CaseMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CaseMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::string, diplomat::Utf8Error> icu4x::CaseMapper::lowercase(std::string_view s, const icu4x::Locale& locale) const {
  if (!diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return diplomat::Err<diplomat::Utf8Error>(diplomat::Utf8Error());
  }
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_CaseMapper_lowercase_mv1(this->AsFFI(),
    {s.data(), s.size()},
    locale.AsFFI(),
    &write);
  return diplomat::Ok<std::string>(std::move(output));
}

inline diplomat::result<std::string, diplomat::Utf8Error> icu4x::CaseMapper::uppercase(std::string_view s, const icu4x::Locale& locale) const {
  if (!diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return diplomat::Err<diplomat::Utf8Error>(diplomat::Utf8Error());
  }
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_CaseMapper_uppercase_mv1(this->AsFFI(),
    {s.data(), s.size()},
    locale.AsFFI(),
    &write);
  return diplomat::Ok<std::string>(std::move(output));
}

inline diplomat::result<std::string, diplomat::Utf8Error> icu4x::CaseMapper::titlecase_segment_with_only_case_data_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options) const {
  if (!diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return diplomat::Err<diplomat::Utf8Error>(diplomat::Utf8Error());
  }
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_CaseMapper_titlecase_segment_with_only_case_data_v1_mv1(this->AsFFI(),
    {s.data(), s.size()},
    locale.AsFFI(),
    options.AsFFI(),
    &write);
  return diplomat::Ok<std::string>(std::move(output));
}

inline diplomat::result<std::string, diplomat::Utf8Error> icu4x::CaseMapper::fold(std::string_view s) const {
  if (!diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return diplomat::Err<diplomat::Utf8Error>(diplomat::Utf8Error());
  }
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_CaseMapper_fold_mv1(this->AsFFI(),
    {s.data(), s.size()},
    &write);
  return diplomat::Ok<std::string>(std::move(output));
}

inline diplomat::result<std::string, diplomat::Utf8Error> icu4x::CaseMapper::fold_turkic(std::string_view s) const {
  if (!diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return diplomat::Err<diplomat::Utf8Error>(diplomat::Utf8Error());
  }
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_CaseMapper_fold_turkic_mv1(this->AsFFI(),
    {s.data(), s.size()},
    &write);
  return diplomat::Ok<std::string>(std::move(output));
}

inline void icu4x::CaseMapper::add_case_closure_to(char32_t c, icu4x::CodePointSetBuilder& builder) const {
  icu4x::capi::icu4x_CaseMapper_add_case_closure_to_mv1(this->AsFFI(),
    c,
    builder.AsFFI());
}

inline char32_t icu4x::CaseMapper::simple_lowercase(char32_t ch) const {
  auto result = icu4x::capi::icu4x_CaseMapper_simple_lowercase_mv1(this->AsFFI(),
    ch);
  return result;
}

inline char32_t icu4x::CaseMapper::simple_uppercase(char32_t ch) const {
  auto result = icu4x::capi::icu4x_CaseMapper_simple_uppercase_mv1(this->AsFFI(),
    ch);
  return result;
}

inline char32_t icu4x::CaseMapper::simple_titlecase(char32_t ch) const {
  auto result = icu4x::capi::icu4x_CaseMapper_simple_titlecase_mv1(this->AsFFI(),
    ch);
  return result;
}

inline char32_t icu4x::CaseMapper::simple_fold(char32_t ch) const {
  auto result = icu4x::capi::icu4x_CaseMapper_simple_fold_mv1(this->AsFFI(),
    ch);
  return result;
}

inline char32_t icu4x::CaseMapper::simple_fold_turkic(char32_t ch) const {
  auto result = icu4x::capi::icu4x_CaseMapper_simple_fold_turkic_mv1(this->AsFFI(),
    ch);
  return result;
}

inline const icu4x::capi::CaseMapper* icu4x::CaseMapper::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::CaseMapper*>(this);
}

inline icu4x::capi::CaseMapper* icu4x::CaseMapper::AsFFI() {
  return reinterpret_cast<icu4x::capi::CaseMapper*>(this);
}

inline const icu4x::CaseMapper* icu4x::CaseMapper::FromFFI(const icu4x::capi::CaseMapper* ptr) {
  return reinterpret_cast<const icu4x::CaseMapper*>(ptr);
}

inline icu4x::CaseMapper* icu4x::CaseMapper::FromFFI(icu4x::capi::CaseMapper* ptr) {
  return reinterpret_cast<icu4x::CaseMapper*>(ptr);
}

inline void icu4x::CaseMapper::operator delete(void* ptr) {
  icu4x::capi::icu4x_CaseMapper_destroy_mv1(reinterpret_cast<icu4x::capi::CaseMapper*>(ptr));
}


#endif // icu4x_CaseMapper_HPP
