#ifndef temporal_rs_ToStringRoundingOptions_D_HPP
#define temporal_rs_ToStringRoundingOptions_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "Precision.d.hpp"
#include "TemporalRoundingMode.d.hpp"
#include "TemporalUnit.d.hpp"

namespace temporal_rs {
struct Precision;
class TemporalRoundingMode;
class TemporalUnit;
}


namespace temporal_rs {
namespace capi {
    struct ToStringRoundingOptions {
      temporal_rs::capi::Precision precision;
      temporal_rs::capi::TemporalUnit_option smallest_unit;
      temporal_rs::capi::TemporalRoundingMode_option rounding_mode;
    };
    
    typedef struct ToStringRoundingOptions_option {union { ToStringRoundingOptions ok; }; bool is_ok; } ToStringRoundingOptions_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct ToStringRoundingOptions {
  temporal_rs::Precision precision;
  std::optional<temporal_rs::TemporalUnit> smallest_unit;
  std::optional<temporal_rs::TemporalRoundingMode> rounding_mode;

  inline temporal_rs::capi::ToStringRoundingOptions AsFFI() const;
  inline static temporal_rs::ToStringRoundingOptions FromFFI(temporal_rs::capi::ToStringRoundingOptions c_struct);
};

} // namespace
#endif // temporal_rs_ToStringRoundingOptions_D_HPP
