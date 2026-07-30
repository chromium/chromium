#ifndef ICU4X_DecimalSignDisplay_D_HPP
#define ICU4X_DecimalSignDisplay_D_HPP

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
    enum DecimalSignDisplay {
      DecimalSignDisplay_Auto = 0,
      DecimalSignDisplay_Never = 1,
      DecimalSignDisplay_Always = 2,
      DecimalSignDisplay_ExceptZero = 3,
      DecimalSignDisplay_Negative = 4,
    };

    typedef struct DecimalSignDisplay_option {union { DecimalSignDisplay ok; }; bool is_ok; } DecimalSignDisplay_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * ECMA-402 compatible sign display preference.
 *
 * See the [Rust documentation for `SignDisplay`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/enum.SignDisplay.html) for more information.
 */
class DecimalSignDisplay {
public:
    enum Value {
        Auto = 0,
        Never = 1,
        Always = 2,
        ExceptZero = 3,
        Negative = 4,
    };

    DecimalSignDisplay(): value(Value::Auto) {}

    // Implicit conversions between enum and ::Value
    constexpr DecimalSignDisplay(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DecimalSignDisplay AsFFI() const;
    inline static icu4x::DecimalSignDisplay FromFFI(icu4x::capi::DecimalSignDisplay c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DecimalSignDisplay_D_HPP
