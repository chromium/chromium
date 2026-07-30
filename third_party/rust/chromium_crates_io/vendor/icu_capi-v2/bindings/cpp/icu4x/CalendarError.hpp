#ifndef ICU4X_CalendarError_HPP
#define ICU4X_CalendarError_HPP

#include "CalendarError.d.hpp"

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

inline icu4x::capi::CalendarError icu4x::CalendarError::AsFFI() const {
    return static_cast<icu4x::capi::CalendarError>(value);
}

inline icu4x::CalendarError icu4x::CalendarError::FromFFI(icu4x::capi::CalendarError c_enum) {
    switch (c_enum) {
        case icu4x::capi::CalendarError_Unknown:
        case icu4x::capi::CalendarError_OutOfRange:
        case icu4x::capi::CalendarError_UnknownEra:
        case icu4x::capi::CalendarError_UnknownMonthCode:
            return static_cast<icu4x::CalendarError::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_CalendarError_HPP
