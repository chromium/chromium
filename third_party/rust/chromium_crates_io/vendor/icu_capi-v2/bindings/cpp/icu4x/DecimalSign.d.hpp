#ifndef ICU4X_DecimalSign_D_HPP
#define ICU4X_DecimalSign_D_HPP

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
    enum DecimalSign {
      DecimalSign_None = 0,
      DecimalSign_Negative = 1,
      DecimalSign_Positive = 2,
    };

    typedef struct DecimalSign_option {union { DecimalSign ok; }; bool is_ok; } DecimalSign_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * The sign of a Decimal, as shown in formatting.
 *
 * See the [Rust documentation for `Sign`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/enum.Sign.html) for more information.
 */
class DecimalSign {
public:
    enum Value {
        /**
         * No sign (implicitly positive, e.g., 1729).
         */
        None = 0,
        /**
         * A negative sign, e.g., -1729.
         */
        Negative = 1,
        /**
         * An explicit positive sign, e.g., +1729.
         */
        Positive = 2,
    };

    DecimalSign(): value(Value::None) {}

    // Implicit conversions between enum and ::Value
    constexpr DecimalSign(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DecimalSign AsFFI() const;
    inline static icu4x::DecimalSign FromFFI(icu4x::capi::DecimalSign c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DecimalSign_D_HPP
