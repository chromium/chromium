#ifndef temporal_rs_AnyCalendarKind_HPP
#define temporal_rs_AnyCalendarKind_HPP

#include "AnyCalendarKind.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {
    
    typedef struct temporal_rs_AnyCalendarKind_get_for_bcp47_string_result {union {temporal_rs::capi::AnyCalendarKind ok; }; bool is_ok;} temporal_rs_AnyCalendarKind_get_for_bcp47_string_result;
    temporal_rs_AnyCalendarKind_get_for_bcp47_string_result temporal_rs_AnyCalendarKind_get_for_bcp47_string(diplomat::capi::DiplomatStringView s);
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::capi::AnyCalendarKind temporal_rs::AnyCalendarKind::AsFFI() const {
  return static_cast<temporal_rs::capi::AnyCalendarKind>(value);
}

inline temporal_rs::AnyCalendarKind temporal_rs::AnyCalendarKind::FromFFI(temporal_rs::capi::AnyCalendarKind c_enum) {
  switch (c_enum) {
    case temporal_rs::capi::AnyCalendarKind_Buddhist:
    case temporal_rs::capi::AnyCalendarKind_Chinese:
    case temporal_rs::capi::AnyCalendarKind_Coptic:
    case temporal_rs::capi::AnyCalendarKind_Dangi:
    case temporal_rs::capi::AnyCalendarKind_Ethiopian:
    case temporal_rs::capi::AnyCalendarKind_EthiopianAmeteAlem:
    case temporal_rs::capi::AnyCalendarKind_Gregorian:
    case temporal_rs::capi::AnyCalendarKind_Hebrew:
    case temporal_rs::capi::AnyCalendarKind_Indian:
    case temporal_rs::capi::AnyCalendarKind_IslamicCivil:
    case temporal_rs::capi::AnyCalendarKind_IslamicObservational:
    case temporal_rs::capi::AnyCalendarKind_IslamicTabular:
    case temporal_rs::capi::AnyCalendarKind_IslamicUmmAlQura:
    case temporal_rs::capi::AnyCalendarKind_Iso:
    case temporal_rs::capi::AnyCalendarKind_Japanese:
    case temporal_rs::capi::AnyCalendarKind_JapaneseExtended:
    case temporal_rs::capi::AnyCalendarKind_Persian:
    case temporal_rs::capi::AnyCalendarKind_Roc:
      return static_cast<temporal_rs::AnyCalendarKind::Value>(c_enum);
    default:
      abort();
  }
}

inline std::optional<temporal_rs::AnyCalendarKind> temporal_rs::AnyCalendarKind::get_for_bcp47_string(std::string_view s) {
  auto result = temporal_rs::capi::temporal_rs_AnyCalendarKind_get_for_bcp47_string({s.data(), s.size()});
  return result.is_ok ? std::optional<temporal_rs::AnyCalendarKind>(temporal_rs::AnyCalendarKind::FromFFI(result.ok)) : std::nullopt;
}
#endif // temporal_rs_AnyCalendarKind_HPP
