#ifndef temporal_rs_I128Nanoseconds_HPP
#define temporal_rs_I128Nanoseconds_HPP

#include "I128Nanoseconds.d.hpp"

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


inline temporal_rs::capi::I128Nanoseconds temporal_rs::I128Nanoseconds::AsFFI() const {
  return temporal_rs::capi::I128Nanoseconds {
    /* .high = */ high,
    /* .low = */ low,
  };
}

inline temporal_rs::I128Nanoseconds temporal_rs::I128Nanoseconds::FromFFI(temporal_rs::capi::I128Nanoseconds c_struct) {
  return temporal_rs::I128Nanoseconds {
    /* .high = */ c_struct.high,
    /* .low = */ c_struct.low,
  };
}


#endif // temporal_rs_I128Nanoseconds_HPP
