#ifndef ICU4X_DateTimeWriteError_D_HPP
#define ICU4X_DateTimeWriteError_D_HPP

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
    enum DateTimeWriteError {
      DateTimeWriteError_Unknown = 0,
      DateTimeWriteError_MissingTimeZoneVariant = 1,
    };

    typedef struct DateTimeWriteError_option {union { DateTimeWriteError ok; }; bool is_ok; } DateTimeWriteError_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An error when formatting a datetime.
 *
 * Currently never returned by any API.
 *
 * Additional information: [1](https://docs.rs/icu/2.2.0/icu/datetime/unchecked/enum.FormattedDateTimeUncheckedError.html)
 */
class DateTimeWriteError {
public:
    enum Value {
        Unknown = 0,
        /**
         * Unused
         */
        MissingTimeZoneVariant = 1,
    };

    DateTimeWriteError(): value(Value::Unknown) {}

    // Implicit conversions between enum and ::Value
    constexpr DateTimeWriteError(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DateTimeWriteError AsFFI() const;
    inline static icu4x::DateTimeWriteError FromFFI(icu4x::capi::DateTimeWriteError c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DateTimeWriteError_D_HPP
