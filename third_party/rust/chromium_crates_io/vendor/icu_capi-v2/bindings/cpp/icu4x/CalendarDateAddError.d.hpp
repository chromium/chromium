#ifndef ICU4X_CalendarDateAddError_D_HPP
#define ICU4X_CalendarDateAddError_D_HPP

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
    enum CalendarDateAddError {
      CalendarDateAddError_Unknown = 0,
      CalendarDateAddError_InvalidDay = 1,
      CalendarDateAddError_MonthNotInYear = 2,
      CalendarDateAddError_Overflow = 3,
    };

    typedef struct CalendarDateAddError_option {union { CalendarDateAddError ok; }; bool is_ok; } CalendarDateAddError_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Additional information: [1](https://docs.rs/icu/2.2.0/icu/calendar/error/enum.DateAddError.html)
 */
class CalendarDateAddError {
public:
    enum Value {
        Unknown = 0,
        InvalidDay = 1,
        MonthNotInYear = 2,
        Overflow = 3,
    };

    CalendarDateAddError(): value(Value::Unknown) {}

    // Implicit conversions between enum and ::Value
    constexpr CalendarDateAddError(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::CalendarDateAddError AsFFI() const;
    inline static icu4x::CalendarDateAddError FromFFI(icu4x::capi::CalendarDateAddError c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_CalendarDateAddError_D_HPP
