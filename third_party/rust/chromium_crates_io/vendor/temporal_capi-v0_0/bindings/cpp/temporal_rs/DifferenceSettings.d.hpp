#ifndef temporal_rs_DifferenceSettings_D_HPP
#define temporal_rs_DifferenceSettings_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "TemporalRoundingMode.d.hpp"
#include "TemporalUnit.d.hpp"

namespace temporal_rs {
class TemporalRoundingMode;
class TemporalUnit;
}


namespace temporal_rs {
namespace capi {
    struct DifferenceSettings {
      temporal_rs::capi::TemporalUnit_option largest_unit;
      temporal_rs::capi::TemporalUnit_option smallest_unit;
      temporal_rs::capi::TemporalRoundingMode_option rounding_mode;
      diplomat::capi::OptionU32 increment;
    };
    
    typedef struct DifferenceSettings_option {union { DifferenceSettings ok; }; bool is_ok; } DifferenceSettings_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct DifferenceSettings {
  std::optional<temporal_rs::TemporalUnit> largest_unit;
  std::optional<temporal_rs::TemporalUnit> smallest_unit;
  std::optional<temporal_rs::TemporalRoundingMode> rounding_mode;
  std::optional<uint32_t> increment;

  inline temporal_rs::capi::DifferenceSettings AsFFI() const;
  inline static temporal_rs::DifferenceSettings FromFFI(temporal_rs::capi::DifferenceSettings c_struct);
};

} // namespace
#endif // temporal_rs_DifferenceSettings_D_HPP
