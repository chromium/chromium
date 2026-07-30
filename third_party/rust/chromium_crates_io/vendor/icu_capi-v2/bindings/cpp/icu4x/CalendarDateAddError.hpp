#ifndef ICU4X_CalendarDateAddError_HPP
#define ICU4X_CalendarDateAddError_HPP

#include "CalendarDateAddError.d.hpp"

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

inline icu4x::capi::CalendarDateAddError icu4x::CalendarDateAddError::AsFFI() const {
    return static_cast<icu4x::capi::CalendarDateAddError>(value);
}

inline icu4x::CalendarDateAddError icu4x::CalendarDateAddError::FromFFI(icu4x::capi::CalendarDateAddError c_enum) {
    switch (c_enum) {
        case icu4x::capi::CalendarDateAddError_Unknown:
        case icu4x::capi::CalendarDateAddError_InvalidDay:
        case icu4x::capi::CalendarDateAddError_MonthNotInYear:
        case icu4x::capi::CalendarDateAddError_Overflow:
            return static_cast<icu4x::CalendarDateAddError::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_CalendarDateAddError_HPP
