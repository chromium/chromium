#ifndef temporal_rs_IsoTime_D_HPP
#define temporal_rs_IsoTime_D_HPP

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
    struct IsoTime {
      uint8_t hour;
      uint8_t minute;
      uint8_t second;
      uint16_t millisecond;
      uint16_t microsecond;
      uint16_t nanosecond;
    };

    typedef struct IsoTime_option {union { IsoTime ok; }; bool is_ok; } IsoTime_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct IsoTime {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t millisecond;
  uint16_t microsecond;
  uint16_t nanosecond;

  inline temporal_rs::capi::IsoTime AsFFI() const;
  inline static temporal_rs::IsoTime FromFFI(temporal_rs::capi::IsoTime c_struct);
};

} // namespace
#endif // temporal_rs_IsoTime_D_HPP
