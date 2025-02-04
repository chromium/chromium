#ifndef icu4x_FixedDecimalRoundingMode_HPP
#define icu4x_FixedDecimalRoundingMode_HPP

#include "FixedDecimalRoundingMode.d.hpp"

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

inline icu4x::capi::FixedDecimalRoundingMode icu4x::FixedDecimalRoundingMode::AsFFI() const {
  return static_cast<icu4x::capi::FixedDecimalRoundingMode>(value);
}

inline icu4x::FixedDecimalRoundingMode icu4x::FixedDecimalRoundingMode::FromFFI(icu4x::capi::FixedDecimalRoundingMode c_enum) {
  switch (c_enum) {
    case icu4x::capi::FixedDecimalRoundingMode_Ceil:
    case icu4x::capi::FixedDecimalRoundingMode_Expand:
    case icu4x::capi::FixedDecimalRoundingMode_Floor:
    case icu4x::capi::FixedDecimalRoundingMode_Trunc:
    case icu4x::capi::FixedDecimalRoundingMode_HalfCeil:
    case icu4x::capi::FixedDecimalRoundingMode_HalfExpand:
    case icu4x::capi::FixedDecimalRoundingMode_HalfFloor:
    case icu4x::capi::FixedDecimalRoundingMode_HalfTrunc:
    case icu4x::capi::FixedDecimalRoundingMode_HalfEven:
      return static_cast<icu4x::FixedDecimalRoundingMode::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_FixedDecimalRoundingMode_HPP
