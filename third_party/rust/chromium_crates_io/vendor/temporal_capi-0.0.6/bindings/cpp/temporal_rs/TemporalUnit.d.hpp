#ifndef temporal_rs_TemporalUnit_D_HPP
#define temporal_rs_TemporalUnit_D_HPP

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
    enum TemporalUnit {
      TemporalUnit_Auto = 0,
      TemporalUnit_Nanosecond = 1,
      TemporalUnit_Microsecond = 2,
      TemporalUnit_Millisecond = 3,
      TemporalUnit_Second = 4,
      TemporalUnit_Minute = 5,
      TemporalUnit_Hour = 6,
      TemporalUnit_Day = 7,
      TemporalUnit_Week = 8,
      TemporalUnit_Month = 9,
      TemporalUnit_Year = 10,
    };
    
    typedef struct TemporalUnit_option {union { TemporalUnit ok; }; bool is_ok; } TemporalUnit_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class TemporalUnit {
public:
  enum Value {
    Auto = 0,
    Nanosecond = 1,
    Microsecond = 2,
    Millisecond = 3,
    Second = 4,
    Minute = 5,
    Hour = 6,
    Day = 7,
    Week = 8,
    Month = 9,
    Year = 10,
  };

  TemporalUnit() = default;
  // Implicit conversions between enum and ::Value
  constexpr TemporalUnit(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline temporal_rs::capi::TemporalUnit AsFFI() const;
  inline static temporal_rs::TemporalUnit FromFFI(temporal_rs::capi::TemporalUnit c_enum);
private:
    Value value;
};

} // namespace
#endif // temporal_rs_TemporalUnit_D_HPP
