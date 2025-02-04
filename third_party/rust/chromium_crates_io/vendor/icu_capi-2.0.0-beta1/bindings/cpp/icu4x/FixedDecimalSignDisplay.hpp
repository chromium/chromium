#ifndef icu4x_FixedDecimalSignDisplay_HPP
#define icu4x_FixedDecimalSignDisplay_HPP

#include "FixedDecimalSignDisplay.d.hpp"

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

inline icu4x::capi::FixedDecimalSignDisplay icu4x::FixedDecimalSignDisplay::AsFFI() const {
  return static_cast<icu4x::capi::FixedDecimalSignDisplay>(value);
}

inline icu4x::FixedDecimalSignDisplay icu4x::FixedDecimalSignDisplay::FromFFI(icu4x::capi::FixedDecimalSignDisplay c_enum) {
  switch (c_enum) {
    case icu4x::capi::FixedDecimalSignDisplay_Auto:
    case icu4x::capi::FixedDecimalSignDisplay_Never:
    case icu4x::capi::FixedDecimalSignDisplay_Always:
    case icu4x::capi::FixedDecimalSignDisplay_ExceptZero:
    case icu4x::capi::FixedDecimalSignDisplay_Negative:
      return static_cast<icu4x::FixedDecimalSignDisplay::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_FixedDecimalSignDisplay_HPP
