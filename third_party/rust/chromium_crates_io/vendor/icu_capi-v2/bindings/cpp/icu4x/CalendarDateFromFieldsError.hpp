#ifndef ICU4X_CalendarDateFromFieldsError_HPP
#define ICU4X_CalendarDateFromFieldsError_HPP

#include "CalendarDateFromFieldsError.d.hpp"

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

} // namespace capi
} // namespace

inline icu4x::capi::CalendarDateFromFieldsError icu4x::CalendarDateFromFieldsError::AsFFI() const {
    return static_cast<icu4x::capi::CalendarDateFromFieldsError>(value);
}

inline icu4x::CalendarDateFromFieldsError icu4x::CalendarDateFromFieldsError::FromFFI(icu4x::capi::CalendarDateFromFieldsError c_enum) {
    switch (c_enum) {
        case icu4x::capi::CalendarDateFromFieldsError_Unknown:
        case icu4x::capi::CalendarDateFromFieldsError_InvalidDay:
        case icu4x::capi::CalendarDateFromFieldsError_InvalidOrdinalMonth:
        case icu4x::capi::CalendarDateFromFieldsError_InvalidEra:
        case icu4x::capi::CalendarDateFromFieldsError_MonthCodeInvalidSyntax:
        case icu4x::capi::CalendarDateFromFieldsError_MonthNotInCalendar:
        case icu4x::capi::CalendarDateFromFieldsError_MonthNotInYear:
        case icu4x::capi::CalendarDateFromFieldsError_InconsistentYear:
        case icu4x::capi::CalendarDateFromFieldsError_InconsistentMonth:
        case icu4x::capi::CalendarDateFromFieldsError_NotEnoughFields:
        case icu4x::capi::CalendarDateFromFieldsError_TooManyFields:
        case icu4x::capi::CalendarDateFromFieldsError_Overflow:
            return static_cast<icu4x::CalendarDateFromFieldsError::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_CalendarDateFromFieldsError_HPP
