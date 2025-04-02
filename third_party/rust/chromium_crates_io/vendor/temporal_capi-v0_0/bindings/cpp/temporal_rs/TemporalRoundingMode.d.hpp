#ifndef temporal_rs_TemporalRoundingMode_D_HPP
#define temporal_rs_TemporalRoundingMode_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    enum TemporalRoundingMode {
      TemporalRoundingMode_Ceil = 0,
      TemporalRoundingMode_Floor = 1,
      TemporalRoundingMode_Expand = 2,
      TemporalRoundingMode_Trunc = 3,
      TemporalRoundingMode_HalfCeil = 4,
      TemporalRoundingMode_HalfFloor = 5,
      TemporalRoundingMode_HalfExpand = 6,
      TemporalRoundingMode_HalfTrunc = 7,
      TemporalRoundingMode_HalfEven = 8,
    };
    
    typedef struct TemporalRoundingMode_option {union { TemporalRoundingMode ok; }; bool is_ok; } TemporalRoundingMode_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class TemporalRoundingMode {
public:
  enum Value {
    Ceil = 0,
    Floor = 1,
    Expand = 2,
    Trunc = 3,
    HalfCeil = 4,
    HalfFloor = 5,
    HalfExpand = 6,
    HalfTrunc = 7,
    HalfEven = 8,
  };

  TemporalRoundingMode() = default;
  // Implicit conversions between enum and ::Value
  constexpr TemporalRoundingMode(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline temporal_rs::capi::TemporalRoundingMode AsFFI() const;
  inline static temporal_rs::TemporalRoundingMode FromFFI(temporal_rs::capi::TemporalRoundingMode c_enum);
private:
    Value value;
};

} // namespace
#endif // temporal_rs_TemporalRoundingMode_D_HPP
