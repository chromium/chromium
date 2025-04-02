#ifndef icu4x_DateTimeMismatchedCalendarError_HPP
#define icu4x_DateTimeMismatchedCalendarError_HPP

#include "DateTimeMismatchedCalendarError.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "AnyCalendarKind.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    
    } // extern "C"
} // namespace capi
} // namespace


inline icu4x::capi::DateTimeMismatchedCalendarError icu4x::DateTimeMismatchedCalendarError::AsFFI() const {
  return icu4x::capi::DateTimeMismatchedCalendarError {
    /* .this_kind = */ this_kind.AsFFI(),
    /* .date_kind = */ date_kind.has_value() ? (icu4x::capi::AnyCalendarKind_option{ { date_kind.value().AsFFI() }, true }) : (icu4x::capi::AnyCalendarKind_option{ {}, false }),
  };
}

inline icu4x::DateTimeMismatchedCalendarError icu4x::DateTimeMismatchedCalendarError::FromFFI(icu4x::capi::DateTimeMismatchedCalendarError c_struct) {
  return icu4x::DateTimeMismatchedCalendarError {
    /* .this_kind = */ icu4x::AnyCalendarKind::FromFFI(c_struct.this_kind),
    /* .date_kind = */ c_struct.date_kind.is_ok ? std::optional(icu4x::AnyCalendarKind::FromFFI(c_struct.date_kind.ok)) : std::nullopt,
  };
}


#endif // icu4x_DateTimeMismatchedCalendarError_HPP
