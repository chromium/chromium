#ifndef icu4x_NoCalendarFormatter_HPP
#define icu4x_NoCalendarFormatter_HPP

#include "NoCalendarFormatter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataProvider.hpp"
#include "DateTimeFormatterLoadError.hpp"
#include "DateTimeLength.hpp"
#include "Locale.hpp"
#include "Time.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_NoCalendarFormatter_create_with_length_mv1_result {union {icu4x::capi::NoCalendarFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_NoCalendarFormatter_create_with_length_mv1_result;
    icu4x_NoCalendarFormatter_create_with_length_mv1_result icu4x_NoCalendarFormatter_create_with_length_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength length);
    
    typedef struct icu4x_NoCalendarFormatter_create_with_length_and_provider_mv1_result {union {icu4x::capi::NoCalendarFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_NoCalendarFormatter_create_with_length_and_provider_mv1_result;
    icu4x_NoCalendarFormatter_create_with_length_and_provider_mv1_result icu4x_NoCalendarFormatter_create_with_length_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength length);
    
    void icu4x_NoCalendarFormatter_format_mv1(const icu4x::capi::NoCalendarFormatter* self, const icu4x::capi::Time* value, diplomat::capi::DiplomatWrite* write);
    
    
    void icu4x_NoCalendarFormatter_destroy_mv1(NoCalendarFormatter* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::NoCalendarFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::NoCalendarFormatter::create_with_length(const icu4x::Locale& locale, icu4x::DateTimeLength length) {
  auto result = icu4x::capi::icu4x_NoCalendarFormatter_create_with_length_mv1(locale.AsFFI(),
    length.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::NoCalendarFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Ok<std::unique_ptr<icu4x::NoCalendarFormatter>>(std::unique_ptr<icu4x::NoCalendarFormatter>(icu4x::NoCalendarFormatter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::NoCalendarFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::NoCalendarFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::NoCalendarFormatter::create_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length) {
  auto result = icu4x::capi::icu4x_NoCalendarFormatter_create_with_length_and_provider_mv1(provider.AsFFI(),
    locale.AsFFI(),
    length.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::NoCalendarFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Ok<std::unique_ptr<icu4x::NoCalendarFormatter>>(std::unique_ptr<icu4x::NoCalendarFormatter>(icu4x::NoCalendarFormatter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::NoCalendarFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline std::string icu4x::NoCalendarFormatter::format(const icu4x::Time& value) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_NoCalendarFormatter_format_mv1(this->AsFFI(),
    value.AsFFI(),
    &write);
  return output;
}

inline const icu4x::capi::NoCalendarFormatter* icu4x::NoCalendarFormatter::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::NoCalendarFormatter*>(this);
}

inline icu4x::capi::NoCalendarFormatter* icu4x::NoCalendarFormatter::AsFFI() {
  return reinterpret_cast<icu4x::capi::NoCalendarFormatter*>(this);
}

inline const icu4x::NoCalendarFormatter* icu4x::NoCalendarFormatter::FromFFI(const icu4x::capi::NoCalendarFormatter* ptr) {
  return reinterpret_cast<const icu4x::NoCalendarFormatter*>(ptr);
}

inline icu4x::NoCalendarFormatter* icu4x::NoCalendarFormatter::FromFFI(icu4x::capi::NoCalendarFormatter* ptr) {
  return reinterpret_cast<icu4x::NoCalendarFormatter*>(ptr);
}

inline void icu4x::NoCalendarFormatter::operator delete(void* ptr) {
  icu4x::capi::icu4x_NoCalendarFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::NoCalendarFormatter*>(ptr));
}


#endif // icu4x_NoCalendarFormatter_HPP
