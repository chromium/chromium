#ifndef icu4x_DateFormatter_HPP
#define icu4x_DateFormatter_HPP

#include "DateFormatter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "Calendar.hpp"
#include "DataProvider.hpp"
#include "Date.hpp"
#include "DateTimeFormatError.hpp"
#include "DateTimeFormatterLoadError.hpp"
#include "DateTimeLength.hpp"
#include "IsoDate.hpp"
#include "Locale.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_DateFormatter_create_with_length_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_with_length_mv1_result;
    icu4x_DateFormatter_create_with_length_mv1_result icu4x_DateFormatter_create_with_length_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength length);
    
    typedef struct icu4x_DateFormatter_create_with_length_and_provider_mv1_result {union {icu4x::capi::DateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateFormatter_create_with_length_and_provider_mv1_result;
    icu4x_DateFormatter_create_with_length_and_provider_mv1_result icu4x_DateFormatter_create_with_length_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength length);
    
    typedef struct icu4x_DateFormatter_format_mv1_result {union { icu4x::capi::DateTimeFormatError err;}; bool is_ok;} icu4x_DateFormatter_format_mv1_result;
    icu4x_DateFormatter_format_mv1_result icu4x_DateFormatter_format_mv1(const icu4x::capi::DateFormatter* self, const icu4x::capi::Date* value, diplomat::capi::DiplomatWrite* write);
    
    typedef struct icu4x_DateFormatter_format_iso_mv1_result {union { icu4x::capi::DateTimeFormatError err;}; bool is_ok;} icu4x_DateFormatter_format_iso_mv1_result;
    icu4x_DateFormatter_format_iso_mv1_result icu4x_DateFormatter_format_iso_mv1(const icu4x::capi::DateFormatter* self, const icu4x::capi::IsoDate* value, diplomat::capi::DiplomatWrite* write);
    
    icu4x::capi::Calendar* icu4x_DateFormatter_calendar_mv1(const icu4x::capi::DateFormatter* self);
    
    
    void icu4x_DateFormatter_destroy_mv1(DateFormatter* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_with_length(const icu4x::Locale& locale, icu4x::DateTimeLength length) {
  auto result = icu4x::capi::icu4x_DateFormatter_create_with_length_mv1(locale.AsFFI(),
    length.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::DateFormatter::create_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length) {
  auto result = icu4x::capi::icu4x_DateFormatter_create_with_length_and_provider_mv1(provider.AsFFI(),
    locale.AsFFI(),
    length.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Ok<std::unique_ptr<icu4x::DateFormatter>>(std::unique_ptr<icu4x::DateFormatter>(icu4x::DateFormatter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::DateFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline diplomat::result<std::string, icu4x::DateTimeFormatError> icu4x::DateFormatter::format(const icu4x::Date& value) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_DateFormatter_format_mv1(this->AsFFI(),
    value.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, icu4x::DateTimeFormatError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, icu4x::DateTimeFormatError>(diplomat::Err<icu4x::DateTimeFormatError>(icu4x::DateTimeFormatError::FromFFI(result.err)));
}

inline diplomat::result<std::string, icu4x::DateTimeFormatError> icu4x::DateFormatter::format_iso(const icu4x::IsoDate& value) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_DateFormatter_format_iso_mv1(this->AsFFI(),
    value.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, icu4x::DateTimeFormatError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, icu4x::DateTimeFormatError>(diplomat::Err<icu4x::DateTimeFormatError>(icu4x::DateTimeFormatError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::Calendar> icu4x::DateFormatter::calendar() const {
  auto result = icu4x::capi::icu4x_DateFormatter_calendar_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::Calendar>(icu4x::Calendar::FromFFI(result));
}

inline const icu4x::capi::DateFormatter* icu4x::DateFormatter::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::DateFormatter*>(this);
}

inline icu4x::capi::DateFormatter* icu4x::DateFormatter::AsFFI() {
  return reinterpret_cast<icu4x::capi::DateFormatter*>(this);
}

inline const icu4x::DateFormatter* icu4x::DateFormatter::FromFFI(const icu4x::capi::DateFormatter* ptr) {
  return reinterpret_cast<const icu4x::DateFormatter*>(ptr);
}

inline icu4x::DateFormatter* icu4x::DateFormatter::FromFFI(icu4x::capi::DateFormatter* ptr) {
  return reinterpret_cast<icu4x::DateFormatter*>(ptr);
}

inline void icu4x::DateFormatter::operator delete(void* ptr) {
  icu4x::capi::icu4x_DateFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::DateFormatter*>(ptr));
}


#endif // icu4x_DateFormatter_HPP
