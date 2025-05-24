#ifndef temporal_rs_DurationOverflow_HPP
#define temporal_rs_DurationOverflow_HPP

#include "DurationOverflow.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::capi::DurationOverflow temporal_rs::DurationOverflow::AsFFI() const {
  return static_cast<temporal_rs::capi::DurationOverflow>(value);
}

inline temporal_rs::DurationOverflow temporal_rs::DurationOverflow::FromFFI(temporal_rs::capi::DurationOverflow c_enum) {
  switch (c_enum) {
    case temporal_rs::capi::DurationOverflow_Constrain:
    case temporal_rs::capi::DurationOverflow_Balance:
      return static_cast<temporal_rs::DurationOverflow::Value>(c_enum);
    default:
      std::abort();
  }
}
#endif // temporal_rs_DurationOverflow_HPP
