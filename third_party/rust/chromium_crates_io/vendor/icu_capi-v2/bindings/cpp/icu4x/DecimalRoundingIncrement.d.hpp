#ifndef ICU4X_DecimalRoundingIncrement_D_HPP
#define ICU4X_DecimalRoundingIncrement_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    enum DecimalRoundingIncrement {
      DecimalRoundingIncrement_MultiplesOf1 = 0,
      DecimalRoundingIncrement_MultiplesOf2 = 1,
      DecimalRoundingIncrement_MultiplesOf5 = 2,
      DecimalRoundingIncrement_MultiplesOf25 = 3,
    };

    typedef struct DecimalRoundingIncrement_option {union { DecimalRoundingIncrement ok; }; bool is_ok; } DecimalRoundingIncrement_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Increment used in a rounding operation.
 *
 * See the [Rust documentation for `RoundingIncrement`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/enum.RoundingIncrement.html) for more information.
 */
class DecimalRoundingIncrement {
public:
    enum Value {
        MultiplesOf1 = 0,
        MultiplesOf2 = 1,
        MultiplesOf5 = 2,
        MultiplesOf25 = 3,
    };

    DecimalRoundingIncrement(): value(Value::MultiplesOf1) {}

    // Implicit conversions between enum and ::Value
    constexpr DecimalRoundingIncrement(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DecimalRoundingIncrement AsFFI() const;
    inline static icu4x::DecimalRoundingIncrement FromFFI(icu4x::capi::DecimalRoundingIncrement c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DecimalRoundingIncrement_D_HPP
