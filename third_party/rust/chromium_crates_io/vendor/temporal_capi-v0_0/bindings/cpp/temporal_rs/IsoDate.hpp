#ifndef temporal_rs_IsoDate_HPP
#define temporal_rs_IsoDate_HPP

#include "IsoDate.d.hpp"

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


inline temporal_rs::capi::IsoDate temporal_rs::IsoDate::AsFFI() const {
  return temporal_rs::capi::IsoDate {
    /* .year = */ year,
    /* .month = */ month,
    /* .day = */ day,
  };
}

inline temporal_rs::IsoDate temporal_rs::IsoDate::FromFFI(temporal_rs::capi::IsoDate c_struct) {
  return temporal_rs::IsoDate {
    /* .year = */ c_struct.year,
    /* .month = */ c_struct.month,
    /* .day = */ c_struct.day,
  };
}


#endif // temporal_rs_IsoDate_HPP
