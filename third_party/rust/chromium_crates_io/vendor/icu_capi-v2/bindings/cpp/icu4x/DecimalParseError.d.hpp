#ifndef ICU4X_DecimalParseError_D_HPP
#define ICU4X_DecimalParseError_D_HPP

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
    enum DecimalParseError {
      DecimalParseError_Unknown = 0,
      DecimalParseError_Limit = 1,
      DecimalParseError_Syntax = 2,
    };

    typedef struct DecimalParseError_option {union { DecimalParseError ok; }; bool is_ok; } DecimalParseError_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Additional information: [1](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/enum.ParseError.html)
 */
class DecimalParseError {
public:
    enum Value {
        Unknown = 0,
        Limit = 1,
        Syntax = 2,
    };

    DecimalParseError(): value(Value::Unknown) {}

    // Implicit conversions between enum and ::Value
    constexpr DecimalParseError(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DecimalParseError AsFFI() const;
    inline static icu4x::DecimalParseError FromFFI(icu4x::capi::DecimalParseError c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DecimalParseError_D_HPP
