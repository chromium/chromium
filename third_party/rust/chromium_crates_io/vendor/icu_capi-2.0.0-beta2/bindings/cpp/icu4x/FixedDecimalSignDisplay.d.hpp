#ifndef icu4x_FixedDecimalSignDisplay_D_HPP
#define icu4x_FixedDecimalSignDisplay_D_HPP

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
    enum FixedDecimalSignDisplay {
      FixedDecimalSignDisplay_Auto = 0,
      FixedDecimalSignDisplay_Never = 1,
      FixedDecimalSignDisplay_Always = 2,
      FixedDecimalSignDisplay_ExceptZero = 3,
      FixedDecimalSignDisplay_Negative = 4,
    };
    
    typedef struct FixedDecimalSignDisplay_option {union { FixedDecimalSignDisplay ok; }; bool is_ok; } FixedDecimalSignDisplay_option;
} // namespace capi
} // namespace

namespace icu4x {
class FixedDecimalSignDisplay {
public:
  enum Value {
    Auto = 0,
    Never = 1,
    Always = 2,
    ExceptZero = 3,
    Negative = 4,
  };

  FixedDecimalSignDisplay() = default;
  // Implicit conversions between enum and ::Value
  constexpr FixedDecimalSignDisplay(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::FixedDecimalSignDisplay AsFFI() const;
  inline static icu4x::FixedDecimalSignDisplay FromFFI(icu4x::capi::FixedDecimalSignDisplay c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_FixedDecimalSignDisplay_D_HPP
