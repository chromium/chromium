#ifndef icu4x_ZonedDateTimeFormatter_HPP
#define icu4x_ZonedDateTimeFormatter_HPP

#include "ZonedDateTimeFormatter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataProvider.hpp"
#include "DateTime.hpp"
#include "DateTimeFormatError.hpp"
#include "DateTimeFormatterLoadError.hpp"
#include "DateTimeLength.hpp"
#include "IsoDateTime.hpp"
#include "Locale.hpp"
#include "TimeZoneInfo.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_ZonedDateTimeFormatter_create_with_length_mv1_result {union {icu4x::capi::ZonedDateTimeFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateTimeFormatter_create_with_length_mv1_result;
    icu4x_ZonedDateTimeFormatter_create_with_length_mv1_result icu4x_ZonedDateTimeFormatter_create_with_length_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength length);
    
    typedef struct icu4x_ZonedDateTimeFormatter_format_datetime_with_custom_time_zone_mv1_result {union { icu4x::capi::DateTimeFormatError err;}; bool is_ok;} icu4x_ZonedDateTimeFormatter_format_datetime_with_custom_time_zone_mv1_result;
    icu4x_ZonedDateTimeFormatter_format_datetime_with_custom_time_zone_mv1_result icu4x_ZonedDateTimeFormatter_format_datetime_with_custom_time_zone_mv1(const icu4x::capi::ZonedDateTimeFormatter* self, const icu4x::capi::DateTime* datetime, const icu4x::capi::TimeZoneInfo* time_zone, diplomat::capi::DiplomatWrite* write);
    
    typedef struct icu4x_ZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1_result {union { icu4x::capi::DateTimeFormatError err;}; bool is_ok;} icu4x_ZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1_result;
    icu4x_ZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1_result icu4x_ZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1(const icu4x::capi::ZonedDateTimeFormatter* self, const icu4x::capi::IsoDateTime* datetime, const icu4x::capi::TimeZoneInfo* time_zone, diplomat::capi::DiplomatWrite* write);
    
    
    void icu4x_ZonedDateTimeFormatter_destroy_mv1(ZonedDateTimeFormatter* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::ZonedDateTimeFormatter::create_with_length(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length) {
  auto result = icu4x::capi::icu4x_ZonedDateTimeFormatter_create_with_length_mv1(provider.AsFFI(),
    locale.AsFFI(),
    length.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Ok<std::unique_ptr<icu4x::ZonedDateTimeFormatter>>(std::unique_ptr<icu4x::ZonedDateTimeFormatter>(icu4x::ZonedDateTimeFormatter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::ZonedDateTimeFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline diplomat::result<std::string, icu4x::DateTimeFormatError> icu4x::ZonedDateTimeFormatter::format_datetime_with_custom_time_zone(const icu4x::DateTime& datetime, const icu4x::TimeZoneInfo& time_zone) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_ZonedDateTimeFormatter_format_datetime_with_custom_time_zone_mv1(this->AsFFI(),
    datetime.AsFFI(),
    time_zone.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, icu4x::DateTimeFormatError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, icu4x::DateTimeFormatError>(diplomat::Err<icu4x::DateTimeFormatError>(icu4x::DateTimeFormatError::FromFFI(result.err)));
}

inline diplomat::result<std::string, icu4x::DateTimeFormatError> icu4x::ZonedDateTimeFormatter::format_iso_datetime_with_custom_time_zone(const icu4x::IsoDateTime& datetime, const icu4x::TimeZoneInfo& time_zone) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_ZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1(this->AsFFI(),
    datetime.AsFFI(),
    time_zone.AsFFI(),
    &write);
  return result.is_ok ? diplomat::result<std::string, icu4x::DateTimeFormatError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, icu4x::DateTimeFormatError>(diplomat::Err<icu4x::DateTimeFormatError>(icu4x::DateTimeFormatError::FromFFI(result.err)));
}

inline const icu4x::capi::ZonedDateTimeFormatter* icu4x::ZonedDateTimeFormatter::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::ZonedDateTimeFormatter*>(this);
}

inline icu4x::capi::ZonedDateTimeFormatter* icu4x::ZonedDateTimeFormatter::AsFFI() {
  return reinterpret_cast<icu4x::capi::ZonedDateTimeFormatter*>(this);
}

inline const icu4x::ZonedDateTimeFormatter* icu4x::ZonedDateTimeFormatter::FromFFI(const icu4x::capi::ZonedDateTimeFormatter* ptr) {
  return reinterpret_cast<const icu4x::ZonedDateTimeFormatter*>(ptr);
}

inline icu4x::ZonedDateTimeFormatter* icu4x::ZonedDateTimeFormatter::FromFFI(icu4x::capi::ZonedDateTimeFormatter* ptr) {
  return reinterpret_cast<icu4x::ZonedDateTimeFormatter*>(ptr);
}

inline void icu4x::ZonedDateTimeFormatter::operator delete(void* ptr) {
  icu4x::capi::icu4x_ZonedDateTimeFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::ZonedDateTimeFormatter*>(ptr));
}


#endif // icu4x_ZonedDateTimeFormatter_HPP
