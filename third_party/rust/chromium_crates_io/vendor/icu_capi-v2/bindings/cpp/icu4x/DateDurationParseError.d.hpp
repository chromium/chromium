#ifndef ICU4X_DateDurationParseError_D_HPP
#define ICU4X_DateDurationParseError_D_HPP

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
    enum DateDurationParseError {
      DateDurationParseError_InvalidStructure = 0,
      DateDurationParseError_TimeNotSupported = 1,
      DateDurationParseError_MissingValue = 2,
      DateDurationParseError_DuplicateUnit = 3,
      DateDurationParseError_NumberOverflow = 4,
      DateDurationParseError_PlusNotAllowed = 5,
    };

    typedef struct DateDurationParseError_option {union { DateDurationParseError ok; }; bool is_ok; } DateDurationParseError_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Additional information: [1](https://docs.rs/icu/2.2.0/icu/calendar/error/enum.DateDurationParseError.html)
 */
class DateDurationParseError {
public:
    enum Value {
        InvalidStructure = 0,
        TimeNotSupported = 1,
        MissingValue = 2,
        DuplicateUnit = 3,
        NumberOverflow = 4,
        PlusNotAllowed = 5,
    };

    DateDurationParseError(): value(Value::InvalidStructure) {}

    // Implicit conversions between enum and ::Value
    constexpr DateDurationParseError(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DateDurationParseError AsFFI() const;
    inline static icu4x::DateDurationParseError FromFFI(icu4x::capi::DateDurationParseError c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DateDurationParseError_D_HPP
