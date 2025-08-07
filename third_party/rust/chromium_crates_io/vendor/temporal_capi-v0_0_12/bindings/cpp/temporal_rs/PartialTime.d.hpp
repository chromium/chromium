#ifndef temporal_rs_PartialTime_D_HPP
#define temporal_rs_PartialTime_D_HPP

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
    struct PartialTime {
      diplomat::capi::OptionU8 hour;
      diplomat::capi::OptionU8 minute;
      diplomat::capi::OptionU8 second;
      diplomat::capi::OptionU16 millisecond;
      diplomat::capi::OptionU16 microsecond;
      diplomat::capi::OptionU16 nanosecond;
    };

    typedef struct PartialTime_option {union { PartialTime ok; }; bool is_ok; } PartialTime_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct PartialTime {
  std::optional<uint8_t> hour;
  std::optional<uint8_t> minute;
  std::optional<uint8_t> second;
  std::optional<uint16_t> millisecond;
  std::optional<uint16_t> microsecond;
  std::optional<uint16_t> nanosecond;

  inline temporal_rs::capi::PartialTime AsFFI() const;
  inline static temporal_rs::PartialTime FromFFI(temporal_rs::capi::PartialTime c_struct);
};

} // namespace
#endif // temporal_rs_PartialTime_D_HPP
