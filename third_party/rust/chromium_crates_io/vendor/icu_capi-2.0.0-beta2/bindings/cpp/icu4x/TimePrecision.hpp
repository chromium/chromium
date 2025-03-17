#ifndef icu4x_TimePrecision_HPP
#define icu4x_TimePrecision_HPP

#include "TimePrecision.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::TimePrecision icu4x::TimePrecision::AsFFI() const {
  return static_cast<icu4x::capi::TimePrecision>(value);
}

inline icu4x::TimePrecision icu4x::TimePrecision::FromFFI(icu4x::capi::TimePrecision c_enum) {
  switch (c_enum) {
    case icu4x::capi::TimePrecision_Hour:
    case icu4x::capi::TimePrecision_Minute:
    case icu4x::capi::TimePrecision_MinuteOptional:
    case icu4x::capi::TimePrecision_Second:
    case icu4x::capi::TimePrecision_Subsecond1:
    case icu4x::capi::TimePrecision_Subsecond2:
    case icu4x::capi::TimePrecision_Subsecond3:
    case icu4x::capi::TimePrecision_Subsecond4:
    case icu4x::capi::TimePrecision_Subsecond5:
    case icu4x::capi::TimePrecision_Subsecond6:
    case icu4x::capi::TimePrecision_Subsecond7:
    case icu4x::capi::TimePrecision_Subsecond8:
    case icu4x::capi::TimePrecision_Subsecond9:
      return static_cast<icu4x::TimePrecision::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_TimePrecision_HPP
