#ifndef temporal_rs_TemporalRoundingMode_HPP
#define temporal_rs_TemporalRoundingMode_HPP

#include "TemporalRoundingMode.d.hpp"

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

inline temporal_rs::capi::TemporalRoundingMode temporal_rs::TemporalRoundingMode::AsFFI() const {
  return static_cast<temporal_rs::capi::TemporalRoundingMode>(value);
}

inline temporal_rs::TemporalRoundingMode temporal_rs::TemporalRoundingMode::FromFFI(temporal_rs::capi::TemporalRoundingMode c_enum) {
  switch (c_enum) {
    case temporal_rs::capi::TemporalRoundingMode_Ceil:
    case temporal_rs::capi::TemporalRoundingMode_Floor:
    case temporal_rs::capi::TemporalRoundingMode_Expand:
    case temporal_rs::capi::TemporalRoundingMode_Trunc:
    case temporal_rs::capi::TemporalRoundingMode_HalfCeil:
    case temporal_rs::capi::TemporalRoundingMode_HalfFloor:
    case temporal_rs::capi::TemporalRoundingMode_HalfExpand:
    case temporal_rs::capi::TemporalRoundingMode_HalfTrunc:
    case temporal_rs::capi::TemporalRoundingMode_HalfEven:
      return static_cast<temporal_rs::TemporalRoundingMode::Value>(c_enum);
    default:
      abort();
  }
}
#endif // temporal_rs_TemporalRoundingMode_HPP
