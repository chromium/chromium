#ifndef temporal_rs_TemporalUnit_HPP
#define temporal_rs_TemporalUnit_HPP

#include "TemporalUnit.d.hpp"

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

inline temporal_rs::capi::TemporalUnit temporal_rs::TemporalUnit::AsFFI() const {
  return static_cast<temporal_rs::capi::TemporalUnit>(value);
}

inline temporal_rs::TemporalUnit temporal_rs::TemporalUnit::FromFFI(temporal_rs::capi::TemporalUnit c_enum) {
  switch (c_enum) {
    case temporal_rs::capi::TemporalUnit_Auto:
    case temporal_rs::capi::TemporalUnit_Nanosecond:
    case temporal_rs::capi::TemporalUnit_Microsecond:
    case temporal_rs::capi::TemporalUnit_Millisecond:
    case temporal_rs::capi::TemporalUnit_Second:
    case temporal_rs::capi::TemporalUnit_Minute:
    case temporal_rs::capi::TemporalUnit_Hour:
    case temporal_rs::capi::TemporalUnit_Day:
    case temporal_rs::capi::TemporalUnit_Week:
    case temporal_rs::capi::TemporalUnit_Month:
    case temporal_rs::capi::TemporalUnit_Year:
      return static_cast<temporal_rs::TemporalUnit::Value>(c_enum);
    default:
      abort();
  }
}
#endif // temporal_rs_TemporalUnit_HPP
