#ifndef icu4x_IsoWeekday_HPP
#define icu4x_IsoWeekday_HPP

#include "IsoWeekday.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::IsoWeekday icu4x::IsoWeekday::AsFFI() const {
  return static_cast<icu4x::capi::IsoWeekday>(value);
}

inline icu4x::IsoWeekday icu4x::IsoWeekday::FromFFI(icu4x::capi::IsoWeekday c_enum) {
  switch (c_enum) {
    case icu4x::capi::IsoWeekday_Monday:
    case icu4x::capi::IsoWeekday_Tuesday:
    case icu4x::capi::IsoWeekday_Wednesday:
    case icu4x::capi::IsoWeekday_Thursday:
    case icu4x::capi::IsoWeekday_Friday:
    case icu4x::capi::IsoWeekday_Saturday:
    case icu4x::capi::IsoWeekday_Sunday:
      return static_cast<icu4x::IsoWeekday::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_IsoWeekday_HPP
