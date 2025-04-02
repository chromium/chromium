#ifndef icu4x_FixedDecimalSignedRoundingMode_HPP
#define icu4x_FixedDecimalSignedRoundingMode_HPP

#include "FixedDecimalSignedRoundingMode.d.hpp"

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

inline icu4x::capi::FixedDecimalSignedRoundingMode icu4x::FixedDecimalSignedRoundingMode::AsFFI() const {
  return static_cast<icu4x::capi::FixedDecimalSignedRoundingMode>(value);
}

inline icu4x::FixedDecimalSignedRoundingMode icu4x::FixedDecimalSignedRoundingMode::FromFFI(icu4x::capi::FixedDecimalSignedRoundingMode c_enum) {
  switch (c_enum) {
    case icu4x::capi::FixedDecimalSignedRoundingMode_Expand:
    case icu4x::capi::FixedDecimalSignedRoundingMode_Trunc:
    case icu4x::capi::FixedDecimalSignedRoundingMode_HalfExpand:
    case icu4x::capi::FixedDecimalSignedRoundingMode_HalfTrunc:
    case icu4x::capi::FixedDecimalSignedRoundingMode_HalfEven:
    case icu4x::capi::FixedDecimalSignedRoundingMode_Ceil:
    case icu4x::capi::FixedDecimalSignedRoundingMode_Floor:
    case icu4x::capi::FixedDecimalSignedRoundingMode_HalfCeil:
    case icu4x::capi::FixedDecimalSignedRoundingMode_HalfFloor:
      return static_cast<icu4x::FixedDecimalSignedRoundingMode::Value>(c_enum);
    default:
      abort();
  }
}
#endif // icu4x_FixedDecimalSignedRoundingMode_HPP
