#ifndef ICU4X_Weekday_D_HPP
#define ICU4X_Weekday_D_HPP

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
    enum Weekday {
      Weekday_Monday = 1,
      Weekday_Tuesday = 2,
      Weekday_Wednesday = 3,
      Weekday_Thursday = 4,
      Weekday_Friday = 5,
      Weekday_Saturday = 6,
      Weekday_Sunday = 7,
    };

    typedef struct Weekday_option {union { Weekday ok; }; bool is_ok; } Weekday_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `Weekday`](https://docs.rs/icu/2.2.0/icu/calendar/types/enum.Weekday.html) for more information.
 */
class Weekday {
public:
    enum Value {
        Monday = 1,
        Tuesday = 2,
        Wednesday = 3,
        Thursday = 4,
        Friday = 5,
        Saturday = 6,
        Sunday = 7,
    };

    Weekday(): value(Value::Monday) {}

    // Implicit conversions between enum and ::Value
    constexpr Weekday(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::Weekday AsFFI() const;
    inline static icu4x::Weekday FromFFI(icu4x::capi::Weekday c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_Weekday_D_HPP
