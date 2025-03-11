#ifndef icu4x_WeekRelativeUnit_HPP
#define icu4x_WeekRelativeUnit_HPP

#include "WeekRelativeUnit.d.hpp"

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

inline icu4x::capi::WeekRelativeUnit icu4x::WeekRelativeUnit::AsFFI() const {
  return static_cast<icu4x::capi::WeekRelativeUnit>(value);
}

inline icu4x::WeekRelativeUnit icu4x::WeekRelativeUnit::FromFFI(icu4x::capi::WeekRelativeUnit c_enum) {
  switch (c_enum) {
    case icu4x::capi::WeekRelativeUnit_Previous:
    case icu4x::capi::WeekRelativeUnit_Current:
    case icu4x::capi::WeekRelativeUnit_Next:
      return static_cast<icu4x::WeekRelativeUnit::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_WeekRelativeUnit_HPP
