#ifndef icu4x_GregorianDateFormatter_HPP
#define icu4x_GregorianDateFormatter_HPP

#include "GregorianDateFormatter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataProvider.hpp"
#include "DateTimeFormatterLoadError.hpp"
#include "DateTimeLength.hpp"
#include "IsoDate.hpp"
#include "IsoDateTime.hpp"
#include "Locale.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_GregorianDateFormatter_create_with_length_mv1_result {union {icu4x::capi::GregorianDateFormatter* ok; icu4x::capi::DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_GregorianDateFormatter_create_with_length_mv1_result;
    icu4x_GregorianDateFormatter_create_with_length_mv1_result icu4x_GregorianDateFormatter_create_with_length_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DateTimeLength length);
    
    void icu4x_GregorianDateFormatter_format_iso_date_mv1(const icu4x::capi::GregorianDateFormatter* self, const icu4x::capi::IsoDate* value, diplomat::capi::DiplomatWrite* write);
    
    void icu4x_GregorianDateFormatter_format_iso_datetime_mv1(const icu4x::capi::GregorianDateFormatter* self, const icu4x::capi::IsoDateTime* value, diplomat::capi::DiplomatWrite* write);
    
    
    void icu4x_GregorianDateFormatter_destroy_mv1(GregorianDateFormatter* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::GregorianDateFormatter>, icu4x::DateTimeFormatterLoadError> icu4x::GregorianDateFormatter::create_with_length(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::DateTimeLength length) {
  auto result = icu4x::capi::icu4x_GregorianDateFormatter_create_with_length_mv1(provider.AsFFI(),
    locale.AsFFI(),
    length.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::GregorianDateFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Ok<std::unique_ptr<icu4x::GregorianDateFormatter>>(std::unique_ptr<icu4x::GregorianDateFormatter>(icu4x::GregorianDateFormatter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::GregorianDateFormatter>, icu4x::DateTimeFormatterLoadError>(diplomat::Err<icu4x::DateTimeFormatterLoadError>(icu4x::DateTimeFormatterLoadError::FromFFI(result.err)));
}

inline std::string icu4x::GregorianDateFormatter::format_iso_date(const icu4x::IsoDate& value) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_GregorianDateFormatter_format_iso_date_mv1(this->AsFFI(),
    value.AsFFI(),
    &write);
  return output;
}

inline std::string icu4x::GregorianDateFormatter::format_iso_datetime(const icu4x::IsoDateTime& value) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_GregorianDateFormatter_format_iso_datetime_mv1(this->AsFFI(),
    value.AsFFI(),
    &write);
  return output;
}

inline const icu4x::capi::GregorianDateFormatter* icu4x::GregorianDateFormatter::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::GregorianDateFormatter*>(this);
}

inline icu4x::capi::GregorianDateFormatter* icu4x::GregorianDateFormatter::AsFFI() {
  return reinterpret_cast<icu4x::capi::GregorianDateFormatter*>(this);
}

inline const icu4x::GregorianDateFormatter* icu4x::GregorianDateFormatter::FromFFI(const icu4x::capi::GregorianDateFormatter* ptr) {
  return reinterpret_cast<const icu4x::GregorianDateFormatter*>(ptr);
}

inline icu4x::GregorianDateFormatter* icu4x::GregorianDateFormatter::FromFFI(icu4x::capi::GregorianDateFormatter* ptr) {
  return reinterpret_cast<icu4x::GregorianDateFormatter*>(ptr);
}

inline void icu4x::GregorianDateFormatter::operator delete(void* ptr) {
  icu4x::capi::icu4x_GregorianDateFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::GregorianDateFormatter*>(ptr));
}


#endif // icu4x_GregorianDateFormatter_HPP
