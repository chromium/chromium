#ifndef ICU4X_DateDurationUnit_D_HPP
#define ICU4X_DateDurationUnit_D_HPP

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
    enum DateDurationUnit {
      DateDurationUnit_Years = 0,
      DateDurationUnit_Months = 1,
      DateDurationUnit_Weeks = 2,
      DateDurationUnit_Days = 3,
    };

    typedef struct DateDurationUnit_option {union { DateDurationUnit ok; }; bool is_ok; } DateDurationUnit_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `DateDurationUnit`](https://docs.rs/icu/2.2.0/icu/calendar/options/enum.DateDurationUnit.html) for more information.
 */
class DateDurationUnit {
public:
    enum Value {
        Years = 0,
        Months = 1,
        Weeks = 2,
        Days = 3,
    };

    DateDurationUnit(): value(Value::Years) {}

    // Implicit conversions between enum and ::Value
    constexpr DateDurationUnit(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DateDurationUnit AsFFI() const;
    inline static icu4x::DateDurationUnit FromFFI(icu4x::capi::DateDurationUnit c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DateDurationUnit_D_HPP
