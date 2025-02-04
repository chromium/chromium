#ifndef icu4x_AnyCalendarKind_HPP
#define icu4x_AnyCalendarKind_HPP

#include "AnyCalendarKind.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "Locale.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_AnyCalendarKind_get_for_locale_mv1_result {union {icu4x::capi::AnyCalendarKind ok; }; bool is_ok;} icu4x_AnyCalendarKind_get_for_locale_mv1_result;
    icu4x_AnyCalendarKind_get_for_locale_mv1_result icu4x_AnyCalendarKind_get_for_locale_mv1(const icu4x::capi::Locale* locale);
    
    typedef struct icu4x_AnyCalendarKind_get_for_bcp47_mv1_result {union {icu4x::capi::AnyCalendarKind ok; }; bool is_ok;} icu4x_AnyCalendarKind_get_for_bcp47_mv1_result;
    icu4x_AnyCalendarKind_get_for_bcp47_mv1_result icu4x_AnyCalendarKind_get_for_bcp47_mv1(diplomat::capi::DiplomatStringView s);
    
    void icu4x_AnyCalendarKind_bcp47_mv1(icu4x::capi::AnyCalendarKind self, diplomat::capi::DiplomatWrite* write);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::AnyCalendarKind icu4x::AnyCalendarKind::AsFFI() const {
  return static_cast<icu4x::capi::AnyCalendarKind>(value);
}

inline icu4x::AnyCalendarKind icu4x::AnyCalendarKind::FromFFI(icu4x::capi::AnyCalendarKind c_enum) {
  switch (c_enum) {
    case icu4x::capi::AnyCalendarKind_Iso:
    case icu4x::capi::AnyCalendarKind_Gregorian:
    case icu4x::capi::AnyCalendarKind_Buddhist:
    case icu4x::capi::AnyCalendarKind_Japanese:
    case icu4x::capi::AnyCalendarKind_JapaneseExtended:
    case icu4x::capi::AnyCalendarKind_Ethiopian:
    case icu4x::capi::AnyCalendarKind_EthiopianAmeteAlem:
    case icu4x::capi::AnyCalendarKind_Indian:
    case icu4x::capi::AnyCalendarKind_Coptic:
    case icu4x::capi::AnyCalendarKind_Dangi:
    case icu4x::capi::AnyCalendarKind_Chinese:
    case icu4x::capi::AnyCalendarKind_Hebrew:
    case icu4x::capi::AnyCalendarKind_IslamicCivil:
    case icu4x::capi::AnyCalendarKind_IslamicObservational:
    case icu4x::capi::AnyCalendarKind_IslamicTabular:
    case icu4x::capi::AnyCalendarKind_IslamicUmmAlQura:
    case icu4x::capi::AnyCalendarKind_Persian:
    case icu4x::capi::AnyCalendarKind_Roc:
      return static_cast<icu4x::AnyCalendarKind::Value>(c_enum);
    default:
      abort();
  }
}

inline std::optional<icu4x::AnyCalendarKind> icu4x::AnyCalendarKind::get_for_locale(const icu4x::Locale& locale) {
  auto result = icu4x::capi::icu4x_AnyCalendarKind_get_for_locale_mv1(locale.AsFFI());
  return result.is_ok ? std::optional<icu4x::AnyCalendarKind>(icu4x::AnyCalendarKind::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::AnyCalendarKind> icu4x::AnyCalendarKind::get_for_bcp47(std::string_view s) {
  auto result = icu4x::capi::icu4x_AnyCalendarKind_get_for_bcp47_mv1({s.data(), s.size()});
  return result.is_ok ? std::optional<icu4x::AnyCalendarKind>(icu4x::AnyCalendarKind::FromFFI(result.ok)) : std::nullopt;
}

inline std::string icu4x::AnyCalendarKind::bcp47() {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_AnyCalendarKind_bcp47_mv1(this->AsFFI(),
    &write);
  return output;
}
#endif // icu4x_AnyCalendarKind_HPP
