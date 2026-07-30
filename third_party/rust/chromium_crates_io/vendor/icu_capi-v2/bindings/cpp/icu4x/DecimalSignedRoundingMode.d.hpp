#ifndef ICU4X_DecimalSignedRoundingMode_D_HPP
#define ICU4X_DecimalSignedRoundingMode_D_HPP

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
    enum DecimalSignedRoundingMode {
      DecimalSignedRoundingMode_Expand = 0,
      DecimalSignedRoundingMode_Trunc = 1,
      DecimalSignedRoundingMode_HalfExpand = 2,
      DecimalSignedRoundingMode_HalfTrunc = 3,
      DecimalSignedRoundingMode_HalfEven = 4,
      DecimalSignedRoundingMode_Ceil = 5,
      DecimalSignedRoundingMode_Floor = 6,
      DecimalSignedRoundingMode_HalfCeil = 7,
      DecimalSignedRoundingMode_HalfFloor = 8,
    };

    typedef struct DecimalSignedRoundingMode_option {union { DecimalSignedRoundingMode ok; }; bool is_ok; } DecimalSignedRoundingMode_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Mode used in a rounding operation for signed numbers.
 *
 * See the [Rust documentation for `SignedRoundingMode`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/enum.SignedRoundingMode.html) for more information.
 */
class DecimalSignedRoundingMode {
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

    DecimalSignedRoundingMode(): value(Value::Expand) {}

    // Implicit conversions between enum and ::Value
    constexpr DecimalSignedRoundingMode(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DecimalSignedRoundingMode AsFFI() const;
    inline static icu4x::DecimalSignedRoundingMode FromFFI(icu4x::capi::DecimalSignedRoundingMode c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DecimalSignedRoundingMode_D_HPP
