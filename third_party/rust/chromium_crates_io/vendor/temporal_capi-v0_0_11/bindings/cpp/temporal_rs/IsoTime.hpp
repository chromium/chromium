#ifndef temporal_rs_IsoTime_HPP
#define temporal_rs_IsoTime_HPP

#include "IsoTime.d.hpp"

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


inline temporal_rs::capi::IsoTime temporal_rs::IsoTime::AsFFI() const {
  return temporal_rs::capi::IsoTime {
    /* .hour = */ hour,
    /* .minute = */ minute,
    /* .second = */ second,
    /* .millisecond = */ millisecond,
    /* .microsecond = */ microsecond,
    /* .nanosecond = */ nanosecond,
  };
}

inline temporal_rs::IsoTime temporal_rs::IsoTime::FromFFI(temporal_rs::capi::IsoTime c_struct) {
  return temporal_rs::IsoTime {
    /* .hour = */ c_struct.hour,
    /* .minute = */ c_struct.minute,
    /* .second = */ c_struct.second,
    /* .millisecond = */ c_struct.millisecond,
    /* .microsecond = */ c_struct.microsecond,
    /* .nanosecond = */ c_struct.nanosecond,
  };
}


#endif // temporal_rs_IsoTime_HPP
