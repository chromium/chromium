#ifndef icu4x_GregorianZonedDateTimeFormatter_HPP
#define icu4x_GregorianZonedDateTimeFormatter_HPP

#include "GregorianZonedDateTimeFormatter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataProvider.hpp"
#include "DateTimeFormatError.hpp"
#include "DateTimeFormatterLoadError.hpp"
#include "DateTimeLength.hpp"
#include "IsoDate.hpp"
#include "Locale.hpp"
#include "Time.hpp"
#include "TimeZoneInfo.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_GregorianZonedDateTimeFormatter_create_with_length_mv1_result {union {icu4x::capi::GregorianZonedDateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_GregorianZonedDateTimeFormatter_create_with_length_mv1_result;
    icu4x_GregorianZonedDateTimeFormatter_create_with_length_mv1_result icu4x_GregorianZonedDateTimeFormatter_create_with_length_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength length);
    
    typedef struct icu4x_GregorianZonedDateTimeFormatter_create_with_length_and_provider_mv1_result {union {icu4x::capi::GregorianZonedDateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_GregorianZonedDateTimeFormatter_create_with_length_and_provider_mv1_result;
    icu4x_GregorianZonedDateTimeFormatter_create_with_length_and_provider_mv1_result icu4x_GregorianZonedDateTimeFormatter_create_with_length_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength length);
    
    typedef struct icu4x_GregorianZonedDateTimeFormatter_format_iso_mv1_result {union { icu4x::capi::DateTimeFormatError err;}; bool is_ok;} icu4x_GregorianZonedDateTimeFormatter_format_iso_mv1_result;
    icu4x_GregorianZonedDateTimeFormatter_format_iso_mv1_result icu4x_GregorianZonedDateTimeFormatter_format_iso_mv1(const icu4x::capi::GregorianZonedDateTimeFormatter* self, const icu4x::capi::IsoDate* date, const icu4x::capi::Time* time, const icu4x::capi::TimeZoneInfo* zone, diplomat::capi::DiplomatWrite* write);
    
    
    void icu4x_GregorianZonedDateTimeFormatter_destroy_mv1(GregorianZonedDateTimeFormatter* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::GregorianZonedDateTimeFormatter::create_with_length(const icu4x::Locale& locale, icu4x::DateTimeLength length) {
  auto result = icu4x::capi::icu4x_GregorianZonedDateTimeFormatter_create_with_length_mv1(locale.AsFFI(),
    length.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Ok<std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>>(std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>(icu4x::GregorianZonedDateTimeFormatter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::GregorianZonedDateTimeFormatter::create_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length) {
  auto result = icu4x::capi::icu4x_GregorianZonedDateTimeFormatter_create_with_length_and_provider_mv1(provider.AsFFI(),
    locale.AsFFI(),
    length.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Ok<std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>>(std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>(icu4x::GregorianZonedDateTimeFormatter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::GregorianZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline diplomat::result<std::string, icu4x::DateTimeFormatError> icu4x::GregorianZonedDateTimeFormatter::format_iso(const icu4x::IsoDate& date, const icu4x::Time& time, const icu4x::TimeZoneInfo& zone) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_GregorianZonedDateTimeFormatter_format_iso_mv1(this->AsFFI(),
    date.AsFFI(),
    time.AsFFI(),
    zone.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, icu4x::DateTimeFormatError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, icu4x::DateTimeFormatError>(diplomat::Err<icu4x::DateTimeFormatError>(icu4x::DateTimeFormatError::FromFFI(result.err)));
}

inline const icu4x::capi::GregorianZonedDateTimeFormatter* icu4x::GregorianZonedDateTimeFormatter::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::GregorianZonedDateTimeFormatter*>(this);
}

inline icu4x::capi::GregorianZonedDateTimeFormatter* icu4x::GregorianZonedDateTimeFormatter::AsFFI() {
  return reinterpret_cast<icu4x::capi::GregorianZonedDateTimeFormatter*>(this);
}

inline const icu4x::GregorianZonedDateTimeFormatter* icu4x::GregorianZonedDateTimeFormatter::FromFFI(const icu4x::capi::GregorianZonedDateTimeFormatter* ptr) {
  return reinterpret_cast<const icu4x::GregorianZonedDateTimeFormatter*>(ptr);
}

inline icu4x::GregorianZonedDateTimeFormatter* icu4x::GregorianZonedDateTimeFormatter::FromFFI(icu4x::capi::GregorianZonedDateTimeFormatter* ptr) {
  return reinterpret_cast<icu4x::GregorianZonedDateTimeFormatter*>(ptr);
}

inline void icu4x::GregorianZonedDateTimeFormatter::operator delete(void* ptr) {
  icu4x::capi::icu4x_GregorianZonedDateTimeFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::GregorianZonedDateTimeFormatter*>(ptr));
}


#endif // icu4x_GregorianZonedDateTimeFormatter_HPP
