#ifndef icu4x_FixedDecimalSignedRoundingMode_D_HPP
#define icu4x_FixedDecimalSignedRoundingMode_D_HPP

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
    enum FixedDecimalSignedRoundingMode {
      FixedDecimalSignedRoundingMode_Expand = 0,
      FixedDecimalSignedRoundingMode_Trunc = 1,
      FixedDecimalSignedRoundingMode_HalfExpand = 2,
      FixedDecimalSignedRoundingMode_HalfTrunc = 3,
      FixedDecimalSignedRoundingMode_HalfEven = 4,
      FixedDecimalSignedRoundingMode_Ceil = 5,
      FixedDecimalSignedRoundingMode_Floor = 6,
      FixedDecimalSignedRoundingMode_HalfCeil = 7,
      FixedDecimalSignedRoundingMode_HalfFloor = 8,
    };
    
    typedef struct FixedDecimalSignedRoundingMode_option {union { FixedDecimalSignedRoundingMode ok; }; bool is_ok; } FixedDecimalSignedRoundingMode_option;
} // namespace capi
} // namespace

namespace icu4x {
class FixedDecimalSignedRoundingMode {
public:
  enum Value {
    Expand = 0,
    Trunc = 1,
    HalfExpand = 2,
    HalfTrunc = 3,
    HalfEven = 4,
    Ceil = 5,
    Floor = 6,
    HalfCeil = 7,
    HalfFloor = 8,
  };

  FixedDecimalSignedRoundingMode() = default;
  // Implicit conversions between enum and ::Value
  constexpr FixedDecimalSignedRoundingMode(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::FixedDecimalSignedRoundingMode AsFFI() const;
  inline static icu4x::FixedDecimalSignedRoundingMode FromFFI(icu4x::capi::FixedDecimalSignedRoundingMode c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_FixedDecimalSignedRoundingMode_D_HPP
