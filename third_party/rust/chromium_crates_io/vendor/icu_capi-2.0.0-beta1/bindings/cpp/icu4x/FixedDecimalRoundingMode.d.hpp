#ifndef icu4x_FixedDecimalRoundingMode_D_HPP
#define icu4x_FixedDecimalRoundingMode_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum FixedDecimalRoundingMode {
      FixedDecimalRoundingMode_Ceil = 0,
      FixedDecimalRoundingMode_Expand = 1,
      FixedDecimalRoundingMode_Floor = 2,
      FixedDecimalRoundingMode_Trunc = 3,
      FixedDecimalRoundingMode_HalfCeil = 4,
      FixedDecimalRoundingMode_HalfExpand = 5,
      FixedDecimalRoundingMode_HalfFloor = 6,
      FixedDecimalRoundingMode_HalfTrunc = 7,
      FixedDecimalRoundingMode_HalfEven = 8,
    };
    
    typedef struct FixedDecimalRoundingMode_option {union { FixedDecimalRoundingMode ok; }; bool is_ok; } FixedDecimalRoundingMode_option;
} // namespace capi
} // namespace

namespace icu4x {
class FixedDecimalRoundingMode {
public:
  enum Value {
    Ceil = 0,
    Expand = 1,
    Floor = 2,
    Trunc = 3,
    HalfCeil = 4,
    HalfExpand = 5,
    HalfFloor = 6,
    HalfTrunc = 7,
    HalfEven = 8,
  };

  FixedDecimalRoundingMode() = default;
  // Implicit conversions between enum and ::Value
  constexpr FixedDecimalRoundingMode(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::FixedDecimalRoundingMode AsFFI() const;
  inline static icu4x::FixedDecimalRoundingMode FromFFI(icu4x::capi::FixedDecimalRoundingMode c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_FixedDecimalRoundingMode_D_HPP
