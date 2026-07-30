#ifndef ICU4X_DateMissingFieldsStrategy_D_HPP
#define ICU4X_DateMissingFieldsStrategy_D_HPP

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
    enum DateMissingFieldsStrategy {
      DateMissingFieldsStrategy_Reject = 0,
      DateMissingFieldsStrategy_Ecma = 1,
    };

    typedef struct DateMissingFieldsStrategy_option {union { DateMissingFieldsStrategy ok; }; bool is_ok; } DateMissingFieldsStrategy_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `MissingFieldsStrategy`](https://docs.rs/icu/2.2.0/icu/calendar/options/enum.MissingFieldsStrategy.html) for more information.
 */
class DateMissingFieldsStrategy {
public:
    enum Value {
        Reject = 0,
        Ecma = 1,
    };

    DateMissingFieldsStrategy(): value(Value::Reject) {}

    // Implicit conversions between enum and ::Value
    constexpr DateMissingFieldsStrategy(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DateMissingFieldsStrategy AsFFI() const;
    inline static icu4x::DateMissingFieldsStrategy FromFFI(icu4x::capi::DateMissingFieldsStrategy c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DateMissingFieldsStrategy_D_HPP
