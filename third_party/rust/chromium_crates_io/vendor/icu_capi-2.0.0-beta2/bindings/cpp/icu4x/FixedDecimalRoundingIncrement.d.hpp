#ifndef icu4x_FixedDecimalRoundingIncrement_D_HPP
#define icu4x_FixedDecimalRoundingIncrement_D_HPP

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
    enum FixedDecimalRoundingIncrement {
      FixedDecimalRoundingIncrement_MultiplesOf1 = 0,
      FixedDecimalRoundingIncrement_MultiplesOf2 = 1,
      FixedDecimalRoundingIncrement_MultiplesOf5 = 2,
      FixedDecimalRoundingIncrement_MultiplesOf25 = 3,
    };
    
    typedef struct FixedDecimalRoundingIncrement_option {union { FixedDecimalRoundingIncrement ok; }; bool is_ok; } FixedDecimalRoundingIncrement_option;
} // namespace capi
} // namespace

namespace icu4x {
class FixedDecimalRoundingIncrement {
public:
  enum Value {
    MultiplesOf1 = 0,
    MultiplesOf2 = 1,
    MultiplesOf5 = 2,
    MultiplesOf25 = 3,
  };

  FixedDecimalRoundingIncrement() = default;
  // Implicit conversions between enum and ::Value
  constexpr FixedDecimalRoundingIncrement(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline icu4x::capi::FixedDecimalRoundingIncrement AsFFI() const;
  inline static icu4x::FixedDecimalRoundingIncrement FromFFI(icu4x::capi::FixedDecimalRoundingIncrement c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_FixedDecimalRoundingIncrement_D_HPP
