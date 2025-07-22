#ifndef temporal_rs_PartialTime_HPP
#define temporal_rs_PartialTime_HPP

#include "PartialTime.d.hpp"

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


inline temporal_rs::capi::PartialTime temporal_rs::PartialTime::AsFFI() const {
  return temporal_rs::capi::PartialTime {
    /* .hour = */ hour.has_value() ? (diplomat::capi::OptionU8{ { hour.value() }, true }) : (diplomat::capi::OptionU8{ {}, false }),
    /* .minute = */ minute.has_value() ? (diplomat::capi::OptionU8{ { minute.value() }, true }) : (diplomat::capi::OptionU8{ {}, false }),
    /* .second = */ second.has_value() ? (diplomat::capi::OptionU8{ { second.value() }, true }) : (diplomat::capi::OptionU8{ {}, false }),
    /* .millisecond = */ millisecond.has_value() ? (diplomat::capi::OptionU16{ { millisecond.value() }, true }) : (diplomat::capi::OptionU16{ {}, false }),
    /* .microsecond = */ microsecond.has_value() ? (diplomat::capi::OptionU16{ { microsecond.value() }, true }) : (diplomat::capi::OptionU16{ {}, false }),
    /* .nanosecond = */ nanosecond.has_value() ? (diplomat::capi::OptionU16{ { nanosecond.value() }, true }) : (diplomat::capi::OptionU16{ {}, false }),
  };
}

inline temporal_rs::PartialTime temporal_rs::PartialTime::FromFFI(temporal_rs::capi::PartialTime c_struct) {
  return temporal_rs::PartialTime {
    /* .hour = */ c_struct.hour.is_ok ? std::optional(c_struct.hour.ok) : std::nullopt,
    /* .minute = */ c_struct.minute.is_ok ? std::optional(c_struct.minute.ok) : std::nullopt,
    /* .second = */ c_struct.second.is_ok ? std::optional(c_struct.second.ok) : std::nullopt,
    /* .millisecond = */ c_struct.millisecond.is_ok ? std::optional(c_struct.millisecond.ok) : std::nullopt,
    /* .microsecond = */ c_struct.microsecond.is_ok ? std::optional(c_struct.microsecond.ok) : std::nullopt,
    /* .nanosecond = */ c_struct.nanosecond.is_ok ? std::optional(c_struct.nanosecond.ok) : std::nullopt,
  };
}


#endif // temporal_rs_PartialTime_HPP
