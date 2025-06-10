#ifndef temporal_rs_IsoDateTime_HPP
#define temporal_rs_IsoDateTime_HPP

#include "IsoDateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "IsoDate.hpp"
#include "IsoTime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace


inline temporal_rs::capi::IsoDateTime temporal_rs::IsoDateTime::AsFFI() const {
  return temporal_rs::capi::IsoDateTime {
    /* .date = */ date.AsFFI(),
    /* .time = */ time.AsFFI(),
  };
}

inline temporal_rs::IsoDateTime temporal_rs::IsoDateTime::FromFFI(temporal_rs::capi::IsoDateTime c_struct) {
  return temporal_rs::IsoDateTime {
    /* .date = */ temporal_rs::IsoDate::FromFFI(c_struct.date),
    /* .time = */ temporal_rs::IsoTime::FromFFI(c_struct.time),
  };
}


#endif // temporal_rs_IsoDateTime_HPP
