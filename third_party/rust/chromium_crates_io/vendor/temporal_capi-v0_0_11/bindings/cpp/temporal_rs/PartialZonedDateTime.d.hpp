#ifndef temporal_rs_PartialZonedDateTime_D_HPP
#define temporal_rs_PartialZonedDateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "PartialDate.d.hpp"
#include "PartialTime.d.hpp"

namespace temporal_rs {
namespace capi { struct TimeZone; }
class TimeZone;
struct PartialDate;
struct PartialTime;
}


namespace temporal_rs {
namespace capi {
    struct PartialZonedDateTime {
      temporal_rs::capi::PartialDate date;
      temporal_rs::capi::PartialTime time;
      diplomat::capi::OptionStringView offset;
      const temporal_rs::capi::TimeZone* timezone;
    };

    typedef struct PartialZonedDateTime_option {union { PartialZonedDateTime ok; }; bool is_ok; } PartialZonedDateTime_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct PartialZonedDateTime {
  temporal_rs::PartialDate date;
  temporal_rs::PartialTime time;
  std::optional<std::string_view> offset;
  const temporal_rs::TimeZone* timezone;

  inline temporal_rs::capi::PartialZonedDateTime AsFFI() const;
  inline static temporal_rs::PartialZonedDateTime FromFFI(temporal_rs::capi::PartialZonedDateTime c_struct);
};

} // namespace
#endif // temporal_rs_PartialZonedDateTime_D_HPP
