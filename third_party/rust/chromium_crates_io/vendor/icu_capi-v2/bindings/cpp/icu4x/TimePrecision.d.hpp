#ifndef icu4x_TimePrecision_D_HPP
#define icu4x_TimePrecision_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum TimePrecision {
      TimePrecision_Hour = 0,
      TimePrecision_Minute = 1,
      TimePrecision_MinuteOptional = 2,
      TimePrecision_Second = 3,
      TimePrecision_Subsecond1 = 4,
      TimePrecision_Subsecond2 = 5,
      TimePrecision_Subsecond3 = 6,
      TimePrecision_Subsecond4 = 7,
      TimePrecision_Subsecond5 = 8,
      TimePrecision_Subsecond6 = 9,
      TimePrecision_Subsecond7 = 10,
      TimePrecision_Subsecond8 = 11,
      TimePrecision_Subsecond9 = 12,
    };
    
    typedef struct TimePrecision_option {union { TimePrecision ok; }; bool is_ok; } TimePrecision_option;
} // namespace capi
} // namespace

namespace icu4x {
class TimePrecision {
public:
  enum Value {
    Hour = 0,
    Minute = 1,
    MinuteOptional = 2,
    Second = 3,
    Subsecond1 = 4,
    Subsecond2 = 5,
    Subsecond3 = 6,
    Subsecond4 = 7,
    Subsecond5 = 8,
    Subsecond6 = 9,
    Subsecond7 = 10,
    Subsecond8 = 11,
    Subsecond9 = 12,
  };

  TimePrecision() = default;
  // Implicit conversions between enum and ::Value
  constexpr TimePrecision(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::TimePrecision AsFFI() const;
  inline static icu4x::TimePrecision FromFFI(icu4x::capi::TimePrecision c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_TimePrecision_D_HPP
