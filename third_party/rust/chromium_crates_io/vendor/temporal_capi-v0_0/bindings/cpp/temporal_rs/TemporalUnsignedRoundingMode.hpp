#ifndef temporal_rs_TemporalUnsignedRoundingMode_HPP
#define temporal_rs_TemporalUnsignedRoundingMode_HPP

#include "TemporalUnsignedRoundingMode.d.hpp"

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
    
    
    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::capi::TemporalUnsignedRoundingMode temporal_rs::TemporalUnsignedRoundingMode::AsFFI() const {
  return static_cast<temporal_rs::capi::TemporalUnsignedRoundingMode>(value);
}

inline temporal_rs::TemporalUnsignedRoundingMode temporal_rs::TemporalUnsignedRoundingMode::FromFFI(temporal_rs::capi::TemporalUnsignedRoundingMode c_enum) {
  switch (c_enum) {
    case temporal_rs::capi::TemporalUnsignedRoundingMode_Infinity:
    case temporal_rs::capi::TemporalUnsignedRoundingMode_Zero:
    case temporal_rs::capi::TemporalUnsignedRoundingMode_HalfInfinity:
    case temporal_rs::capi::TemporalUnsignedRoundingMode_HalfZero:
    case temporal_rs::capi::TemporalUnsignedRoundingMode_HalfEven:
      return static_cast<temporal_rs::TemporalUnsignedRoundingMode::Value>(c_enum);
    default:
      abort();
  }
}
#endif // temporal_rs_TemporalUnsignedRoundingMode_HPP
