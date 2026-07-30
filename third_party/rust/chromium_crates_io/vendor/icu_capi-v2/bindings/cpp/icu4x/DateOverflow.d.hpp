#ifndef ICU4X_DateOverflow_D_HPP
#define ICU4X_DateOverflow_D_HPP

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
    enum DateOverflow {
      DateOverflow_Constrain = 0,
      DateOverflow_Reject = 1,
    };

    typedef struct DateOverflow_option {union { DateOverflow ok; }; bool is_ok; } DateOverflow_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `Overflow`](https://docs.rs/icu/2.2.0/icu/calendar/options/enum.Overflow.html) for more information.
 */
class DateOverflow {
public:
    enum Value {
        Constrain = 0,
        Reject = 1,
    };

    DateOverflow(): value(Value::Constrain) {}

    // Implicit conversions between enum and ::Value
    constexpr DateOverflow(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DateOverflow AsFFI() const;
    inline static icu4x::DateOverflow FromFFI(icu4x::capi::DateOverflow c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DateOverflow_D_HPP
