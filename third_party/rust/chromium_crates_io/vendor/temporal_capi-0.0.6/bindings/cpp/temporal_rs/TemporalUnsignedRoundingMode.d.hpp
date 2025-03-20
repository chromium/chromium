#ifndef temporal_rs_TemporalUnsignedRoundingMode_D_HPP
#define temporal_rs_TemporalUnsignedRoundingMode_D_HPP

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
    enum TemporalUnsignedRoundingMode {
      TemporalUnsignedRoundingMode_Infinity = 0,
      TemporalUnsignedRoundingMode_Zero = 1,
      TemporalUnsignedRoundingMode_HalfInfinity = 2,
      TemporalUnsignedRoundingMode_HalfZero = 3,
      TemporalUnsignedRoundingMode_HalfEven = 4,
    };
    
    typedef struct TemporalUnsignedRoundingMode_option {union { TemporalUnsignedRoundingMode ok; }; bool is_ok; } TemporalUnsignedRoundingMode_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class TemporalUnsignedRoundingMode {
public:
  enum Value {
    Infinity = 0,
    Zero = 1,
    HalfInfinity = 2,
    HalfZero = 3,
    HalfEven = 4,
  };

  TemporalUnsignedRoundingMode() = default;
  // Implicit conversions between enum and ::Value
  constexpr TemporalUnsignedRoundingMode(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline temporal_rs::capi::TemporalUnsignedRoundingMode AsFFI() const;
  inline static temporal_rs::TemporalUnsignedRoundingMode FromFFI(temporal_rs::capi::TemporalUnsignedRoundingMode c_enum);
private:
    Value value;
};

} // namespace
#endif // temporal_rs_TemporalUnsignedRoundingMode_D_HPP
