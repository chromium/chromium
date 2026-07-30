#ifndef ICU4X_CalendarDateFromFieldsError_D_HPP
#define ICU4X_CalendarDateFromFieldsError_D_HPP

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
    enum CalendarDateFromFieldsError {
      CalendarDateFromFieldsError_Unknown = 0,
      CalendarDateFromFieldsError_InvalidDay = 1,
      CalendarDateFromFieldsError_InvalidOrdinalMonth = 9,
      CalendarDateFromFieldsError_InvalidEra = 2,
      CalendarDateFromFieldsError_MonthCodeInvalidSyntax = 3,
      CalendarDateFromFieldsError_MonthNotInCalendar = 4,
      CalendarDateFromFieldsError_MonthNotInYear = 5,
      CalendarDateFromFieldsError_InconsistentYear = 6,
      CalendarDateFromFieldsError_InconsistentMonth = 7,
      CalendarDateFromFieldsError_NotEnoughFields = 8,
      CalendarDateFromFieldsError_TooManyFields = 10,
      CalendarDateFromFieldsError_Overflow = 11,
    };

    typedef struct CalendarDateFromFieldsError_option {union { CalendarDateFromFieldsError ok; }; bool is_ok; } CalendarDateFromFieldsError_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Additional information: [1](https://docs.rs/icu/2.2.0/icu/calendar/error/enum.DateFromFieldsError.html)
 */
class CalendarDateFromFieldsError {
public:
    enum Value {
        Unknown = 0,
        InvalidDay = 1,
        InvalidOrdinalMonth = 9,
        InvalidEra = 2,
        MonthCodeInvalidSyntax = 3,
        MonthNotInCalendar = 4,
        MonthNotInYear = 5,
        InconsistentYear = 6,
        InconsistentMonth = 7,
        NotEnoughFields = 8,
        TooManyFields = 10,
        Overflow = 11,
    };

    CalendarDateFromFieldsError(): value(Value::Unknown) {}

    // Implicit conversions between enum and ::Value
    constexpr CalendarDateFromFieldsError(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::CalendarDateFromFieldsError AsFFI() const;
    inline static icu4x::CalendarDateFromFieldsError FromFFI(icu4x::capi::CalendarDateFromFieldsError c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_CalendarDateFromFieldsError_D_HPP
