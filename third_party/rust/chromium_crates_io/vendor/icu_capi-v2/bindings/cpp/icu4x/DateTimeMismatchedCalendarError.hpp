#ifndef ICU4X_DateTimeMismatchedCalendarError_HPP
#define ICU4X_DateTimeMismatchedCalendarError_HPP

#include "DateTimeMismatchedCalendarError.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CalendarKind.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::DateTimeMismatchedCalendarError icu4x::DateTimeMismatchedCalendarError::AsFFI() const {
    return icu4x::capi::DateTimeMismatchedCalendarError {
        /* .this_kind = */ this_kind.AsFFI(),
        /* .date_kind = */ date_kind.has_value() ? (icu4x::capi::CalendarKind_option{ { date_kind.value().AsFFI() }, true }) : (icu4x::capi::CalendarKind_option{ {}, false }),
    };
}

inline icu4x::DateTimeMismatchedCalendarError icu4x::DateTimeMismatchedCalendarError::FromFFI(icu4x::capi::DateTimeMismatchedCalendarError c_struct) {
    return icu4x::DateTimeMismatchedCalendarError {
        /* .this_kind = */ icu4x::CalendarKind::FromFFI(c_struct.this_kind),
        /* .date_kind = */ c_struct.date_kind.is_ok ? std::optional(icu4x::CalendarKind::FromFFI(c_struct.date_kind.ok)) : std::nullopt,
    };
}


#endif // ICU4X_DateTimeMismatchedCalendarError_HPP
